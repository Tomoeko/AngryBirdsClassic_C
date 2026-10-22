#include "ab/classic_scene.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Classic scene failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan string(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

#include "classic_characters.h"
#include "classic_post_physics.h"
#include "classic_render_owner.h"

static bool compare_native(AbClassicScene *scene, AbClassicLevelView level, FILE *file,
                           AbClassicVisuals *visuals) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_CLASSIC_SCENE_V1 22\n") == 0);
    AbClassicSceneView view = ab_classic_scene_view(scene);
    CHECK(view.count == 23 && level.bodies->count == view.count);
    size_t compared = 0;
    uint32_t first_id = 0;
    for (size_t cursor = 0; cursor < view.count; ++cursor) {
        size_t index = view.order[cursor];
        CHECK(index < view.count);
        const AbSceneSprite *item = view.items + index;
        if (!item->region) {
            CHECK(!item->visible && !item->texture);
            continue;
        }
        char object[128], sprite[128];
        int32_t layer;
        uint32_t collider, id, scale_x, scale_y;
        CHECK(fscanf(file,
                     "%127s %" SCNd32 " %" SCNu32 " %" SCNu32 " %" SCNx32 " %" SCNx32
                     " %127s\n",
                     object, &layer, &collider, &id, &scale_x, &scale_y, sprite) == 7);
        AbSpan name = level.bodies->objects[index].name;
        CHECK(name.size == strlen(object) &&
              memcmp(name.data, object, name.size) == 0 && item->layer == layer &&
              item->collider == collider && item->visible &&
              bits(item->pose.scale.x) == scale_x &&
              bits(item->pose.scale.y) == scale_y && id >= index);
        if (!compared) {
            first_id = id - (uint32_t)index;
        }
        CHECK(id - (uint32_t)index == first_id);
        /* Current expression names are observed inputs to the setter; this does
         * not assert the live capture is the original initialization instant. */
        CHECK(ab_classic_visuals_set_sprite(visuals, index, string(sprite), NULL) &&
              ab_classic_scene_sync_visuals(scene, visuals, NULL) &&
              item->region->name.size == strlen(sprite) &&
              memcmp(item->region->name.data, sprite, strlen(sprite)) == 0);
        ++compared;
    }
    CHECK(compared == 22 && fgets(header, sizeof(header), file) &&
          strcmp(header, "AB classic scene complete\n") == 0 && fgetc(file) == EOF &&
          !ferror(file));
    puts("All 22 ordinary native object names, creation/layer order, colliders and "
         "constructor scales match; current captured selections bind.");
    return true;
}

typedef struct {
    size_t count, object;
    AbSpan name;
} VisualNotices;

static void visual_missing(void *context, size_t object, AbSpan name) {
    VisualNotices *notices = context;
    ++notices->count;
    notices->object = object;
    notices->name = name;
}

static bool visual_owner_checks(AbClassicLevel *level, const AbSpriteAtlas *atlas,
                                VisualNotices *notices, AbClassicVisuals **output) {
    AbClassicVisualResources resources = {.atlas = atlas,
                                          .context = notices,
                                          .missing = visual_missing,
                                          .storage_bytes = 16384,
                                          .name_capacity = 128};
    AbClassicVisuals *visuals = NULL;
    CHECK(!ab_classic_visuals_open(NULL, resources, &visuals, NULL) && !visuals &&
          !ab_classic_visuals_open(level, resources, NULL, NULL));
    resources.storage_bytes = 1;
    CHECK(!ab_classic_visuals_open(level, resources, &visuals, NULL) && !visuals);
    resources.storage_bytes = 16384;
    resources.name_capacity = SIZE_MAX;
    CHECK(!ab_classic_visuals_open(level, resources, &visuals, NULL) && !visuals);
    resources.name_capacity = 1;
    CHECK(!ab_classic_visuals_open(level, resources, &visuals, NULL) && !visuals);
    resources.name_capacity = 128;
    CHECK(ab_classic_visuals_open(level, resources, &visuals, NULL));
    size_t bytes = ab_classic_visuals_bytes(visuals);
    ab_classic_visuals_close(visuals);
    visuals = NULL;
    resources.storage_bytes = bytes - 1;
    resources.missing = NULL;
    CHECK(ab_classic_visuals_open(level, resources, &visuals, NULL) &&
          ab_classic_visuals_bytes(visuals) <= resources.storage_bytes);
    AbSpan retained = ab_classic_visuals_object(visuals, 0)->sprite;
    AbSpan expected = ab_classic_level_view(level).initial_sprites[0].sprite.string;
    bool exhausted = false;
    for (size_t index = 0; index <= bytes / sizeof(AbSpan); ++index) {
        char name[128];
        int length = snprintf(name, sizeof(name), "missing-unique-%zu", index);
        CHECK(length > 0 && (size_t)length < sizeof(name));
        AbSpan before = ab_classic_visuals_object(visuals, 0)->sprite;
        if (!ab_classic_visuals_set_sprite(
                visuals, 0, (AbSpan){(const uint8_t *)name, (size_t)length}, NULL)) {
            AbSpan after = ab_classic_visuals_object(visuals, 0)->sprite;
            CHECK(before.data == after.data && before.size == after.size &&
                  ab_classic_visuals_set_sprite(visuals, 0, before, NULL));
            exhausted = true;
            break;
        }
    }
    CHECK(exhausted && retained.size == expected.size &&
          !memcmp(retained.data, expected.data, retained.size));
    ab_classic_visuals_close(visuals);
    visuals = NULL;
    resources.missing = visual_missing;
    resources.storage_bytes = bytes;
    CHECK(ab_classic_visuals_open(level, resources, &visuals, NULL) &&
          ab_classic_visuals_bytes(visuals) == bytes &&
          ab_classic_visuals_bytes(NULL) == 0 &&
          ab_classic_visuals_view(NULL).count == 0 && !notices->count);
    AbClassicLevelView view = ab_classic_level_view(level);
    CHECK(ab_classic_visuals_view(visuals).count == 23);
    for (size_t index = 0; index < 23; ++index) {
        const AbObjectVisual *object = ab_classic_visuals_object(visuals, index);
        AbDataValue initial = view.initial_sprites[index].sprite;
        CHECK(object && initial.kind == AB_DATA_STRING && object->sprite_present &&
              object->sprite.size == initial.string.size &&
              (!object->sprite.size || memcmp(object->sprite.data, initial.string.data,
                                              object->sprite.size) == 0));
        CHECK(!object->sprite.size || object->sprite.data != initial.string.data);
        AbSpriteAtlasBinding binding;
        bool bound = ab_classic_visuals_binding(visuals, index, &binding);
        if (index + 1 == 23) {
            CHECK(!object->attached && !bound && !object->sprite.size);
        } else {
            CHECK(object->attached && bound &&
                  object->attached->sprite == binding.sprite.region &&
                  !object->attached->composite);
        }
    }
    CHECK(!ab_classic_visuals_object(visuals, 23) &&
          !ab_classic_visuals_set_sprite(visuals, 23, string("BIRD_RED"), NULL));
    ab_classic_visuals_close(NULL);
    *output = visuals;
    return true;
}

static bool visual_action_checks(AbClassicScene *scene, AbClassicVisuals *visuals,
                                 VisualNotices *notices) {
    AbClassicSceneView view = ab_classic_scene_view(scene);
    AbSpan original = ab_classic_visuals_object(visuals, 0)->sprite;
    uint8_t original_bytes[128];
    CHECK(original.size <= sizeof(original_bytes));
    memcpy(original_bytes, original.data, original.size);
    original.data = original_bytes;
    CHECK(ab_classic_scene_sync_visuals(scene, visuals, NULL) &&
          !ab_classic_scene_sync_visuals(NULL, visuals, NULL) &&
          !ab_classic_scene_sync_visuals(scene, NULL, NULL));
    char temporary_name[] = "BIRD_RED";
    CHECK(ab_classic_visuals_set_sprite(visuals, 0, string(temporary_name), NULL));
    memset(temporary_name, '?', sizeof(temporary_name));
    const AbObjectVisual *object = ab_classic_visuals_object(visuals, 0);
    CHECK(object->sprite.size == 8 && memcmp(object->sprite.data, "BIRD_RED", 8) == 0 &&
          object->sprite.data == object->attached->name.data &&
          ab_classic_visuals_set_sprite(visuals, 0, object->sprite, NULL));
    AbSpriteAssetState prior = *object->attached;
    CHECK(!ab_classic_visuals_set_sprite(visuals, 0, (AbSpan){NULL, 1}, NULL) &&
          !ab_classic_visuals_set_sprite(visuals, 0, (AbSpan){original_bytes, 129},
                                         NULL) &&
          object->attached->sprite == prior.sprite && object->sprite.size == 8 &&
          memcmp(object->sprite.data, "BIRD_RED", 8) == 0);
    AbDrawCommand commands[24] = {{.texture = 99}};
    AbDrawList list = {commands, 1, 24};
    AbCoordinateCamera camera = {{-1000, -1000}, {0.1f, 0.1f}};
    CHECK(ab_classic_scene_set_visible(scene, 0, false, NULL) &&
          ab_classic_visuals_set_sprite(visuals, 0, string("missing"), NULL) &&
          ab_classic_scene_sync_visuals(scene, visuals, NULL) &&
          view.items[0].unbound && !view.items[0].visible && notices->count == 1 &&
          notices->object == 0 && notices->name.size == 7);
    CHECK(ab_classic_scene_set_visible(scene, 0, true, NULL) &&
          ab_classic_scene_draw(scene, camera, 480, 320, &list, NULL) &&
          list.count == 22 && view.items[0].visible && view.items[0].unbound);
    CHECK(object->sprite.size == 7 && !object->attached->sprite &&
          !object->attached->composite);
    CHECK(ab_classic_visuals_set_sprite(visuals, 0, original, NULL) &&
          ab_classic_scene_sync_visuals(scene, visuals, NULL) &&
          view.items[0].visible && !view.items[0].unbound);
    list.count = 1;
    CHECK(ab_classic_scene_draw(scene, camera, 480, 320, &list, NULL) &&
          list.count == 23);
    CHECK(ab_classic_visuals_set_sprite(visuals, 0, string(""), NULL) &&
          ab_classic_scene_sync_visuals(scene, visuals, NULL) &&
          view.items[0].unbound && object->sprite_present && !object->sprite.size &&
          notices->count == 1);
    CHECK(ab_classic_visuals_set_sprite(visuals, 0, original, NULL) &&
          ab_classic_scene_sync_visuals(scene, visuals, NULL));
    CHECK(ab_classic_visuals_set_sprite(visuals, 22, string("BIRD_RED"), NULL) &&
          !ab_classic_visuals_object(visuals, 22)->attached &&
          ab_classic_scene_sync_visuals(scene, visuals, NULL) &&
          view.items[22].unbound);
    puts("Canonical visual names, cached bindings, empty/missing draw suppression and "
         "visibility restoration pass for all 23 loaded objects.");
    return true;
}

static bool state_and_draw_checks(AbClassicScene *scene, AbClassicLevel *owner,
                                  AbClassicVisuals *visuals) {
    AbClassicLevelView level = ab_classic_level_view(owner);
    AbClassicSceneView view = ab_classic_scene_view(scene);
    const AbSpriteRegion *original = view.items[0].region;
    CHECK(
        original && !ab_classic_scene_set_sprite(scene, 0, string("missing"), NULL) &&
        view.items[0].region == original &&
        !ab_classic_scene_set_sprite(scene, view.count - 1, string("BIRD_RED"), NULL) &&
        !ab_classic_scene_set_sprite(NULL, 0, string("BIRD_RED"), NULL));
    CHECK(!ab_classic_scene_set_scale(scene, view.count, (AbVec2){1, 1}, NULL) &&
          !ab_classic_scene_set_scale(scene, 0, (AbVec2){0, 1}, NULL) &&
          !ab_classic_scene_set_scale(scene, 0, (AbVec2){1, NAN}, NULL) &&
          view.items[0].pose.scale.x == 1 && view.items[0].pose.scale.y == 1);
    CHECK(!ab_classic_scene_set_visible(scene, view.count, false, NULL) &&
          !ab_classic_scene_set_visible(NULL, 0, false, NULL));
    CHECK(ab_classic_scene_set_scale(scene, 0, (AbVec2){-1, 2}, NULL));
    AbDrawCommand commands[24] = {{.texture = 99}};
    AbDrawList list = {commands, 1, 24};
    AbCoordinateCamera camera = {{-1000, -1000}, {0.1f, 0.1f}};
    CHECK(ab_classic_scene_draw(scene, camera, 480, 320, &list, NULL) &&
          list.count == 23 && commands[0].texture == 99 &&
          view.items[0].pose.scale.x == -1 && view.items[0].pose.scale.y == 2);
    size_t command = 1;
    for (size_t cursor = 0; cursor < view.count; ++cursor) {
        size_t index = view.order[cursor];
        const AbSceneSprite *item = view.items + index;
        if (item->region) {
            CHECK(commands[command].texture == item->texture &&
                  commands[command].blend == AB_BLEND_PREMULTIPLIED &&
                  commands[command].coordinates == AB_DRAW_CLIP);
            ++command;
        }
    }
    list.count = 1;
    list.capacity = 2;
    CHECK(!ab_classic_scene_draw(scene, camera, 480, 320, &list, NULL) &&
          list.count == 1 && commands[0].texture == 99);
    list.capacity = 24;
    AbBodyPose saved = level.frame->objects[0].rendered;
    level.frame->objects[0].rendered.position.x += 1;
    level.frame->objects[0].rendered.angle = -0.5f;
    CHECK(ab_classic_scene_draw(scene, camera, 480, 320, &list, NULL) &&
          view.items[0].pose.physics_position.x == saved.position.x + 1 &&
          view.items[0].pose.angle_radians == -0.5f);
    level.frame->objects[0].rendered = saved;
    list.count = 1;
    CHECK(ab_classic_scene_set_visible(scene, 0, false, NULL) &&
          ab_classic_scene_draw(scene, camera, 480, 320, &list, NULL) &&
          list.count == 22);
    CHECK(ab_classic_scene_set_visible(scene, 0, true, NULL) &&
          ab_classic_scene_set_scale(scene, 0, (AbVec2){1, 1}, NULL));
    size_t body = level.frame->objects[0].body;
    level.frame->objects[0].body = SIZE_MAX;
    list.count = 1;
    CHECK(!ab_classic_scene_draw(scene, camera, 480, 320, &list, NULL) &&
          list.count == 1);
    level.frame->objects[0].body = body;
    CHECK(ab_classic_scene_set_visible(scene, 0, false, NULL) &&
          ab_world_destroy_body(level.world, body, NULL));
    CHECK(ab_classic_scene_draw(scene, camera, 480, 320, &list, NULL) &&
          list.count == 22);
    CHECK(ab_classic_scene_set_visible(scene, 0, true, NULL));
    list.count = 1;
    CHECK(!ab_classic_scene_draw(scene, camera, 480, 320, &list, NULL) &&
          list.count == 1 &&
          !ab_classic_scene_draw(NULL, camera, 480, 320, &list, NULL));
    CHECK(ab_classic_level_forget_object(owner, 0, NULL) &&
          ab_classic_scene_sync_visuals(scene, visuals, NULL) &&
          ab_classic_scene_draw(scene, camera, 480, 320, &list, NULL) &&
          list.count == 22 && !view.items[0].visible &&
          !ab_classic_scene_set_visible(scene, 0, true, NULL));
    level.world->locked = true;
    CHECK(ab_classic_level_forget_object(owner, 1, NULL));
    level.world->locked = false;
    list.count = 1;
    CHECK(ab_world_has_body(level.world, level.frame->objects[1].body) &&
          ab_classic_scene_draw(scene, camera, 480, 320, &list, NULL) &&
          list.count == 21 && !view.items[1].visible);
    puts("Live pose/sprite/scale/visibility updates, hidden removals and draw rollback "
         "pass.");
    return true;
}

static bool open_level(const AbZip *archive, void *sprite_context,
                       bool (*sprite_size)(void *, AbSpan, AbVec2 *, AbError *),
                       AbClassicLevel **output, AbError *error) {
    const char *paths[] = {"assets/data/images/base/INGAME_BLOCKS_1.dat",
                           "assets/data/images/base/INGAME_PIGS_NEW_SHEET_1.dat",
                           "assets/data/images/base/INGAME_BLOCKS_3.dat"};
    AbClassicLevelAssets assets = {.level_path = "assets/data/levels/pack1/Level1.lua",
                                   .catalog_path = "assets/data/scripts/blocks.lua",
                                   .sprite_paths = sprite_size ? NULL : paths,
                                   .sprite_sheet_count = sprite_size ? 0 : 3,
                                   .resource_bytes = 16 * 1024 * 1024,
                                   .placement_scale = 1,
                                   .physics_scale = 0.05f,
                                   .derive_ground_position = true,
                                   .derive_world_gravity = true,
                                   .sprite_context = sprite_context,
                                   .sprite_size = sprite_size};
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

static bool owner_checks(AbClassicLevel *level, const AbSpriteAtlas *atlas,
                         AbClassicScene **output) {
    AbClassicScene *scene = NULL;
    CHECK(!ab_classic_scene_open(NULL, atlas, 7, AB_BLEND_PREMULTIPLIED, 16384, &scene,
                                 NULL) &&
          !scene &&
          !ab_classic_scene_open(level, NULL, 7, AB_BLEND_PREMULTIPLIED, 16384, &scene,
                                 NULL) &&
          !scene &&
          !ab_classic_scene_open(level, atlas, 0, AB_BLEND_PREMULTIPLIED, 16384, &scene,
                                 NULL) &&
          !scene &&
          !ab_classic_scene_open(level, atlas, UINT32_MAX, AB_BLEND_PREMULTIPLIED,
                                 16384, &scene, NULL) &&
          !scene &&
          !ab_classic_scene_open(level, atlas, 7, (AbBlend)99, 16384, &scene, NULL) &&
          !scene &&
          !ab_classic_scene_open(level, atlas, 7, AB_BLEND_PREMULTIPLIED, 1, &scene,
                                 NULL) &&
          !scene &&
          !ab_classic_scene_open(level, atlas, 7, AB_BLEND_PREMULTIPLIED, 16384, NULL,
                                 NULL));
    CHECK(ab_classic_scene_open(level, atlas, 7, AB_BLEND_PREMULTIPLIED, 16384, &scene,
                                NULL));
    size_t bytes = ab_classic_scene_bytes(scene);
    ab_classic_scene_close(scene);
    scene = NULL;
    CHECK(!ab_classic_scene_open(level, atlas, 7, AB_BLEND_PREMULTIPLIED, bytes - 1,
                                 &scene, NULL) &&
          !scene);
    CHECK(ab_classic_scene_open(level, atlas, 7, AB_BLEND_PREMULTIPLIED, bytes, &scene,
                                NULL) &&
          ab_classic_scene_bytes(scene) == bytes && ab_classic_scene_bytes(NULL) == 0 &&
          ab_classic_scene_view(NULL).count == 0);
    ab_classic_scene_close(NULL);
    *output = scene;
    return true;
}

#include "classic_resource_scene.h"

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr,
                "Usage: ab_classic_scene_tests APK NATIVE_SCENE [NATIVE_POST]\n");
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbClassicLevel *level = NULL;
    AbSpriteAtlas *atlas = NULL;
    AbClassicScene *scene = NULL;
    AbClassicVisuals *visuals = NULL;
    AbClassicCharacters *characters = NULL;
    AbClassicPostPhysics *post = NULL;
    VisualNotices notices = {0};
    AbError error = {0};
    FILE *file = fopen(argv[2], "rb");
    FILE *post_file = argc == 4 ? fopen(argv[3], "rb") : NULL;
    bool okay = file && (argc == 3 || post_file) &&
                ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                open_level(&archive, NULL, NULL, &level, &error);
    AbClassicLevelView view = ab_classic_level_view(level);
    okay = okay &&
           ab_sprite_atlas_open(&archive, view.sprites, 16 * 1024 * 1024,
                                8 * 1024 * 1024, &atlas, &error) &&
           render_owner_checks(level, atlas) &&
           resource_scene_checks(&archive, level, atlas, file, &error) &&
           owner_checks(level, atlas, &scene) &&
           visual_owner_checks(level, atlas, &notices, &visuals) &&
           compare_native(scene, view, file, visuals) &&
           visual_action_checks(scene, visuals, &notices) &&
           character_owner_checks(level, visuals, scene, &characters) &&
           post_owner_checks(level, &post) &&
           (!post_file || post_native_checks(level, post, post_file)) &&
           state_and_draw_checks(scene, level, visuals);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    if (file && fclose(file) != 0) {
        okay = false;
    }
    if (post_file && fclose(post_file) != 0) {
        okay = false;
    }
    ab_classic_post_physics_close(post);
    ab_classic_characters_close(characters);
    ab_classic_scene_close(scene);
    ab_classic_visuals_close(visuals);
    ab_sprite_atlas_close(atlas);
    ab_classic_level_close(level);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
