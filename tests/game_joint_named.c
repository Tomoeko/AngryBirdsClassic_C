#include "ab/classic_joints.h"

#include "game_joint_named_cases.h"
#include "world_storage.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Named joint check failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static uint32_t name_id(AbSpan name) {
    for (uint32_t index = 0; index < 5; ++index) {
        if (name.size == strlen(joint_named_names[index]) &&
            (!name.size ||
             memcmp(name.data, joint_named_names[index], name.size) == 0)) {
            return index;
        }
    }
    return UINT32_MAX;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    return fgetc(file) == '\n';
}

static bool snapshot(FILE *file, const AbClassicJoints *joints, uint32_t sample,
                     uint32_t pass) {
    const AbPhysicsWorld *world = joints->world;
    uint32_t mask = 0;
    for (uint32_t index = 0; index < 4; ++index) {
        mask |= ab_classic_joints_metadata(joints, text(joint_named_names[index]))
                    ? 1u << index
                    : 0;
    }
    uint32_t actual[25] = {(uint32_t)world->joint_count, (uint32_t)joints->joint_count,
                           (uint32_t)joints->glue_count, mask, world->locked};
    for (size_t index = 5; index < 21; ++index) {
        actual[index] = UINT32_MAX;
    }
    for (size_t index = 0; index < world->joint_count; ++index) {
        actual[5 + index] =
            (uint32_t)(uintptr_t)world->storage.joints[world->joint_order[index]]
                .user_data;
    }
    for (size_t index = 0; index < joints->joint_count; ++index) {
        const AbClassicJointRecord *record = joints->storage.joints + index;
        actual[9 + index * 2] = name_id(record->definition.name);
        actual[10 + index * 2] =
            record->world_joint == SIZE_MAX
                ? 0
                : (uint32_t)(uintptr_t)world->storage.joints[record->world_joint]
                      .user_data;
    }
    for (size_t index = 0; index < joints->glue_count; ++index) {
        actual[17 + index] = name_id(joints->storage.glue[index].definition.name);
    }
    for (size_t index = 0; index < 2; ++index) {
        const AbBodyMotion *body = &world->storage.bodies[index].motion;
        actual[21 + index * 2] = body->awake;
        actual[22 + index * 2] = bits(body->sleep_time);
    }
    uint32_t expected[25];
    CHECK(read_words(file, expected, 5) && read_words(file, expected + 5, 16) &&
          read_words(file, expected + 21, 4));
    for (size_t field = 0; field < 25; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Native named joint %" PRIu32 " pass %" PRIu32 " field %zu: "
                    "%08" PRIx32 " != %08" PRIx32 "\n",
                    sample, pass, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool native_case(FILE *file, uint32_t sample, AbPhysicsWorld *world,
                        WorldStorage *memory) {
    uint32_t header[7], rows[4][2], generated[7], generated_rows[4][2];
    game_joint_named_case(sample, generated, generated_rows);
    CHECK(read_words(file, header, 7) &&
          memcmp(header, generated, sizeof(header)) == 0);
    for (size_t index = 0; index < 4; ++index) {
        CHECK(read_words(file, rows[index], 2) &&
              memcmp(rows[index], generated_rows[index], sizeof(rows[index])) == 0);
    }
    AbWorldStorage storage = prepare_storage(memory, 2, 0);
    const AbBodyDefinition body = {.type = AB_BODY_DYNAMIC, .active = true};
    for (size_t index = 0; index < 2; ++index) {
        CHECK(ab_body_initialize(&body, &memory->bodies[index].motion));
    }
    CHECK(ab_world_open(world, default_definition(), storage, NULL));
    AbClassicJointRecord records[4], glue[4];
    AbClassicJoints joints;
    CHECK(ab_classic_joints_open(&joints, world,
                                 (AbClassicJointStorage){records, 4, glue, 4}, NULL));
    for (size_t index = 0; index < 4; ++index) {
        AbClassicJoint definition = {.type = (AbClassicJointType)rows[index][0],
                                     .name = text(joint_named_names[rows[index][1]]),
                                     .endpoints = {text("first"), text("second")},
                                     .destroy_timer = 1,
                                     .joint = {.first_body = 0,
                                               .second_body = 1,
                                               .constraint.kind = AB_JOINT_WELD,
                                               .user_data = index + 1}};
        world->locked = (header[2] & (1u << index)) != 0;
        size_t slot;
        CHECK(ab_classic_joints_create(&joints, &definition, &slot, NULL));
    }
    for (size_t index = 0; index < 2; ++index) {
        memory->bodies[index].motion.awake = ((sample >> (4 + index)) & 1u) != 0;
        memory->bodies[index].motion.sleep_time = index ? 0.125f : 0.25f;
    }
    world->locked = header[1] != 0;
    CHECK(snapshot(file, &joints, sample, 0));
    for (uint32_t request = 0; request < 4; ++request) {
        CHECK(ab_classic_joints_remove_named(
            &joints, text(joint_named_names[header[3 + request]]), NULL));
        CHECK(snapshot(file, &joints, sample, request + 1));
    }
    world->locked = false;
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_GAME_JOINT_NAMED_V1 4096\n") == 0);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        WorldStorage memory;
        AbPhysicsWorld world = {0};
        bool okay = native_case(file, sample, &world, &memory);
        ab_world_close(&world);
        if (!okay) {
            return false;
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB game joint named reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched 16,384 original named removals: immediate-before-deferred lookup, "
         "duplicate/null records, ordered erasure, metadata clearing, engine lists "
         "and conditional body wake state");
    return true;
}

static bool edge_case(AbPhysicsWorld *world, WorldStorage *memory) {
    CHECK(ab_world_open(world, default_definition(), prepare_storage(memory, 0, 0),
                        NULL));
    AbClassicJointRecord records[2];
    AbClassicJoints joints;
    CHECK(ab_classic_joints_open(&joints, world,
                                 (AbClassicJointStorage){records, 2, NULL, 0}, NULL));
    world->locked = true;
    for (size_t index = 0; index < 2; ++index) {
        AbClassicJoint definition = {.type = AB_CLASSIC_JOINT_WELD,
                                     .name = text(joint_named_names[index]),
                                     .joint.constraint.kind = AB_JOINT_WELD};
        size_t slot;
        CHECK(ab_classic_joints_create(&joints, &definition, &slot, NULL) &&
              slot == SIZE_MAX);
    }
    CHECK(ab_classic_joints_remove_named(&joints, text("joint2"), NULL) &&
          joints.joint_count == 1 && name_id(records[0].definition.name) == 0 &&
          records[0].metadata_visible);
    world->locked = false;
    CHECK(!ab_classic_joints_remove_named(&joints, text("joint"), NULL) &&
          joints.joint_count == 1 && records[0].metadata_visible && !joints.failed);
    CHECK(ab_classic_joints_remove_named(&joints, text("absent"), NULL));
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
    bool okay;
    if (file) {
        okay = native_checks(file);
    } else {
        WorldStorage memory;
        AbPhysicsWorld world = {0};
        okay = edge_case(&world, &memory);
        ab_world_close(&world);
    }
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
