#ifndef AB_TEST_CLASSIC_SIMULATION_H
#define AB_TEST_CLASSIC_SIMULATION_H

#include "ab/classic_simulation.h"
#include "ab/game_object_removal.h"

typedef struct {
    AudioTest audio;
    AbClassicLevel *owner;
    AbClassicLevelView level;
    AbClassicSimulation *simulation;
    AbGameObjectCore physical[4];
    AbGameObjectCoreRegistry registry;
    AbGameObjectCoreHost host;
    AbGameObjectRemoval removal;
    AbGameFrameObject poses[2];
    AbClassicMotionProperties properties[2];
    size_t steps;
    bool invalid;
} SimulationRemovalTest;

static bool simulation_removal_audio(void *context, AbRollingAudioCall *call,
                                     AbError *error) {
    return audio_host(&((SimulationRemovalTest *)context)->audio, call, error);
}

static void simulation_forget(void *context, AbGameObjectCore *object) {
    SimulationRemovalTest *test = context;
    size_t index = (size_t)(object - test->physical);
    uintptr_t value = 0;
    bool present = true;
    test->invalid |= index >= 2 ||
                     !ab_string_map_get(test->registry.objects, object->name, &value,
                                        &present, NULL) ||
                     present ||
                     !ab_classic_level_forget_object(test->owner, index, NULL);
}

static bool simulation_remove_phase(void *context, const AbClassicSimulationCall *call,
                                    AbError *error) {
    SimulationRemovalTest *test = context;
    if (call->stage != AB_CLASSIC_SIMULATION_AFTER_STEP) {
        if (test->steps) {
            size_t orphan = test->physical[1].body;
            CHECK(test->level.world->storage.bodies[orphan].motion.force.x == 0);
        }
        return true;
    }
    if (test->steps++ == 0) {
        for (size_t index = 0; index < 2; ++index) {
            test->level.world->locked = index == 1;
            bool removed = false;
            CHECK(ab_game_object_remove(&test->removal, test->physical[index].name,
                                        &removed, error) &&
                  removed);
            test->level.world->locked = false;
            test->poses[index] = test->level.frame->objects[index];
            test->properties[index] = test->level.motion_properties[index];
        }
        CHECK(!test->invalid &&
              !ab_world_has_body(test->level.world, test->physical[0].body) &&
              ab_world_has_body(test->level.world, test->physical[1].body));
        AbClassicSimulationView view = ab_classic_simulation_view(test->simulation);
        CHECK(ab_force_queue_append(
            view.queue,
            (AbQueuedForce){.object = test->physical[1].name, .force = {1000, 0}},
            error));
    }
    return true;
}

static bool simulation_removal_checks(AbClassicLevel *level,
                                      AbClassicSimulation **output) {
    SimulationRemovalTest test = {.owner = level,
                                  .level = ab_classic_level_view(level)};
    CHECK(test.level.bodies->count == 4 && test.level.removed_objects &&
          test.level.frame->removed_objects == test.level.removed_objects &&
          !ab_classic_level_forget_object(level, 4, NULL));
    CHECK(ab_string_map_open(4, 4, 4096, &test.registry.objects, NULL) &&
          ab_string_map_open(0, 0, 4096, &test.removal.teleporters, NULL));
    test.registry.world = test.level.world;
    test.host.context = &test;
    test.host.destroy_object = simulation_forget;
    test.removal.objects = &test.registry;
    test.removal.joints = test.level.joints;
    test.removal.object_host = &test.host;
    for (size_t index = 0; index < 4; ++index) {
        test.physical[index] =
            (AbGameObjectCore){.name = test.level.bodies->objects[index].name,
                               .body = test.level.bodies->objects[index].physics.body};
        CHECK(ab_string_map_set(test.registry.objects, test.physical[index].name,
                                (uintptr_t)(test.physical + index), true, NULL));
    }
    AbClassicSimulationConfig config = {
        .minimum_x = -100,
        .maximum_x = 100,
        .velocity_iterations = 8,
        .position_iterations = 3,
        .maximum_steps = 4,
        .phase = simulation_remove_phase,
        .rolling = simulation_removal_audio,
        .context = &test,
        .limits = {.storage_bytes = 65536, .queued_forces = 2}};
    CHECK(ab_classic_simulation_open(level, config, output, NULL));
    test.simulation = *output;
    test.level.frame->clock.accumulator = 0;
    AbFrameClockInput timing = {.seconds = 0x1.111112p-4f, .physics_enabled = true};
    AbFrameClockPlan plan;
    CHECK(ab_classic_simulation_advance(*output, &timing, &plan, NULL) &&
          plan.steps == 2 && test.steps == 2 && !test.invalid);
    timing.seconds = 0x1.111112p-5f;
    CHECK(ab_classic_simulation_advance(*output, &timing, &plan, NULL) &&
          plan.steps == 1 && test.steps == 3 && !test.invalid);
    for (size_t index = 0; index < 2; ++index) {
        CHECK(test.level.removed_objects[index] &&
              !memcmp(test.poses + index, test.level.frame->objects + index,
                      sizeof(*test.poses)) &&
              !memcmp(test.properties + index, test.level.motion_properties + index,
                      sizeof(*test.properties)) &&
              ab_classic_level_forget_object(level, index, NULL));
        AbVec2 position = test.level.character_objects[index].jump.position;
        CHECK(ab_classic_level_set_position(level, index, (AbVec2){77, 88}, NULL) &&
              ab_classic_level_set_rotation(level, index, 2, NULL) &&
              !memcmp(&position, &test.level.character_objects[index].jump.position,
                      sizeof(position)));
    }
    CHECK(!ab_classic_simulation_view(*output).queue->count);
    ab_classic_simulation_close(*output);
    *output = NULL;
    ab_string_map_close(test.registry.objects);
    ab_string_map_close(test.removal.teleporters);
    return true;
}

typedef struct {
    AudioTest audio;
    AbClassicLevelView level;
    AbClassicSimulation *simulation;
    size_t body, count;
    AbClassicSimulationStage stages[16];
    float water_seconds, time;
    bool fail_script, reentry_checked;
} SimulationTest;

static bool simulation_audio(void *context, AbRollingAudioCall *call, AbError *error) {
    SimulationTest *test = context;
    return audio_host(&test->audio, call, error);
}

static bool simulation_phase(void *context, const AbClassicSimulationCall *call,
                             AbError *error) {
    SimulationTest *test = context;
    AbClassicSimulationView view = ab_classic_simulation_view(test->simulation);
    CHECK(test->count < 16 && !test->level.world->locked && call->time == test->time &&
          view.queue->count == 1);
    test->stages[test->count++] = call->stage;
    const AbBodyMotion *body = &test->level.world->storage.bodies[test->body].motion;
    if (call->stage == AB_CLASSIC_SIMULATION_AFTER_STEP) {
        CHECK(call->seconds == 0x1.111112p-5f && body->force.x == 0 &&
              body->force.y == 0 && body->torque == 0);
    } else {
        CHECK(body->force.x == 24 && body->force.y == 0 && body->torque == 0 &&
              call->seconds == (call->stage == AB_CLASSIC_SIMULATION_WATER
                                    ? test->water_seconds
                                    : 0x1.111112p-5f));
    }
    if (!test->reentry_checked) {
        AbFrameClockInput timing = {.physics_enabled = true};
        AbFrameClockPlan plan = {.steps = 99};
        CHECK(!ab_classic_simulation_advance(test->simulation, &timing, &plan, NULL) &&
              plan.steps == 99);
        test->reentry_checked = true;
    }
    if (test->fail_script && call->stage == AB_CLASSIC_SIMULATION_SCRIPT_STEP) {
        return ab_fail(error, "Requested simulation script-step failure");
    }
    return true;
}

typedef struct {
    AudioTest audio;
    AbClassicSimulation *simulation;
    AbClassicLevelView level;
    size_t after_steps;
} SimulationGlueTest;

static bool simulation_glue_audio(void *context, AbRollingAudioCall *call,
                                  AbError *error) {
    SimulationGlueTest *test = context;
    return audio_host(&test->audio, call, error);
}

static bool simulation_glue_phase(void *context, const AbClassicSimulationCall *call,
                                  AbError *error) {
    (void)error;
    SimulationGlueTest *test = context;
    if (call->stage == AB_CLASSIC_SIMULATION_AFTER_STEP) {
        AbClassicSimulationView view = ab_classic_simulation_view(test->simulation);
        CHECK(!view.glue->count && test->level.world->joint_count > 0);
        ++test->after_steps;
    }
    return true;
}

static bool simulation_glue_checks(AbClassicLevel *level,
                                   AbClassicSimulation **output) {
    SimulationGlueTest test = {.level = ab_classic_level_view(level)};
    AbClassicSimulationConfig config = {
        .minimum_x = -100,
        .maximum_x = 100,
        .velocity_iterations = 8,
        .position_iterations = 3,
        .maximum_steps = 8,
        .phase = simulation_glue_phase,
        .rolling = simulation_glue_audio,
        .context = &test,
        .limits = {.storage_bytes = 65536, .glue_pairs = SIZE_MAX}};
    CHECK(!ab_classic_simulation_open(level, config, output, NULL) && !*output);
    config.limits.glue_pairs = 2;
    CHECK(ab_classic_simulation_open(level, config, output, NULL));
    size_t bytes = ab_classic_simulation_bytes(*output);
    ab_classic_simulation_close(*output);
    *output = NULL;
    config.limits.storage_bytes = bytes - 1;
    CHECK(!ab_classic_simulation_open(level, config, output, NULL) && !*output);
    config.limits.storage_bytes = bytes;
    CHECK(ab_classic_simulation_open(level, config, output, NULL));
    test.simulation = *output;
    AbClassicSimulationView view = ab_classic_simulation_view(*output);
    CHECK(view.glue && !view.glue->count && view.glue->capacity == 2 &&
          test.level.world->joint_count == 0);
    size_t first = SIZE_MAX, second = SIZE_MAX;
    for (size_t index = 0; index < test.level.bodies->count; ++index) {
        size_t body = test.level.bodies->objects[index].physics.body;
        if (test.level.world->storage.bodies[body].motion.mass.type ==
            AB_BODY_DYNAMIC) {
            first = body;
            break;
        }
    }
    CHECK(first != SIZE_MAX);
    for (size_t index = 0; index < test.level.bodies->count; ++index) {
        size_t body = test.level.bodies->objects[index].physics.body;
        if (body != first) {
            second = body;
            break;
        }
    }
    CHECK(second != SIZE_MAX);
    view.glue->pairs[0] = (AbClassicGluePair){first, second};
    view.glue->count = 1;
    AbFrameClockInput timing = {0};
    AbFrameClockPlan plan;
    CHECK(ab_classic_simulation_advance(*output, &timing, &plan, NULL) && !plan.steps &&
          view.glue->count == 1 && !test.level.world->joint_count);
    timing.physics_enabled = true;
    CHECK(ab_classic_simulation_advance(*output, &timing, &plan, NULL) && !plan.steps &&
          view.glue->count == 1 && !test.level.world->joint_count);
    timing.seconds = 0x1.111112p-5f;
    CHECK(ab_classic_simulation_advance(*output, &timing, &plan, NULL) &&
          plan.steps == 1 && !view.glue->count && test.after_steps == 1 &&
          test.level.world->joint_count == 1 && !test.level.joints->joint_count);
    view.glue->pairs[0] = (AbClassicGluePair){first, second};
    view.glue->count = 1;
    CHECK(ab_classic_simulation_advance(*output, &timing, &plan, NULL) &&
          plan.steps == 1 && !view.glue->count && test.after_steps == 2 &&
          test.level.world->joint_count == 2);
    view.glue->pairs[0] = (AbClassicGluePair){SIZE_MAX, second};
    view.glue->count = 1;
    CHECK(!test.level.world->definition.clear_forces &&
          ab_world_apply_force(test.level.world, first, (AbVec2){7, 8}, 9));
    AbFrameClock clock = test.level.frame->clock;
    plan.steps = 99;
    CHECK(!ab_classic_simulation_advance(*output, &timing, &plan, NULL) &&
          plan.steps == 99 && view.glue->count == 1 && test.after_steps == 2 &&
          memcmp(&clock, &test.level.frame->clock, sizeof(clock)) == 0 &&
          test.level.world->storage.bodies[first].motion.force.x == 7);
    ab_classic_simulation_close(*output);
    *output = NULL;
    return true;
}

typedef struct {
    AudioTest audio;
    AbClassicSimulation *simulation;
    size_t phases;
    bool checked_reentry;
} DynamicSimulationTest;

static bool dynamic_simulation_phase(void *context, const AbClassicSimulationCall *call,
                                     AbError *error) {
    (void)error;
    DynamicSimulationTest *test = context;
    static const AbClassicSimulationStage stages[] = {AB_CLASSIC_SIMULATION_WATER,
                                                      AB_CLASSIC_SIMULATION_SCRIPT_STEP,
                                                      AB_CLASSIC_SIMULATION_AFTER_STEP};
    CHECK(call->stage == stages[test->phases % 3]);
    if (!test->checked_reentry) {
        AbFrameClockInput timing = {.physics_enabled = true};
        AbFrameClockPlan plan;
        CHECK(!ab_classic_simulation_register_circle(test->simulation, 0, NULL) &&
              !ab_classic_simulation_advance(test->simulation, &timing, &plan, NULL));
        test->checked_reentry = true;
    }
    ++test->phases;
    return true;
}

static bool dynamic_simulation_audio(void *context, AbRollingAudioCall *call,
                                     AbError *error) {
    return audio_host(&((DynamicSimulationTest *)context)->audio, call, error);
}

static bool dynamic_simulation_circle(AbClassicLevel *level, AbClassicSimulation *owner,
                                      AbGameCircleInput input, AbVec2 velocity,
                                      size_t *output) {
    CHECK(ab_classic_level_create_circle(level, input, output, NULL));
    AbClassicLevelView view = ab_classic_level_view(level);
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE,
                           .table = &view.properties->objects[*output].physical}};
    CHECK(ab_object_property_set(&access, string("definition").string,
                                 string("AlphaCircle"), NULL));
    /* Native setVelocity updates the body separately from raw copied fields. */
    CHECK(ab_body_velocity_set(
        &view.world->storage.bodies[view.bodies->objects[*output].physics.body].motion,
        velocity));
    AbFrameClock clock = view.frame->clock;
    AbFrameClockInput timing = {.seconds = 0x1.111112p-5f, .physics_enabled = true};
    AbFrameClockPlan plan = {.steps = 99};
    CHECK(!ab_classic_simulation_advance(owner, &timing, &plan, NULL) &&
          plan.steps == 99 && !memcmp(&clock, &view.frame->clock, sizeof(clock)) &&
          !ab_classic_simulation_register_circle(owner, *output + 1, NULL) &&
          ab_classic_simulation_register_circle(owner, *output, NULL) &&
          !ab_classic_simulation_register_circle(owner, *output, NULL));
    return true;
}

/* Prepared C integration: real frame/solver/history/publication owners, with
 * explicit empty phase fixture and silent native circle rolling metadata.
 * This is not an original full-session or outer split capture. */
static bool simulation_dynamic_checks(AbClassicLevel *level,
                                      AbClassicSimulation **output) {
    AbClassicLevelView loaded = ab_classic_level_view(level);
    CHECK(loaded.bodies->count == 4 && loaded.bodies->capacity == 8);
    for (size_t index = 0; index < 4; ++index) {
        loaded.world->storage.bodies[loaded.bodies->objects[index].physics.body]
            .motion.awake = false;
        CHECK(ab_classic_level_finish_object(level, index, NULL));
    }
    DynamicSimulationTest test = {0};
    AbClassicSimulationConfig config = {
        .minimum_x = -100,
        .maximum_x = 100,
        .velocity_iterations = 8,
        .position_iterations = 3,
        .maximum_steps = 4,
        .phase = dynamic_simulation_phase,
        .rolling = dynamic_simulation_audio,
        .context = &test,
        .limits = {.storage_bytes = 65536, .object_capacity = 8}};
    CHECK(ab_classic_simulation_open(level, config, output, NULL));
    test.simulation = *output;
    AbClassicSimulationView view = ab_classic_simulation_view(*output);
    AbClassicPostObject *stable_objects = view.post.objects;
    const AbClassicActivityState *stable_activity = view.post.activity;
    size_t bytes = ab_classic_simulation_bytes(*output);
    const char *const names[] = {"0_dynamic_first", "zz_dynamic_last",
                                 "middle_dynamic"};
    size_t created[4];
    for (size_t index = 0; index < 3; ++index) {
        AbGameCircleInput circle = {.name = string(names[index]).string,
                                    .position = {40 + (float)index * 4, -10},
                                    .radius = 1,
                                    .density = 1};
        CHECK(dynamic_simulation_circle(level, *output, circle, (AbVec2){6, -2},
                                        created + index));
        view = ab_classic_simulation_view(*output);
        CHECK(view.forces.count == index + 5 && view.post.count == index + 5 &&
              view.post.objects == stable_objects &&
              view.post.activity == stable_activity &&
              ab_classic_simulation_bytes(*output) == bytes && !test.phases &&
              !test.audio.count);
    }
    AbDataEntry ignored_entry = {string("AlphaCircle"), {.kind = AB_DATA_BOOLEAN}};
    AbDataTable ignored = {&ignored_entry, 1, 1, NULL};
    *view.post.ignored_definitions = table(&ignored);
    loaded.frame->clock.accumulator = 0;
    AbFrameClockInput timing = {.seconds = 0x1.111112p-5f, .physics_enabled = true};
    AbFrameClockPlan plan;
    AbGameFrameObject removed_frame = {0};
    AbClassicMotionProperties removed_properties = {0};
    for (size_t frame = 0; frame < 12; ++frame) {
        if (frame == 6) {
            size_t body = loaded.bodies->objects[created[0]].physics.body;
            CHECK(ab_world_destroy_body(loaded.world, body, NULL) &&
                  ab_classic_level_forget_object(level, created[0], NULL) &&
                  ab_level_properties_forget(loaded.properties, created[0], NULL));
            removed_frame = loaded.frame->objects[created[0]];
            removed_properties = loaded.motion_properties[created[0]];
            AbGameCircleInput replacement = {.name = string("replacement").string,
                                             .position = {60, -10},
                                             .radius = 1,
                                             .density = 1};
            CHECK(dynamic_simulation_circle(level, *output, replacement,
                                            (AbVec2){6, -2}, created + 3) &&
                  loaded.bodies->objects[created[3]].physics.body == body);
        }
        CHECK(ab_classic_simulation_advance(*output, &timing, &plan, NULL) &&
              plan.steps == 1 && test.phases == (frame + 1) * 3 && !test.audio.count &&
              view.post.activity->has_awake && !view.post.activity->moving &&
              view.post.activity->above_tolerance);
    }
    for (size_t index = 1; index < 4; ++index) {
        size_t object = created[index];
        CHECK(loaded.motion_properties[object].initialized &&
              loaded.motion_properties[object].velocity.x == 6 &&
              loaded.motion_properties[object].velocity.y == -2 &&
              loaded.character_objects[object].jump.position.x >
                  (index == 3 ? 60 : 40 + (float)index * 4));
    }
    view = ab_classic_simulation_view(*output);
    CHECK(view.forces.count == 8 && view.post.count == 8 &&
          view.post.objects == stable_objects && test.checked_reentry &&
          !memcmp(&removed_frame, loaded.frame->objects + created[0],
                  sizeof(removed_frame)) &&
          !memcmp(&removed_properties, loaded.motion_properties + created[0],
                  sizeof(removed_properties)));
    ab_classic_simulation_close(*output);
    *output = NULL;
    return true;
}

static bool simulation_checks(AbClassicLevel *level, AbClassicSimulation **output) {
    SimulationTest test = {.level = ab_classic_level_view(level)};
    AbClassicSimulationConfig config = {.minimum_x = -100,
                                        .maximum_x = 100,
                                        .velocity_iterations = 8,
                                        .position_iterations = 3,
                                        .maximum_steps = 8,
                                        .phase = simulation_phase,
                                        .rolling = simulation_audio,
                                        .context = &test,
                                        .limits = {.storage_bytes = 65536,
                                                   .queued_forces = 2,
                                                   .force_sound_choices = 8}};
    CHECK(!ab_classic_simulation_open(NULL, config, output, NULL) && !*output &&
          !ab_classic_simulation_open(level, config, NULL, NULL) &&
          ab_classic_simulation_bytes(NULL) == 0 &&
          !ab_classic_simulation_view(NULL).queue &&
          !ab_classic_simulation_advance(NULL, NULL, NULL, NULL));
    config.maximum_steps = 4097;
    CHECK(!ab_classic_simulation_open(level, config, output, NULL) && !*output);
    config.maximum_steps = 8;
    config.limits.queued_forces = SIZE_MAX;
    CHECK(!ab_classic_simulation_open(level, config, output, NULL) && !*output);
    config.limits.queued_forces = 2;
    CHECK(ab_classic_simulation_open(level, config, output, NULL));
    size_t bytes = ab_classic_simulation_bytes(*output);
    ab_classic_simulation_close(*output);
    *output = NULL;
    config.limits.storage_bytes = bytes - 1;
    CHECK(!ab_classic_simulation_open(level, config, output, NULL) && !*output);
    config.limits.storage_bytes = bytes;
    CHECK(ab_classic_simulation_open(level, config, output, NULL) &&
          ab_classic_simulation_bytes(*output) == bytes);
    test.simulation = *output;
    AbClassicSimulationView view = ab_classic_simulation_view(*output);
    CHECK(view.forces.count == 4 && view.post.count == 4 && !view.queue->count &&
          view.queue->capacity == 2 && !view.post.activity->initialized);
    size_t object = view.post.source_indices[0];
    test.body = test.level.frame->objects[object].body;
    AbBodyMotion *body = &test.level.world->storage.bodies[test.body].motion;
    body->mass.linear_velocity = (AbVec2){0};
    AbQueuedForce force = {test.level.bodies->objects[object].name,
                           {24, 0},
                           body->mass.transform.position};
    CHECK(ab_force_queue_append(view.queue, force, NULL));
    AbFrameClockInput timing = {.seconds = 0x1.555556p-4f, .physics_enabled = true};
    AbFrameClockPlan plan = {.steps = 99};
    CHECK(!ab_classic_simulation_advance(*output, NULL, &plan, NULL) &&
          !ab_classic_simulation_advance(*output, &timing, NULL, NULL) &&
          plan.steps == 99);
    test.water_seconds = timing.seconds;
    test.time = test.level.frame->clock.time + timing.seconds;
    float expected_velocity = body->mass.inverse_mass * 24 * (2 * 0x1.111112p-5f);
    CHECK(ab_classic_simulation_advance(*output, &timing, &plan, NULL));
    CHECK(plan.steps == 2 && test.count == 6 && !view.queue->count &&
          view.post.activity->initialized && view.post.activity->has_awake &&
          test.reentry_checked && expected_velocity > 0 &&
          fabsf(body->mass.linear_velocity.x - expected_velocity) < 0x1p-20f);
    const AbClassicSimulationStage expected[] = {
        AB_CLASSIC_SIMULATION_WATER,       AB_CLASSIC_SIMULATION_SCRIPT_STEP,
        AB_CLASSIC_SIMULATION_AFTER_STEP,  AB_CLASSIC_SIMULATION_WATER,
        AB_CLASSIC_SIMULATION_SCRIPT_STEP, AB_CLASSIC_SIMULATION_AFTER_STEP};
    CHECK(memcmp(test.stages, expected, sizeof(expected)) == 0 &&
          test.level.character_objects[object].jump.position.x ==
              test.level.frame->objects[object].rendered.position.x &&
          test.level.motion_properties[object].previous_awake);
    CHECK(ab_force_queue_append(view.queue, force, NULL));
    AbClassicActivityState activity = *view.post.activity;
    AbBodyMotion before = *body;
    AbGameFrameObject frame = test.level.frame->objects[object];
    float accumulator = test.level.frame->clock.accumulator;
    test.count = test.audio.count = 0;
    timing.physics_enabled = false;
    timing.seconds = 0.1f;
    CHECK(ab_classic_simulation_advance(*output, &timing, &plan, NULL) &&
          plan.steps == 0 && view.queue->count == 1 && !test.count &&
          !test.audio.count &&
          memcmp(&activity, view.post.activity, sizeof(activity)) == 0 &&
          memcmp(&before, body, sizeof(before)) == 0 &&
          memcmp(&frame, test.level.frame->objects + object, sizeof(frame)) == 0 &&
          test.level.frame->clock.accumulator == accumulator);
    timing.physics_enabled = true;
    timing.seconds = 0;
    CHECK(ab_classic_simulation_advance(*output, &timing, &plan, NULL) &&
          plan.steps == 0 && !view.queue->count && !test.count);
    CHECK(ab_force_queue_append(view.queue, force, NULL));
    test.audio.count = 0;
    test.fail_script = true;
    timing.seconds = 0.05f;
    test.water_seconds = timing.seconds;
    test.time = test.level.frame->clock.time + timing.seconds;
    AbFrameClock clock = test.level.frame->clock;
    plan.steps = 99;
    AbError error = {0};
    CHECK(!ab_classic_simulation_advance(*output, &timing, &plan, &error) &&
          strstr(error.message, "Requested simulation script-step failure") &&
          plan.steps == 99 && test.count == 2 && view.queue->count == 1 &&
          body->force.x == 24 && !test.audio.count &&
          memcmp(&clock, &test.level.frame->clock, sizeof(clock)) == 0);
    ab_classic_simulation_close(*output);
    *output = NULL;
    CHECK(ab_world_clear_forces(test.level.world, NULL));
    config.phase = NULL;
    CHECK(ab_classic_simulation_open(level, config, output, NULL) &&
          !ab_classic_simulation_advance(*output, &timing, &plan, &error) &&
          strstr(error.message, "simulation phase host") && plan.steps == 99);
    ab_classic_simulation_close(*output);
    *output = NULL;
    return simulation_glue_checks(level, output);
}

#endif
