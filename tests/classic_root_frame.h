#ifndef AB_TEST_CLASSIC_ROOT_FRAME_H
#define AB_TEST_CLASSIC_ROOT_FRAME_H

#include "ab/classic_frame.h"
#include "classic_camera_level.h"

/* Root services here remain the explicit controlled boundaries used by the
 * root corpus. This exercises continuous actual physics/root/particle ownership,
 * not a production menu factory or a native complete-frame comparison. */
typedef struct {
    RootTest *root;
    AbClassicFrame frame;
    AbClassicLevel *level;
    AbClassicFrameParticles particles;
    AbClassicFrameSpotlight spotlight, *spotlight_pointer;
    AbParticles *engine;
    AbClassicSimulation *simulation;
    AbClassicRootCall original_mode;
    AbEngineRandom random;
    bool physics, pending, available, fail_update;
    AbDataValue speed;
    unsigned iteration, phase_calls, update_calls, end_calls;
    float menu_time, world_time;
} RootFrameTest;

static bool frame_test_phase(void *context, const AbClassicSimulationCall *call,
                             AbError *error) {
    RootFrameTest *test = context;
    (void)error;
    CHECK(test->frame.updating && test->particles.active && !test->root->count &&
          (unsigned)call->stage == test->phase_calls % 3 &&
          call->time > ab_classic_level_view(test->level).frame->clock.time);
    ++test->phase_calls;
    AbFrameClockPlan unchanged = {.steps = 91};
    CHECK(!ab_classic_frame_update(&test->frame, 0, &unchanged, NULL) &&
          unchanged.steps == 91 && !test->frame.failed);
    return true;
}

static bool frame_test_available(void *context, bool available, AbError *error) {
    RootFrameTest *test = context;
    CHECK(test->frame.updating && test->particles.active && !test->pending &&
          available && !test->root->count);
    ++test->update_calls;
    return !test->fail_update || ab_fail(error, "Intentional updateAvailable failure");
}

static bool frame_test_mode(void *context, const AbDataValue *arguments, size_t count,
                            AbDataValue *result, AbError *error) {
    RootFrameTest *test = context;
    CHECK(test->frame.updating && test->particles.active && count == 2 &&
          arguments[0].bits == number(test->root->seconds).bits &&
          arguments[1].bits == test->root->globals.time.bits);
    if (test->iteration == 8)
        test->physics = false;
    if (test->iteration == 16)
        test->physics = true;
    return test->original_mode.call(test->original_mode.context, arguments, count,
                                    result, error);
}

static bool frame_test_end(void *context, float seconds, AbError *error) {
    RootFrameTest *test = context;
    (void)error;
    test->menu_time += seconds;
    if (test->physics)
        test->world_time += seconds;
    const AbParticle *menu = ab_particles_at(test->engine, true, 0);
    const AbParticle *world = ab_particles_at(test->engine, false, 0);
    const AbParticle *level = ab_particles_level_at(test->engine, 0);
    CHECK(test->frame.updating && test->particles.calling && menu && world && level &&
          menu->time == test->menu_time && world->time == test->world_time &&
          level->time == test->world_time);
    ++test->end_calls;
    return true;
}

static bool frame_test_missing_sprite(void *context, AbSpan name, uintptr_t *output,
                                      AbError *error) {
    (void)context;
    (void)error;
    CHECK(name.size == 0);
    *output = 0;
    return true;
}

static bool continuous_frame_checks(void) {
    RootTest root;
    CHECK(prepare(&root, 0));
    RootFrameTest test = {.root = &root, .physics = true, .available = true};
    ClassicCameraLevelInput source;
    CHECK(classic_camera_open_level(&source, &test.level));
    AbClassicLevelView level = ab_classic_level_view(test.level);
    CHECK(ab_classic_simulation_open(
        test.level,
        (AbClassicSimulationConfig){.minimum_x = -1000,
                                    .maximum_x = 1000,
                                    .velocity_iterations = 8,
                                    .position_iterations = 3,
                                    .maximum_steps = 4,
                                    .phase = frame_test_phase,
                                    .context = &test,
                                    .limits = {.storage_bytes = 65536}},
        &test.simulation, NULL));

    AbDataEntry sprite_entry = {number(1), text("")};
    AbDataTable sprites = {&sprite_entry, 1, 1, NULL};
    AbDataEntry box_entries[4];
    const char *box_names[] = {"minVelX", "maxVelX", "minVelY", "maxVelY"};
    for (size_t index = 0; index < 4; ++index)
        box_entries[index] = (AbDataEntry){text(box_names[index]), number(0)};
    AbDataTable box = {box_entries, 4, 4, NULL};
    const char *names[] = {"gravityX",    "gravityY",      "minAngleVel",
                           "maxAngleVel", "minScaleBegin", "maxScaleBegin",
                           "minScaleEnd", "maxScaleEnd",   "lifeTime"};
    AbDataEntry entries[12];
    for (size_t index = 0; index < 9; ++index)
        entries[index] = (AbDataEntry){text(names[index]), number(index < 4    ? 0.0f
                                                                  : index == 8 ? 10.0f
                                                                               : 1.0f)};
    entries[9] =
        (AbDataEntry){text("sprites"), {.kind = AB_DATA_TABLE, .table = &sprites}};
    entries[10] =
        (AbDataEntry){text("emitter_box"), {.kind = AB_DATA_TABLE, .table = &box}};
    entries[11] = (AbDataEntry){text("randomizeBirthPosition"), boolean(false)};
    AbDataTable definition = {entries, 12, 12, NULL};
    AbParticleSource particle_source = {text("frame").string, &definition};
    CHECK(ab_particles_open((AbParticleConfig){.sources = &particle_source,
                                               .source_count = 1,
                                               .capacity = 2,
                                               .level_capacity = 2,
                                               .storage_bytes = 8192,
                                               .random = &test.random,
                                               .admission = {40, 125, 0.5f},
                                               .sprite = frame_test_missing_sprite},
                            &test.engine, NULL));
    AbParticleEmission emission = {
        .definition = text("frame").string, .amount = 1, .position = {0, -10}};
    CHECK(ab_particles_emit(test.engine, &emission, NULL) &&
          ab_particles_emit_level(test.engine, &emission, NULL));
    emission.alternate = true;
    CHECK(ab_particles_emit(test.engine, &emission, NULL));
    AbDrawList draws = {0};
    AbParticleDrawStyle style = {.width = 480, .height = 320};
    float menu_scale = 1;
    test.spotlight = (AbClassicFrameSpotlight){&test, frame_test_end};
    test.spotlight_pointer = &test.spotlight;
    CHECK(ab_classic_frame_particles_initialize(
        (AbClassicFrameParticleConfig){.particles = test.engine,
                                       .physics_enabled = &test.physics,
                                       .menu_scale = &menu_scale,
                                       .style = &style,
                                       .draws = &draws,
                                       .spotlight = &test.spotlight_pointer},
        &test.particles, NULL));
    AbClassicFrameConfig config = {.level = test.level,
                                   .simulation = test.simulation,
                                   .root = root.root,
                                   .particles = &test.particles,
                                   .physics_enabled = &test.physics,
                                   .physics_speed_factor = &test.speed,
                                   .update_available_pending = &test.pending,
                                   .update_available_result = &test.available,
                                   .context = &test,
                                   .update_available = frame_test_available};
    AbClassicFrame invalid = {.failed = true};
    CHECK(!ab_classic_frame_initialize((AbClassicFrameConfig){0}, &invalid, NULL) &&
          invalid.failed);
    bool other_flag = true;
    config.physics_enabled = &other_flag;
    CHECK(!ab_classic_frame_initialize(config, &invalid, NULL) && invalid.failed);
    config.physics_enabled = &test.physics;
    CHECK(ab_classic_frame_initialize(config, &test.frame, NULL));
    test.original_mode = root.services[AB_ROOT_CURRENT_MODE];
    root.services[AB_ROOT_CURRENT_MODE] = (AbClassicRootCall){&test, frame_test_mode};
    root.step = 1;
    root.flags.release_build = boolean(false);
    root.seconds = 0x1.111112p-6f;
    float initial_y = level.world->storage.bodies[level.bodies->objects[0].physics.body]
                          .motion.mass.center.y;
    AbFrameClock expected_clock = {0};
    uint32_t total_steps = 0;
    for (test.iteration = 0; test.iteration < 64; ++test.iteration) {
        root.count = 0;
        if (test.iteration == 12)
            test.speed = text("2");
        if (test.iteration == 17)
            test.speed = (AbDataValue){0};
        if (test.iteration == 24)
            test.speed = number(2);
        if (test.iteration == 25)
            test.speed = number(0);
        if (test.iteration == 26)
            test.speed = boolean(false);
        test.pending = test.iteration == 3;
        AbFrameClockInput timing = {.seconds = root.seconds,
                                    .physics_enabled = test.physics,
                                    .speed_factor_present =
                                        test.speed.kind == AB_DATA_NUMBER};
        memcpy(&timing.speed_factor, &test.speed.bits, sizeof(timing.speed_factor));
        AbFrameClockPlan expected, result = {.steps = 91};
        CHECK(ab_frame_clock_advance(&expected_clock, &timing, 4, &expected));
        AbError error = {0};
        if (!ab_classic_frame_update(&test.frame, root.seconds, &result, &error)) {
            fprintf(stderr, "Continuous frame %u: %s\n", test.iteration, error.message);
            return false;
        }
        CHECK(result.steps == expected.steps &&
              result.next.time == expected.next.time &&
              result.next.accumulator == expected.next.accumulator &&
              test.end_calls == test.iteration + 1 && !test.particles.active &&
              !test.frame.failed && !test.frame.updating);
        bool debug_seen = false;
        for (size_t index = 0; index < root.count; ++index) {
            if (root.trace[index][0] == AB_ROOT_DEBUG_UPDATE + 1) {
                CHECK(root.trace[index][1] == 2 &&
                      root.trace[index][7] == number(root.seconds).bits &&
                      root.trace[index][9] == number(root.seconds).bits);
                debug_seen = true;
            }
        }
        CHECK(debug_seen);
        total_steps += result.steps;
        expected_clock = expected.next;
    }
    CHECK(total_steps > 20 && test.phase_calls == total_steps * 3 &&
          test.update_calls == 1 &&
          level.world->storage.bodies[level.bodies->objects[0].physics.body]
                  .motion.mass.center.y > initial_y);

    AbFrameClockPlan unchanged = {.steps = 91};
    test.speed = text("2");
    root.count = 0;
    CHECK(!ab_classic_frame_update(&test.frame, root.seconds, &unchanged, NULL) &&
          unchanged.steps == 91 && !test.frame.failed && !test.particles.active &&
          !root.count);
    test.speed = (AbDataValue){0};
    test.pending = true;
    test.fail_update = true;
    AbError error = {0};
    CHECK(!ab_classic_frame_update(&test.frame, root.seconds, &unchanged, &error) &&
          strstr(error.message, "Intentional updateAvailable failure") &&
          !test.pending && test.frame.failed && test.particles.active && !root.count &&
          unchanged.steps == 91 && test.end_calls == 64 &&
          level.frame->clock.time > expected_clock.time);
    CHECK(!ab_classic_frame_update(&test.frame, 0, &unchanged, NULL) &&
          test.update_calls == 2 && unchanged.steps == 91);

    AbClassicFrameParticleConfig particle_config = test.particles.config;
    CHECK(
        ab_classic_frame_particles_initialize(particle_config, &test.particles, NULL) &&
        ab_classic_frame_initialize(config, &test.frame, NULL));
    root.fail_kind = AB_ROOT_CURRENT_MODE + 1;
    CHECK(!ab_classic_frame_update(&test.frame, root.seconds, &unchanged, &error) &&
          strstr(error.message, "Intentional root service failure") &&
          root.trace[root.count - 1][0] == AB_ROOT_CURRENT_MODE + 1 &&
          test.frame.failed && test.particles.active && test.end_calls == 64 &&
          unchanged.steps == 91);

    close_test(&root);
    CHECK(prepare(&root, 0));
    root.step = 1;
    root.seconds = 0x1.111112p-6f;
    config.root = root.root;
    test.spotlight.move = NULL;
    CHECK(
        ab_classic_frame_particles_initialize(particle_config, &test.particles, NULL) &&
        ab_classic_frame_initialize(config, &test.frame, NULL));
    float prior_menu = ab_particles_at(test.engine, true, 0)->time;
    CHECK(!ab_classic_frame_update(&test.frame, root.seconds, &unchanged, &error) &&
          strstr(error.message, "Missing reached native Spotlight") &&
          test.frame.failed && test.particles.failed && root.count &&
          ab_particles_at(test.engine, true, 0)->time == prior_menu + root.seconds &&
          test.end_calls == 64 && unchanged.steps == 91);

    root.count = 0;
    config.update_available = NULL;
    test.pending = true;
    CHECK(
        ab_classic_frame_particles_initialize(particle_config, &test.particles, NULL) &&
        ab_classic_frame_initialize(config, &test.frame, NULL) &&
        !ab_classic_frame_update(&test.frame, root.seconds, &unchanged, &error) &&
        strstr(error.message, "Missing reached updateAvailable") && !test.pending &&
        test.frame.failed && !root.count && unchanged.steps == 91);
    ab_classic_simulation_close(test.simulation);
    ab_particles_close(test.engine);
    ab_classic_level_close(test.level);
    close_test(&root);
    return true;
}

#endif
