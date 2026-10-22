#include "ab/game_object_removal.h"
#include "game_object_remove_teleporters_cases.h"
#include "world_storage.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Removal teleporter check failed at %d: %s\n", __LINE__,   \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static const char *const names[] = {"first", "second", "third", "missing"};

typedef struct {
    WorldStorage storage;
    AbPhysicsWorld world;
    AbGameObjectCore objects[3];
    AbGameObjectCoreRegistry registry;
    AbGameTeleporter teleporters[3];
    AbClassicJointRecord records[1], glue[1];
    AbClassicJoints joints;
    AbGameObjectRemoval removal;
    unsigned destroyed;
    bool invalid;
} Fixture;

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, output + index) == 1);
    }
    return fgetc(file) == '\n';
}

static void destroy_teleporter(void *context, AbGameTeleporter *teleporter) {
    Fixture *fixture = context;
    size_t index = (size_t)(teleporter - fixture->teleporters);
    if (index >= 3) {
        fixture->invalid = true;
        return;
    }
    uintptr_t handle = 0;
    bool present = true;
    fixture->invalid |=
        !ab_string_map_get(fixture->registry.objects, text(names[index]), &handle,
                           &present, NULL) ||
        present;
    fixture->invalid |=
        !ab_string_map_get(fixture->removal.teleporters, text(names[index]), &handle,
                           &present, NULL) ||
        !present || handle != (uintptr_t)teleporter;
    fixture->destroyed |= 1u << index;
}

static bool prepare(Fixture *fixture, const uint32_t input[5]) {
    memset(fixture, 0, sizeof(*fixture));
    CHECK(ab_world_open(&fixture->world, default_definition(),
                        prepare_storage(&fixture->storage, 0, 0), NULL));
    fixture->registry.world = &fixture->world;
    CHECK(ab_string_map_open(4, 4, 4096, &fixture->registry.objects, NULL));
    fixture->removal = (AbGameObjectRemoval){.objects = &fixture->registry,
                                             .joints = &fixture->joints,
                                             .teleporter_context = fixture,
                                             .destroy_teleporter = destroy_teleporter};
    CHECK(ab_string_map_open(4, 4, 4096, &fixture->removal.teleporters, NULL));
    CHECK(ab_classic_joints_open(
        &fixture->joints, &fixture->world,
        (AbClassicJointStorage){fixture->records, 1, fixture->glue, 1}, NULL));
    for (uint32_t index = 0; index < 3; ++index) {
        AbGameObjectCore *object = fixture->objects + index;
        object->name = text(names[index]);
        const AbBodyDefinition definition = {
            .type = AB_BODY_DYNAMIC, .awake = true, .active = true, .gravity_scale = 1};
        CHECK(ab_world_create_body(&fixture->world, &definition, &object->body, NULL));
        CHECK(ab_string_map_set(fixture->registry.objects, object->name,
                                (uintptr_t)object, true, NULL));
        uint32_t kind = (input[1] >> (index * 2u)) & 3u;
        if (kind) {
            fixture->teleporters[index] =
                (AbGameTeleporter){.kind = kind == 1 ? AB_GAME_TELEPORTER_SIMPLE
                                                     : AB_GAME_TELEPORTER_PORTAL,
                                   .keep_on_object_removal = kind != 2};
            CHECK(ab_string_map_set(fixture->removal.teleporters, object->name,
                                    (uintptr_t)(fixture->teleporters + index), true,
                                    NULL));
        }
    }
    fixture->world.locked = input[3] != 0;
    return true;
}

static void close_fixture(Fixture *fixture) {
    fixture->world.locked = false;
    ab_string_map_close(fixture->registry.objects);
    ab_string_map_close(fixture->removal.teleporters);
    ab_world_close(&fixture->world);
}

static bool snapshot(FILE *file, Fixture *fixture, uint32_t sample, uint32_t pass) {
    AbStringMapLayout layout;
    CHECK(ab_string_map_layout(fixture->registry.objects, &layout));
    uint32_t output[7] = {
        (uint32_t)layout.count, (uint32_t)fixture->world.body_count, 0, 0, 0, 0,
        fixture->world.locked};
    CHECK(ab_string_map_layout(fixture->removal.teleporters, &layout));
    output[3] = (uint32_t)layout.count;
    for (uint32_t index = 0; index < 3; ++index) {
        uintptr_t handle = 0;
        bool present = false;
        CHECK(ab_string_map_get(fixture->registry.objects, text(names[index]), &handle,
                                &present, NULL));
        output[2] |= present ? 1u << index : 0u;
        CHECK(ab_string_map_get(fixture->removal.teleporters, text(names[index]),
                                &handle, &present, NULL));
        if (present) {
            const AbGameTeleporter *teleporter = (const AbGameTeleporter *)handle;
            output[4] |= 1u << index;
            output[5] |= teleporter->kind == AB_GAME_TELEPORTER_SIMPLE ||
                                 !teleporter->keep_on_object_removal
                             ? 1u << index
                             : 0u;
        }
    }
    CHECK(!fixture->invalid);
    uint32_t expected[7];
    CHECK(read_words(file, expected, 7));
    for (size_t index = 0; index < 7; ++index) {
        if (output[index] != expected[index]) {
            fprintf(stderr,
                    "Removal teleporter %" PRIu32 " pass %" PRIu32
                    " field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, pass, index, output[index], expected[index]);
            return false;
        }
    }
    return true;
}

static bool native_case(FILE *file, uint32_t sample) {
    uint32_t input[5], generated[5];
    game_object_remove_teleporters_case(sample, generated);
    CHECK(read_words(file, input, 5) && !memcmp(input, generated, sizeof(input)));
    Fixture fixture;
    bool okay = prepare(&fixture, input) && snapshot(file, &fixture, sample, 0);
    for (uint32_t pass = 0; okay && pass < 3; ++pass) {
        uint32_t requested = pass < 2 ? input[2] : (input[2] + 1u) % 4u;
        if (pass == 1 && input[4] && requested < 3 &&
            ((input[1] >> (requested * 2u)) & 3u) == 3) {
            fixture.teleporters[requested].keep_on_object_removal = false;
        }
        bool removed = false;
        okay = ab_game_object_remove(&fixture.removal, text(names[requested]), &removed,
                                     NULL) &&
               snapshot(file, &fixture, sample, pass + 1u);
    }
    close_fixture(&fixture);
    return okay;
}

static bool native_checks(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_GAME_OBJECT_REMOVE_TELEPORTERS_V1 1024\n"));
    for (uint32_t sample = 0; sample < 1024; ++sample) {
        CHECK(native_case(file, sample));
    }
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB game object removal teleporters reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched 33,792 native object/teleporter words: base/portal retention, "
         "missing physical keys, mutable retention, repeated removal and locks");
    return true;
}

static bool edges(void) {
    const uint32_t input[5] = {0, 1, 0, 0, 0};
    Fixture fixture;
    CHECK(prepare(&fixture, input));
    AbClassicJoint joint = {.type = AB_CLASSIC_JOINT_GLUE,
                            .name = text("glue"),
                            .endpoints = {text("first"), text("second")},
                            .destroy_timer = 0.75f};
    size_t slot;
    CHECK(ab_classic_joints_create(&fixture.joints, &joint, &slot, NULL));
    /* Missing glue metadata fails before teleporter cleanup, but after physical
     * removal and the second endpoint's delayed-destruction flag. */
    fixture.glue[0].metadata_visible = false;
    bool removed = true;
    CHECK(!ab_game_object_remove(&fixture.removal, text("first"), &removed, NULL));
    CHECK(removed && fixture.destroyed == 0 && fixture.joints.glue_count == 1 &&
          fixture.world.body_count == 2 && fixture.objects[1].delayed_destroy);
    fixture.glue[0].metadata_visible = true;
    CHECK(ab_game_object_remove(&fixture.removal, text("first"), &removed, NULL));
    CHECK(!removed && fixture.destroyed == 1 && !fixture.invalid &&
          fixture.joints.glue_count == 0 && fixture.objects[1].destroy_timer == 0.75f);
    /* Unknown teleporter metadata is rejected after physical deletion. */
    fixture.teleporters[2].kind = (AbGameTeleporterKind)9;
    CHECK(ab_string_map_set(fixture.removal.teleporters, text("third"),
                            (uintptr_t)(fixture.teleporters + 2), true, NULL));
    removed = true;
    CHECK(!ab_game_object_remove(&fixture.removal, text("third"), &removed, NULL));
    CHECK(removed && fixture.world.body_count == 1 && fixture.destroyed == 1);
    close_fixture(&fixture);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        return 2;
    }
    FILE *file = argc == 2 ? fopen(argv[1], "r") : NULL;
    if (argc == 2 && !file) {
        return 1;
    }
    bool okay = file ? native_checks(file) : edges();
    if (file && fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
