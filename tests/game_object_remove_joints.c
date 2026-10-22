#include "ab/game_object_removal.h"
#include "game_object_remove_joints_cases.h"
#include "world_storage.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Removal joint check failed at %d: %s\n", __LINE__,        \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    WorldStorage storage;
    AbPhysicsWorld world;
    AbGameObjectCore objects[3];
    AbGameObjectCoreRegistry registry;
    AbClassicJointRecord records[4], glue[4];
    AbClassicJoints joints;
    AbGameObjectRemoval removal;
} Fixture;

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static float number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, output + index) == 1);
    }
    return fgetc(file) == '\n';
}

static bool prepare(Fixture *fixture, const uint32_t header[4], uint32_t rows[4][4]) {
    memset(fixture, 0, sizeof(*fixture));
    CHECK(ab_world_open(&fixture->world, default_definition(),
                        prepare_storage(&fixture->storage, 0, 0), NULL));
    fixture->registry.world = &fixture->world;
    CHECK(ab_string_map_open(4, 4, 4096, &fixture->registry.objects, NULL));
    fixture->removal.objects = &fixture->registry;
    fixture->removal.joints = &fixture->joints;
    CHECK(ab_string_map_open(0, 0, 4096, &fixture->removal.teleporters, NULL));
    for (uint32_t index = 0; index < 3; ++index) {
        AbGameObjectCore *object = fixture->objects + index;
        object->name = text(removal_object_names[index]);
        object->delayed_destroy = ((header[0] >> index) & 1u) != 0;
        object->destroy_timer = number(UINT32_C(0x3e000000) + index * 0x800000u);
        const AbBodyDefinition definition = {
            .type = AB_BODY_DYNAMIC, .awake = true, .active = true, .gravity_scale = 1};
        CHECK(ab_world_create_body(&fixture->world, &definition, &object->body, NULL));
        CHECK(ab_string_map_set(fixture->registry.objects, object->name,
                                (uintptr_t)object, true, NULL));
    }
    CHECK(ab_classic_joints_open(
        &fixture->joints, &fixture->world,
        (AbClassicJointStorage){fixture->records, 4, fixture->glue, 4}, NULL));
    for (uint32_t index = 0; index < 4; ++index) {
        AbClassicJoint definition = {
            .type = (AbClassicJointType)rows[index][0],
            .name = text(removal_joint_names[index]),
            .endpoints = {text(removal_object_names[rows[index][1]]),
                          text(removal_object_names[rows[index][2]])},
            .destroy_timer = number(rows[index][3]),
            .joint = {.first_body = rows[index][1],
                      .second_body = rows[index][2],
                      .constraint.kind = (AbJointKind)(rows[index][0] - 1u),
                      .user_data = index + 1u}};
        fixture->world.locked = (header[3] & (1u << index)) != 0;
        size_t slot;
        CHECK(ab_classic_joints_create(&fixture->joints, &definition, &slot, NULL));
    }
    fixture->world.locked = header[2] != 0;
    return true;
}

static void close_fixture(Fixture *fixture) {
    fixture->world.locked = false;
    ab_string_map_close(fixture->registry.objects);
    ab_string_map_close(fixture->removal.teleporters);
    ab_world_close(&fixture->world);
}

static uint32_t identifier(AbSpan name) {
    for (uint32_t index = 0; index < 4; ++index) {
        AbSpan expected = text(removal_joint_names[index]);
        if (name.size == expected.size &&
            !memcmp(name.data, expected.data, name.size)) {
            return index + 1u;
        }
    }
    return 0;
}

static bool snapshot(FILE *file, Fixture *fixture, uint32_t sample, uint32_t pass) {
    AbStringMapLayout layout;
    CHECK(ab_string_map_layout(fixture->registry.objects, &layout));
    uint32_t output[25] = {(uint32_t)layout.count,
                           (uint32_t)fixture->world.body_count,
                           (uint32_t)fixture->world.joint_count,
                           (uint32_t)fixture->joints.joint_count,
                           (uint32_t)fixture->joints.glue_count,
                           0,
                           fixture->world.locked};
    for (uint32_t index = 0; index < 4; ++index) {
        output[5] |= ab_classic_joints_metadata(&fixture->joints,
                                                text(removal_joint_names[index]))
                         ? 1u << index
                         : 0u;
    }
    for (uint32_t index = 0; index < 3; ++index) {
        uintptr_t handle = 0;
        bool present = false;
        CHECK(ab_string_map_get(fixture->registry.objects,
                                text(removal_object_names[index]), &handle, &present,
                                NULL));
        if (present) {
            const AbGameObjectCore *object = (const AbGameObjectCore *)handle;
            output[7 + index * 3] = 1;
            output[8 + index * 3] = object->delayed_destroy;
            output[9 + index * 3] = bits(object->destroy_timer);
        }
    }
    for (size_t index = 0; index < fixture->joints.joint_count; ++index) {
        output[16 + index] = identifier(fixture->records[index].definition.name);
    }
    for (size_t index = 0; index < fixture->joints.glue_count; ++index) {
        output[20 + index] = identifier(fixture->glue[index].definition.name);
    }
    for (size_t index = 0; index < fixture->world.joint_count; ++index) {
        const AbWorldSolveJoint *joint =
            fixture->storage.joints + fixture->world.joint_order[index];
        CHECK(joint->user_data && joint->user_data <= 4);
        output[24] |= 1u << (joint->user_data - 1u);
    }
    uint32_t expected[25];
    CHECK(read_words(file, expected, 25));
    for (size_t index = 0; index < 25; ++index) {
        if (output[index] != expected[index]) {
            fprintf(stderr,
                    "Removal joint %" PRIu32 " pass %" PRIu32 " field %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    sample, pass, index, output[index], expected[index]);
            return false;
        }
    }
    return true;
}

static bool native_case(FILE *file, uint32_t sample) {
    uint32_t header[4], rows[4][4], expected_header[4], expected_rows[4][4];
    game_object_remove_joints_case(sample, expected_header, expected_rows);
    CHECK(read_words(file, header, 4) &&
          !memcmp(header, expected_header, sizeof(header)));
    for (size_t index = 0; index < 4; ++index) {
        CHECK(read_words(file, rows[index], 4) &&
              !memcmp(rows[index], expected_rows[index], sizeof(rows[index])));
    }
    Fixture fixture;
    bool okay = prepare(&fixture, header, rows) && snapshot(file, &fixture, sample, 0);
    for (uint32_t pass = 0; okay && pass < 3; ++pass) {
        uint32_t requested = pass < 2 ? header[1] : (header[1] + 1u) % 4u;
        AbSpan name = text(removal_object_names[requested]);
        bool removed = false;
        okay = ab_game_object_remove(&fixture.removal, name, &removed, NULL) &&
               snapshot(file, &fixture, sample, pass + 1u);
    }
    close_fixture(&fixture);
    return okay;
}

static bool native_checks(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_GAME_OBJECT_REMOVE_JOINTS_V1 512\n"));
    for (uint32_t sample = 0; sample < 512; ++sample) {
        CHECK(native_case(file, sample));
    }
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB game object removal joints reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched 61,440 native object/joint removal words: ordered records, "
         "directional glue timers, locked worlds, null engine slots and repeats");
    return true;
}

static bool edges(void) {
    uint32_t header[4] = {0}, rows[4][4] = {{5, 0, 1, 0x3f800000},
                                            {5, 0, 1, 0x40000000},
                                            {5, 1, 2, 0x40400000},
                                            {2, 1, 2, 0}};
    Fixture fixture;
    CHECK(prepare(&fixture, header, rows));
    /* The second deferred record publishes the shared name's metadata. The
     * first consumes and clears it. The second marks its endpoint before the
     * missing-table error, preserving its record and every immediate record. */
    fixture.glue[0].definition.name = text(removal_joint_names[1]);
    fixture.glue[0].metadata_visible = false;
    CHECK(!ab_classic_joints_forget_object(&fixture.joints, &fixture.registry,
                                           text("first"), NULL));
    CHECK(fixture.joints.glue_count == 2 && fixture.joints.joint_count == 1 &&
          fixture.objects[1].delayed_destroy && fixture.objects[1].destroy_timer == 2 &&
          !ab_classic_joints_metadata(&fixture.joints, text(removal_joint_names[1])) &&
          fixture.world.joint_count == 1 && fixture.world.body_count == 3);
    /* An absent second physical endpoint avoids the missing metadata lookup. */
    CHECK(ab_string_map_set(fixture.registry.objects, text("second"), 0, false, NULL));
    CHECK(ab_classic_joints_forget_object(&fixture.joints, &fixture.registry,
                                          text("first"), NULL));
    CHECK(fixture.joints.glue_count == 1 && fixture.joints.joint_count == 1);
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
