#include "ab/classic_post_physics.h"
#include "ab/game_object_removal.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Native membership check failed at %d: %s\n", __LINE__,    \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan string(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

#define AB_TEST_POST_NO_OWNER
#include "classic_post_physics.h"

typedef struct {
    AbClassicLevel *level;
    AbGameObjectCore objects[32];
    size_t count;
    bool invalid;
} Lifetime;

static void forget(void *context, AbGameObjectCore *object) {
    Lifetime *lifetime = context;
    size_t index = (size_t)(object - lifetime->objects);
    lifetime->invalid |= index >= lifetime->count ||
                         !ab_classic_level_forget_object(lifetime->level, index, NULL);
}

static bool open_level(const AbZip *archive, AbClassicLevel **output, AbError *error) {
    const char *paths[] = {"assets/data/images/base/INGAME_BLOCKS_1.dat",
                           "assets/data/images/base/INGAME_PIGS_NEW_SHEET_1.dat",
                           "assets/data/images/base/INGAME_BLOCKS_3.dat"};
    AbClassicLevelAssets assets = {.level_path = "assets/data/levels/pack1/Level1.lua",
                                   .catalog_path = "assets/data/scripts/blocks.lua",
                                   .sprite_paths = paths,
                                   .sprite_sheet_count = 3,
                                   .resource_bytes = 16 * 1024 * 1024,
                                   .placement_scale = 1,
                                   .physics_scale = 0.05f,
                                   .derive_ground_position = true,
                                   .derive_world_gravity = true};
    AbWorldDefinition definition = {.broadphase_limits = {128, 1024, 1024},
                                    .maximum_translation = 2,
                                    .maximum_translation_squared = 4,
                                    .warm_start = true,
                                    .clear_forces = true,
                                    .continuous = true};
    return ab_classic_level_load(archive, &assets, definition,
                                 (AbClassicLevelLimits){.objects = 32,
                                                        .contacts = 128,
                                                        .storage_bytes = 1024 * 1024},
                                 true, output, error);
}

static bool compare(AbClassicLevel *level, AbClassicPostPhysics *post, FILE *file,
                    AbStringMap **physical, AbStringMap **teleporters) {
    char marker[96], name[128];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_CLASSIC_POST_REMOVED_V1 1 22\n") &&
          fscanf(file, "%127s\n", name) == 1 && !fseek(file, 0, SEEK_SET));
    AbClassicLevelView view = ab_classic_level_view(level);
    Lifetime lifetime = {.level = level, .count = view.bodies->count};
    CHECK(lifetime.count == 23 && ab_string_map_open(32, 32, 16384, physical, NULL) &&
          ab_string_map_open(0, 0, 4096, teleporters, NULL));
    size_t removed_index = SIZE_MAX;
    for (size_t index = 0; index < lifetime.count; ++index) {
        const AbLevelObject *source = view.bodies->objects + index;
        lifetime.objects[index] = (AbGameObjectCore){
            .name = source->name,
            .body = source->physics.body,
            .collider = (AbGameColliderType)view.frame->objects[index].collider};
        CHECK(ab_string_map_set(*physical, source->name,
                                (uintptr_t)(lifetime.objects + index), true, NULL));
        if (source->name.size == strlen(name) &&
            !memcmp(source->name.data, name, source->name.size)) {
            removed_index = index;
        }
    }
    CHECK(removed_index != SIZE_MAX &&
          lifetime.objects[removed_index].collider == AB_COLLIDER_PIG);
    AbGameObjectCoreRegistry registry = {.world = view.world, .objects = *physical};
    AbGameObjectCoreHost host = {.context = &lifetime, .destroy_object = forget};
    AbGameObjectRemoval removal = {.objects = &registry,
                                   .joints = view.joints,
                                   .teleporters = *teleporters,
                                   .object_host = &host};
    bool removed = false;
    CHECK(ab_game_object_remove(&removal, string(name), &removed, NULL) && removed &&
          !lifetime.invalid && view.removed_objects[removed_index] &&
          view.world->body_count == 22);
    AbGameFrameObject pose = view.frame->objects[removed_index];
    AbClassicMotionProperties properties = view.motion_properties[removed_index];
    CHECK(post_native_checks(level, post, file) &&
          !memcmp(&pose, view.frame->objects + removed_index, sizeof(pose)) &&
          !memcmp(&properties, view.motion_properties + removed_index,
                  sizeof(properties)));
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbClassicLevel *level = NULL;
    AbClassicPostPhysics *post = NULL;
    AbStringMap *physical = NULL, *teleporters = NULL;
    AbError error = {0};
    FILE *file = fopen(argv[2], "r");
    bool okay = file && ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                open_level(&archive, &level, &error) &&
                ab_classic_post_physics_open(
                    level,
                    (AbClassicPostConfig){
                        .minimum_x = -49, .maximum_x = 89, .storage_bytes = 65536},
                    &post, &error) &&
                compare(level, post, file, &physical, &teleporters);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    if (file && fclose(file)) {
        okay = false;
    }
    ab_string_map_close(physical);
    ab_string_map_close(teleporters);
    ab_classic_post_physics_close(post);
    ab_classic_level_close(level);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
