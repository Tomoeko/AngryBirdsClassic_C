#ifndef AB_TEST_LEVEL_FRAMES_H
#define AB_TEST_LEVEL_FRAMES_H

#include "ab/game_frame.h"
#include "ab/classic_simulation.h"
#include "ab/rotation.h"

#include "frame_phases.h"
#include "game_physics_prepare.h"

typedef struct {
    size_t body;
    size_t calls;
} FrameStepCheck;

static bool frame_after_step_check(void *context, AbPhysicsWorld *world,
                                   AbError *error) {
    FrameStepCheck *check = context;
    ++check->calls;
    return ab_world_set_transform(world, check->body, (AbVec2){7, 4}, 0, error);
}

static bool game_frame_edge_checks(void) {
    CHECK(frame_phase_edge_checks());
    CHECK(physics_prepare_edge_checks());
    LevelStepStorage memory;
    AbPhysicsWorld world = {0};
    AbError error = {0};
    const uint32_t settings[] = {0, 0, UINT32_C(0x3d088889), 8, 3, 0, 1, 1};
    CHECK(open_level_step_world(&memory, &world, settings, &error));
    AbBodyDefinition definition = {.type = AB_BODY_DYNAMIC,
                                   .position = {1, 2},
                                   .linear_velocity = {1, 0},
                                   .gravity_scale = 1,
                                   .awake = true,
                                   .active = true};
    size_t body;
    CHECK(ab_world_create_body(&world, &definition, &body, &error));
    AbGameFrameObject object;
    CHECK(ab_game_frame_object_reset(&world, body, 3, &object, &error));
    CHECK(object.history[0].position.x == 1 && object.history[1].position.y == 2);
    AbGameFrame frame = {.objects = &object, .count = 1};
    FrameStepCheck check = {body, 0};
    AbGameFrameControls controls = {.velocity_iterations = 8,
                                    .position_iterations = 3,
                                    .maximum_steps = 8,
                                    .context = &check,
                                    .after_step = frame_after_step_check};
    AbFrameClockInput timing = {.seconds = 0.05f, .physics_enabled = true};
    AbFrameClockPlan plan;
    CHECK(ab_game_frame_advance(&frame, &world, &timing, &controls, &plan, &error));
    CHECK(plan.steps == 1 && check.calls == 1 && frame.history_index == 1);
    CHECK(object.history[1].position.x == 7 && object.history[1].position.y == 4 &&
          object.rendered.position.x > 1 && object.rendered.position.x < 7);
    AbBodyPose rendered = object.rendered;
    float accumulator = frame.clock.accumulator;
    timing.physics_enabled = false;
    timing.seconds = 0.1f;
    CHECK(ab_game_frame_advance(&frame, &world, &timing, &controls, &plan, &error));
    CHECK(plan.steps == 0 && check.calls == 1 && frame.history_index == 1 &&
          number(frame.clock.accumulator).bits == number(accumulator).bits &&
          object.rendered.position.x == rendered.position.x);
    AbFrameClock clock = frame.clock;
    timing.physics_enabled = true;
    controls.maximum_steps = 0;
    CHECK(!ab_game_frame_advance(&frame, &world, &timing, &controls, &plan, &error));
    CHECK(check.calls == 1 && frame.clock.time == clock.time &&
          frame.clock.accumulator == clock.accumulator);
    controls.maximum_steps = 8;
    object.body = world.storage.body_capacity;
    CHECK(!ab_game_frame_advance(&frame, &world, &timing, &controls, &plan, &error));
    CHECK(check.calls == 1 && frame.clock.time == clock.time);
    object.body = body;
    CHECK(ab_world_destroy_body(&world, body, &error));
    controls.after_step = NULL;
    CHECK(ab_game_frame_advance(&frame, &world, &timing, &controls, &plan, &error));
    CHECK(object.rendered.position.x == rendered.position.x &&
          object.history[1].position.x == 7);
    CHECK(!ab_game_frame_object_reset(&world, body, 3, &object, &error));
    CHECK(object.rendered.position.x == rendered.position.x);
    ab_world_close(&world);
    return true;
}

static bool compare_frame_poses(FILE *file, const AbGameFrameObject objects[23],
                                const NativeObjectBody records[23], size_t frame,
                                const char *phase) {
    for (size_t index = 0; index < 23; ++index) {
        uint32_t expected[9], actual[9];
        CHECK(read_body_words(file, expected, 9));
        const AbBodyPose samples[] = {objects[index].history[0],
                                      objects[index].history[1],
                                      objects[index].rendered};
        for (size_t pose = 0; pose < 3; ++pose) {
            actual[pose * 3] = number(samples[pose].position.x).bits;
            actual[pose * 3 + 1] = number(samples[pose].position.y).bits;
            actual[pose * 3 + 2] = number(samples[pose].angle).bits;
        }
        CHECK(compare_level_words(actual, expected, 9, frame, phase,
                                  records[index].name, index));
    }
    return true;
}

static bool replay_level_frames(FILE *file, AbPhysicsWorld *world,
                                const NativeObjectBody records[23],
                                const uint32_t controls[8], bool extended,
                                AbGameFrame *update, AbGamePhysicsPrepare *prepare,
                                AbClassicSimulation *simulation) {
    const AbObjectBody *bird = NULL;
    size_t bird_index = 0;
    CHECK(update && update->count == 23 && update->objects);
    AbGameFrameObject *poses = update->objects;
    AbGameFrameControls stepping = {
        .velocity_iterations = controls[3],
        .position_iterations = controls[4],
        .maximum_steps = 32,
        .context = prepare,
        .before_step = prepare ? ab_game_physics_before_step : NULL,
        .after_steps = prepare ? ab_game_physics_after_steps : NULL};
    AbError error = {0};
    CHECK(number(0x1.111112p-5f).bits == controls[2]);
    for (size_t index = 0; index < 23; ++index) {
        const AbObjectBody *object = &records[index].instance;
        CHECK(ab_game_frame_object_reset(world, object->body,
                                         (uint32_t)from_bits(object->collider.bits),
                                         poses + index, &error));
        if (records[index].leading_bird) {
            CHECK(!bird);
            bird = &records[index].instance;
            bird_index = index;
        }
    }
    CHECK(bird);
    uint32_t seed[7];
    CHECK(read_body_words(file, seed, extended ? 7 : 5));
    AbSlingTransferStart start = {.position = {from_bits(seed[0]), from_bits(seed[1])},
                                  .target = {from_bits(seed[2]), from_bits(seed[3])}};
    AbSlingTransferInput transfer;
    AbSlingTransferFrame sling_frame;
    CHECK(ab_slingshot_transfer_begin(&start, &transfer));
    /* The load call already made a partial update before our first paired frame.
     * Its elapsed timer and both sling vectors are explicit captured inputs. */
    transfer.seconds = from_bits(seed[4]);
    CHECK(ab_slingshot_transfer_body(world, bird, &transfer, &sling_frame, &error));
    transfer.time = sling_frame.next_time;
    transfer.rotation = world->storage.bodies[bird->body].motion.mass.angle;
    CHECK(ab_game_frame_object_reset(world, bird->body,
                                     (uint32_t)from_bits(bird->collider.bits),
                                     poses + bird_index, &error));
    CHECK(!sling_frame.complete);
    bool ready = false;
    uint32_t total_steps = 0;
    size_t frame_count = extended ? 12 : 10;
    for (size_t frame = 0; frame < frame_count; ++frame) {
        uint32_t input[7], expected[4];
        CHECK(read_body_words(file, input, 7) && input[1] <= 1);
        CHECK(number(update->clock.time).bits == input[3] &&
              number(update->clock.accumulator).bits == input[4]);
        CHECK(compare_level_snapshot(file, world, records, frame, "before",
                                     LEVEL_ALL_BOUNDS));
        CHECK(!extended ||
              compare_frame_poses(file, poses, records, frame, "before poses"));
        AbFrameClockInput elapsed = {.seconds = from_bits(input[0]),
                                     .speed_factor = from_bits(input[2]),
                                     .speed_factor_present = true,
                                     .physics_enabled = input[1] != 0};
        AbFrameClockPlan plan;
        if (prepare) {
            CHECK(ab_frame_clock_advance(&update->clock, &elapsed,
                                         stepping.maximum_steps, &plan));
            prepare->water_seconds = plan.physics_seconds;
        }
        CHECK(simulation
                  ? ab_classic_simulation_advance(simulation, &elapsed, &plan, &error)
                  : ab_game_frame_advance(update, world, &elapsed, &stepping, &plan,
                                          &error));
        CHECK(number(plan.next.time).bits == input[5] &&
              number(plan.next.accumulator).bits == input[6]);
        total_steps += plan.steps;
        if (elapsed.physics_enabled) {
            /* updateObjectProperties receives the body's atan2 result, including
             * its negative half-circle, before the next Lua animation call. */
            const AbRigidTransform *pose =
                &world->storage.bodies[bird->body].motion.mass.transform;
            CHECK(
                ab_angle_from_components(pose->sine, pose->cosine, &transfer.rotation));
        }
        if (ready) {
            CHECK(extended);
            CHECK(ab_slingshot_hold_body(
                world, bird, (AbVec2){from_bits(seed[5]), from_bits(seed[6])}, &error));
        } else {
            transfer.seconds = elapsed.seconds;
            CHECK(ab_slingshot_transfer_body(world, bird, &transfer, &sling_frame,
                                             &error));
            transfer.time = sling_frame.next_time;
            ready = sling_frame.complete;
        }
        transfer.rotation = world->storage.bodies[bird->body].motion.mass.angle;
        CHECK(ab_game_frame_object_reset(world, bird->body,
                                         (uint32_t)from_bits(bird->collider.bits),
                                         poses + bird_index, &error));
        CHECK(compare_level_snapshot(file, world, records, frame, "after",
                                     LEVEL_ALL_BOUNDS));
        CHECK(!extended ||
              compare_frame_poses(file, poses, records, frame, "after poses"));
        CHECK(read_body_words(file, expected, 4));
        uint32_t actual[] = {number(transfer.time).bits, ready ? 1u : 0u,
                             ready ? 0u : 1u, number(ready ? 1.0f : 0.0f).bits};
        CHECK(compare_level_words(actual, expected, 4, frame, "after", "sling", 0));
    }
    char marker[80];
    CHECK(ready && total_steps == (extended ? 26u : 20u));
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB level native frames complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All %zu native level frames match: clock, %u physics steps, sling transfer "
           "and dynamic completion, 23 body states, ordered contacts and all fixture "
           "bounds%s. Sling vectors, initial partial timer and physics enable are "
           "inputs.\n",
           frame_count, total_steps, extended ? ", history and rendered poses" : "");
    return true;
}

#endif
