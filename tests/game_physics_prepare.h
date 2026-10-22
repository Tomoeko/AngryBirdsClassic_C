#ifndef AB_TEST_GAME_PHYSICS_PREPARE_H
#define AB_TEST_GAME_PHYSICS_PREPARE_H

#include "ab/game_physics_prepare.h"

typedef struct {
    AbGamePhysicsPrepare *prepare;
    size_t body;
    AbGamePhysicsStage stages[8];
    size_t count;
    bool fail_water;
} PhysicsPrepareCheck;

static bool physics_prepare_hook(void *context, AbGamePhysicsStage stage,
                                 AbPhysicsWorld *world, float seconds, AbError *error) {
    (void)error;
    PhysicsPrepareCheck *check = context;
    CHECK(check->count < 8 && !world->locked);
    check->stages[check->count++] = stage;
    const AbBodyMotion *body = &world->storage.bodies[check->body].motion;
    if (stage == AB_GAME_PHYSICS_APPLY_QUEUED_FORCES) {
        CHECK(seconds == 0 && body->force.x == 0 && body->force.y == 0);
        check->prepare->objects[0].prepare_forces = true;
        check->prepare->objects[0].forces.force.x = 30 + body->mass.linear_velocity.x;
    } else if (stage == AB_GAME_PHYSICS_WATER) {
        CHECK(seconds == check->prepare->water_seconds && seconds > 0.08f);
        CHECK(body->force.x == 30 && body->force.y == -9);
        return !check->fail_water;
    } else if (stage == AB_GAME_PHYSICS_SCRIPT_STEP) {
        CHECK(number(seconds).bits == UINT32_C(0x3d088889));
        CHECK(body->force.x == 30 && body->force.y == -9);
    } else {
        CHECK(stage == AB_GAME_PHYSICS_CLEAR_FORCE_FUNCTIONS && seconds == 0);
        CHECK(number(body->force.x).bits == 0 && number(body->force.y).bits == 0);
    }
    return true;
}

static bool physics_prepare_edge_checks(void) {
    LevelStepStorage memory;
    AbPhysicsWorld world = {0};
    AbError error = {0};
    const uint32_t settings[] = {0, 0, UINT32_C(0x3d088889), 8, 3, 0, 1, 1};
    CHECK(open_level_step_world(&memory, &world, settings, &error));
    world.definition.gravity = (AbVec2){0, 9};
    world.definition.clear_forces = false;
    AbBodyDefinition definition = {.type = AB_BODY_DYNAMIC,
                                   .linear_velocity = {1, 0},
                                   .gravity_scale = 1,
                                   .awake = true,
                                   .active = true};
    size_t body;
    CHECK(ab_world_create_body(&world, &definition, &body, &error));
    AbGamePhysicsObject objects[2] = {
        {.body = body,
         .friction.air_resistance = 1,
         .forces = {.gravity = {NAN, NAN}, .object_mass = 1, .force_x_present = true}},
        {.body = SIZE_MAX, .forces.gravity_enabled = true},
    };
    AbGamePhysicsPrepare prepare = {.objects = objects,
                                    .count = 2,
                                    .water_seconds = 0x1.555556p-4f,
                                    .hook = physics_prepare_hook};
    PhysicsPrepareCheck check = {.prepare = &prepare, .body = body};
    prepare.context = &check;
    AbGameFrame frame = {0};
    AbGameFrameControls controls = {.velocity_iterations = 8,
                                    .position_iterations = 3,
                                    .maximum_steps = 8,
                                    .context = &prepare,
                                    .before_step = ab_game_physics_before_step,
                                    .after_steps = ab_game_physics_after_steps};
    AbFrameClockInput timing = {.seconds = prepare.water_seconds,
                                .physics_enabled = true};
    AbFrameClockPlan plan;
    CHECK(ab_game_frame_advance(&frame, &world, &timing, &controls, &plan, &error));
    CHECK(plan.steps == 2 && check.count == 7);
    const AbGamePhysicsStage expected[] = {AB_GAME_PHYSICS_APPLY_QUEUED_FORCES,
                                           AB_GAME_PHYSICS_WATER,
                                           AB_GAME_PHYSICS_SCRIPT_STEP,
                                           AB_GAME_PHYSICS_APPLY_QUEUED_FORCES,
                                           AB_GAME_PHYSICS_WATER,
                                           AB_GAME_PHYSICS_SCRIPT_STEP,
                                           AB_GAME_PHYSICS_CLEAR_FORCE_FUNCTIONS};
    CHECK(memcmp(check.stages, expected, sizeof(expected)) == 0);
    CHECK(world.storage.bodies[body].motion.mass.linear_velocity.x == 3 &&
          world.storage.bodies[body].motion.mass.linear_velocity.y == 0);
    check.count = 0;
    timing.seconds = 0;
    prepare.water_seconds = NAN;
    CHECK(ab_game_frame_advance(&frame, &world, &timing, &controls, &plan, &error));
    CHECK(plan.steps == 0 && check.count == 1 &&
          check.stages[0] == AB_GAME_PHYSICS_CLEAR_FORCE_FUNCTIONS);
    check.count = 0;
    timing.physics_enabled = false;
    CHECK(ab_game_frame_advance(&frame, &world, &timing, &controls, &plan, &error));
    CHECK(check.count == 0);
    CHECK(!ab_game_physics_before_step(&prepare, &world, 0, &error));
    CHECK(check.count == 0);
    prepare.water_seconds = 0x1.555556p-4f;
    timing =
        (AbFrameClockInput){.seconds = prepare.water_seconds, .physics_enabled = true};
    check.fail_water = true;
    AbFrameClock saved = frame.clock;
    CHECK(!ab_game_frame_advance(&frame, &world, &timing, &controls, &plan, &error));
    CHECK(check.count == 2 && frame.clock.time == saved.time &&
          frame.clock.accumulator == saved.accumulator);
    CHECK(world.storage.bodies[body].motion.force.x == 30 &&
          world.storage.bodies[body].motion.force.y == -9);
    CHECK(world.storage.bodies[body].motion.mass.linear_velocity.x == 3);
    prepare.hook = NULL;
    prepare.count = 1;
    objects[0].prepare_forces = false;
    objects[0].forces.disabled = true;
    objects[0].forces.gravity_enabled = true;
    bool gravity_enabled = false;
    objects[0].gravity_enabled = &gravity_enabled;
    CHECK(ab_world_clear_forces(&world, &error));
    CHECK(ab_game_physics_before_step(&prepare, &world, 1.0f / 30, &error));
    CHECK(world.storage.bodies[body].motion.force.x == -3 &&
          world.storage.bodies[body].motion.force.y == -9);
    gravity_enabled = true;
    objects[0].forces.gravity_enabled = false;
    CHECK(ab_world_clear_forces(&world, &error));
    CHECK(ab_game_physics_before_step(&prepare, &world, 1.0f / 30, &error));
    CHECK(world.storage.bodies[body].motion.force.x == 0 &&
          world.storage.bodies[body].motion.force.y == 0);
    objects[0].gravity_enabled = NULL;
    CHECK(ab_game_physics_before_step(&prepare, &world, 1.0f / 30, &error));
    CHECK(world.storage.bodies[body].motion.force.x == -3 &&
          world.storage.bodies[body].motion.force.y == -9);
    ab_world_close(&world);
    return true;
}

#endif
