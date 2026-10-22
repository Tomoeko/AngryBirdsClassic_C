#ifndef AB_TEST_WORLD_JOINT_NATIVE_H
#define AB_TEST_WORLD_JOINT_NATIVE_H

#include "joint_constraint_native.h"
#include "joint_state_projection.h"
#include "world_joint_cycle.h"

static size_t joint_with_id(const AbPhysicsWorld *world, uint32_t identifier) {
    for (size_t cursor = 0; cursor < world->joint_count; ++cursor) {
        size_t slot = world->joint_order[cursor];
        if (world->storage.joints[slot].user_data == identifier) {
            return slot;
        }
    }
    return SIZE_MAX;
}

static bool compare_cycle_joints(FILE *file, const AbPhysicsWorld *world,
                                 bool initialized[6], size_t scene, size_t frame,
                                 size_t *states) {
    uint32_t count;
    CHECK(read_words(file, &count, 1) && count == world->joint_count);
    for (size_t cursor = 0; cursor < world->joint_count; ++cursor) {
        const AbWorldSolveJoint *joint =
            world->storage.joints + world->joint_order[cursor];
        uint32_t identifier = (uint32_t)joint->user_data;
        CHECK(identifier >= 1 && identifier <= 6);
        initialized[identifier - 1] |= joint->island;
        uint32_t expected_metadata[9], expected[37], actual[37];
        const uint32_t metadata[] = {
            identifier,
            (uint32_t)joint->constraint.kind,
            (uint32_t)world->storage.bodies[joint->first_body].motion.user_data,
            (uint32_t)world->storage.bodies[joint->second_body].motion.user_data,
            cursor ? (uint32_t)world->storage.joints[world->joint_order[cursor - 1]]
                         .user_data
                   : 0,
            cursor + 1 < world->joint_count
                ? (uint32_t)world->storage.joints[world->joint_order[cursor + 1]]
                      .user_data
                : 0,
            joint->island ? 1u : 0u,
            joint->collide_connected ? 1u : 0u,
            initialized[identifier - 1] ? 1u : 0u};
        CHECK(read_words(file, expected_metadata, 9) &&
              compare_words(metadata, expected_metadata, 9, scene, frame,
                            "joint metadata", cursor) &&
              read_words(file, expected, 37) &&
              project_joint(&joint->constraint, actual));
        if (!initialized[identifier - 1]) {
            joint_state_mask_uninitialized((uint32_t)joint->constraint.kind, actual);
        }
        CHECK(compare_words(actual, expected, 37, scene, frame, "joint state", cursor));
        ++*states;
    }
    return true;
}

static bool read_cycle_operation(FILE *file, AbPhysicsWorld *world, uint32_t scene,
                                 uint32_t frame, bool initialized[6],
                                 size_t counts[12]) {
    uint32_t operation[4], generated[4];
    CHECK(read_words(file, operation, 4));
    world_joint_cycle_operation(frame, generated);
    CHECK(memcmp(operation, generated, sizeof(operation)) == 0 && operation[0] < 12);
    ++counts[operation[0]];
    size_t index = operation[1], body = body_with_id(world, operation[1] + 1);
    AbError error = {0};
    switch (operation[0]) {
        case 0:
            return true;
        case 1:
        case 3: {
            uint32_t input[41], expected[41];
            CHECK(read_words(file, input, 41));
            world_joint_cycle_input(scene, frame, operation, expected);
            AbWorldSolveJoint joint = {.first_body = body_with_id(world, input[1] + 1),
                                       .second_body = body_with_id(world, input[2] + 1),
                                       .collide_connected = ((scene + index) & 1u) != 0,
                                       .user_data = index + 1};
            CHECK(memcmp(input, expected, sizeof(input)) == 0 &&
                  prepare_joint(input, scene * 256 + frame, index, &joint.constraint));
            size_t slot = SIZE_MAX;
            world->locked = operation[0] == 3;
            bool created = ab_world_create_joint(world, &joint, &slot, &error);
            world->locked = false;
            if (created != (operation[0] == 1)) {
                fprintf(stderr,
                        "Joint creation scene %" PRIu32 " frame %" PRIu32
                        " logical slot %zu: %s\n",
                        scene, frame, index, error.message);
                return false;
            }
            if (created) {
                initialized[index] = false;
            } else {
                CHECK(slot == SIZE_MAX && !world->failed);
            }
            return true;
        }
        case 2:
        case 4:
        case 11: {
            size_t slot = joint_with_id(world, operation[1] + 1);
            CHECK(slot != SIZE_MAX);
            if (operation[0] == 11) {
                const AbWorldSolveJoint *joint = world->storage.joints + slot;
                AbBodyMotion *first = &world->storage.bodies[joint->first_body].motion;
                AbBodyMotion *second =
                    &world->storage.bodies[joint->second_body].motion;
                first->awake = false;
                first->sleep_time = 0.25f;
                second->awake = true;
                second->sleep_time = 0.125f;
            }
            world->locked = operation[0] == 4;
            bool removed = ab_world_destroy_joint(world, slot, &error);
            world->locked = false;
            CHECK(removed == (operation[0] != 4) && !world->failed);
            return true;
        }
        case 5:
            return ab_world_destroy_body(world, body, &error);
        case 6: {
            uint32_t words[13];
            CHECK(read_words(file, words, 13));
            AbBodyDefinition definition = read_definition(words);
            size_t slot;
            CHECK(ab_world_create_body(world, &definition, &slot, &error));
            return read_lifecycle_fixture(file, world);
        }
        case 7:
        case 8:
            return ab_world_set_active(world, body, operation[0] == 8, &error);
        case 9:
            world->locked = true;
            CHECK(!ab_world_destroy_body(world, body, &error));
            world->locked = false;
            return !world->failed;
        case 10:
            for (uint32_t identifier = 1; identifier <= 6; ++identifier) {
                size_t slot = joint_with_id(world, identifier);
                if (slot != SIZE_MAX) {
                    CHECK(ab_world_destroy_joint(world, slot, &error));
                }
            }
            return true;
        default:
            return false;
    }
}

static bool native_joint_lifecycle_checks(FILE *file, AbPhysicsWorld *world) {
    size_t events_count[7] = {0}, operation_counts[12] = {0}, joint_states = 0;
    for (uint32_t scene = 0; scene < 16; ++scene) {
        WorldStorage memory;
        CHECK(read_scene(file, &memory, world));
        world->definition.continuous = true;
        bool initialized[6] = {false};
        WorldEvents events = {0};
        world->listener =
            (AbWorldListener){.context = &events,
                              .begin = capture_begin,
                              .end = capture_end,
                              .pre_solve = capture_pre_solve,
                              .post_solve = capture_post_solve,
                              .destroy_fixture = capture_fixture_destruction,
                              .destroy_joint = capture_joint_destruction};
        CHECK(compare_snapshot(file, world, scene, SIZE_MAX, true, true) &&
              compare_events(file, &events, scene, SIZE_MAX, events_count) &&
              compare_cycle_joints(file, world, initialized, scene, SIZE_MAX,
                                   &joint_states));
        for (uint32_t frame = 0; frame < 256; ++frame) {
            events.count = 0;
            CHECK(read_cycle_operation(file, world, scene, frame, initialized,
                                       operation_counts) &&
                  compare_snapshot(file, world, scene, frame, true, true) &&
                  compare_events(file, &events, scene, frame, events_count) &&
                  compare_cycle_joints(file, world, initialized, scene, frame,
                                       &joint_states));
            uint32_t step[3];
            CHECK(read_words(file, step, 3));
            events.count = 0;
            AbError error = {0};
            if (!ab_world_step(world, from_bits(step[0]), step[1], step[2], &error)) {
                fprintf(stderr, "Joint world scene %" PRIu32 " frame %" PRIu32 ": %s\n",
                        scene, frame, error.message);
                return false;
            }
            CHECK(compare_snapshot(file, world, scene, frame, true, true) &&
                  compare_events(file, &events, scene, frame, events_count) &&
                  compare_cycle_joints(file, world, initialized, scene, frame,
                                       &joint_states));
        }
        ab_world_close(world);
    }
    char footer[100];
    CHECK(fgets(footer, sizeof(footer), file) &&
          strcmp(footer, "AB world joint lifecycle reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4,096 native joint lifecycle operations and full world steps match: "
           "%zu joint state snapshots, %zu creations, %zu locked creations, "
           "%zu implicit joint callbacks and %zu implicit fixture callbacks.\n",
           joint_states, operation_counts[1], operation_counts[3], events_count[6],
           events_count[5]);
    return true;
}

#endif
