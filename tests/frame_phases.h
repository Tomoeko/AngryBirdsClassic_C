#ifndef AB_TEST_FRAME_PHASES_H
#define AB_TEST_FRAME_PHASES_H

typedef struct {
    AbGameFrame *frame;
    size_t body;
    size_t before, after, complete;
    unsigned events[12];
    size_t count;
    unsigned fail_phase;
} FramePhaseCheck;

static bool frame_phase_before(void *context, AbPhysicsWorld *world, float seconds,
                               AbError *error) {
    (void)error;
    FramePhaseCheck *check = context;
    CHECK(check->count < 8 && !world->locked);
    check->events[check->count++] = 1;
    ++check->before;
    CHECK(number(seconds).bits == UINT32_C(0x3d088889));
    CHECK(world->storage.bodies[check->body].motion.force.x == 7);
    if (check->fail_phase == 1) {
        return false;
    }
    return ab_world_apply_force(world, check->body, (AbVec2){23, -8}, -9);
}

static bool frame_phase_after(void *context, AbPhysicsWorld *world, AbError *error) {
    FramePhaseCheck *check = context;
    CHECK(check->count < 8 && !world->locked);
    check->events[check->count++] = 2;
    ++check->after;
    const AbBodyMotion *motion = &world->storage.bodies[check->body].motion;
    CHECK(number(motion->force.x).bits == 0 && number(motion->force.y).bits == 0 &&
          number(motion->torque).bits == 0);
    CHECK(motion->mass.linear_velocity.x == (float)check->after);
    if (check->fail_phase == 2) {
        return false;
    }
    CHECK(ab_world_apply_force(world, check->body, (AbVec2){7, 8}, 9));
    return ab_world_set_transform(world, check->body,
                                  (AbVec2){7 + (float)check->after, 4}, 0, error);
}

static bool frame_phase_world(void *context, AbPhysicsWorld *world, AbError *error) {
    (void)error;
    FramePhaseCheck *check = context;
    CHECK(check->count < 12 && !world->locked);
    check->events[check->count++] = 4;
    const AbBodyMotion *motion = &world->storage.bodies[check->body].motion;
    CHECK(motion->force.x == 30 && motion->force.y == 0 && motion->torque == 0);
    return check->fail_phase != 4;
}

static bool frame_phase_complete(void *context, AbPhysicsWorld *world, AbError *error) {
    (void)error;
    FramePhaseCheck *check = context;
    CHECK(check->count < 8 && !world->locked);
    check->events[check->count++] = 3;
    ++check->complete;
    const AbBodyMotion *motion = &world->storage.bodies[check->body].motion;
    CHECK(number(motion->force.x).bits == 0 && number(motion->force.y).bits == 0 &&
          number(motion->torque).bits == 0);
    if (check->after) {
        const AbGameFrameObject *object = check->frame->objects;
        CHECK(object->history[check->frame->history_index].position.x ==
              7 + (float)check->after);
        CHECK(object->rendered.position.x == 1);
    }
    if (check->fail_phase == 3) {
        return false;
    }
    return ab_world_apply_force(world, check->body, (AbVec2){11, 12}, 13);
}

static bool frame_phase_edge_checks(void) {
    for (unsigned scenario = 0; scenario < 7; ++scenario) {
        LevelStepStorage memory;
        AbPhysicsWorld world = {0};
        AbError error = {0};
        const uint32_t settings[] = {0, 0, UINT32_C(0x3d088889), 8, 3, 0, 1, 1};
        CHECK(open_level_step_world(&memory, &world, settings, &error));
        world.definition.clear_forces = false;
        AbBodyDefinition definition = {.type = AB_BODY_DYNAMIC,
                                       .position = {1, 2},
                                       .gravity_scale = 1,
                                       .awake = true,
                                       .active = true};
        size_t body;
        CHECK(ab_world_create_body(&world, &definition, &body, &error));
        CHECK(ab_world_apply_force(&world, body, (AbVec2){7, 8}, 9));
        AbGameFrameObject object;
        CHECK(ab_game_frame_object_reset(&world, body, 3, &object, &error));
        AbGameFrame frame = {.objects = &object, .count = 1};
        FramePhaseCheck check = {
            .frame = &frame,
            .body = body,
            .fail_phase = scenario >= 3 ? (scenario == 6 ? 4 : scenario - 2) : 0};
        AbGameFrameControls controls = {.velocity_iterations = 8,
                                        .position_iterations = 3,
                                        .maximum_steps = 8,
                                        .context = &check,
                                        .after_step = frame_phase_after,
                                        .before_step = frame_phase_before,
                                        .after_steps = frame_phase_complete,
                                        .after_world_step = frame_phase_world};
        AbFrameClockInput timing = {.seconds = scenario == 1 ? 0 : 0x1.555556p-4f,
                                    .physics_enabled = scenario != 2};
        AbFrameClockPlan plan = {.steps = 99};
        bool okay =
            ab_game_frame_advance(&frame, &world, &timing, &controls, &plan, &error);
        CHECK(okay == (scenario < 3));
        const AbBodyMotion *motion = &world.storage.bodies[body].motion;
        if (scenario == 0) {
            const unsigned expected[] = {1, 4, 2, 1, 4, 2, 3};
            CHECK(plan.steps == 2 && check.count == 7 && check.before == 2 &&
                  check.after == 2 && check.complete == 1);
            CHECK(memcmp(check.events, expected, sizeof(expected)) == 0);
            CHECK(object.rendered.position.x > 8 && object.rendered.position.x < 9);
        } else if (scenario == 1) {
            CHECK(plan.steps == 0 && check.count == 1 && check.events[0] == 3 &&
                  object.rendered.position.x == 1);
        } else if (scenario == 2) {
            CHECK(plan.steps == 0 && check.count == 0 && motion->force.x == 7 &&
                  motion->force.y == 8 && motion->torque == 9);
        } else {
            CHECK(frame.clock.time == 0 && frame.clock.accumulator == 0 &&
                  plan.steps == 99 && object.rendered.position.x == 1);
            CHECK(check.count == (scenario == 3   ? 1u
                                  : scenario == 4 ? 3u
                                  : scenario == 5 ? 7u
                                                  : 2u));
            CHECK(motion->force.x == (scenario == 3 ? 7 : scenario == 6 ? 30 : 0));
        }
        if (scenario < 2) {
            CHECK(motion->force.x == 11 && motion->force.y == 12 &&
                  motion->torque == 13);
        }
        ab_world_close(&world);
    }
    return true;
}

#endif
