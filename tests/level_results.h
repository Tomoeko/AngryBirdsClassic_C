#ifndef AB_TEST_LEVEL_RESULTS_H
#define AB_TEST_LEVEL_RESULTS_H

#include "ab/bird_retirement.h"
#include "ab/level_complete.h"
#include "ab/level_flow.h"
#include "ab/slingshot_control.h"

/* Restricted first-level ending projection. Queue poses, proxy IDs, ordinary
 * timer configuration and camera-target identity remain explicit inputs.
 * No specialties, bubbles, joints or level-specific completion hooks occur in
 * this trace. Full mode exit, ending event dispatch and presentation are open. */
typedef struct {
    AbBirdRetirementState retirement;
    AbLevelBonusState bonus;
    AbLevelGoals goals;
    AbLevelCompletionState completion;
    AbLevelEndingDelay ending;
    AbLevelCompleteState initialization;
    AbSlingBandState band;
    AbShotState shot;
    AbShotBird held;
    LevelImpact *impact;
    const AbLevelObject *source_objects;
    AbPhysicsWorld *world;
    float speed_threshold;
    float reset_timer;
    float initial_bonus_timer;
    float last_bird_index;
    size_t leading;
    size_t current_object;
    size_t bird_removals;
    size_t bonuses;
    size_t notifications;
    size_t ending_events;
    bool camera_targets_bird;
    bool completed;
    bool before_script_called;
    bool controller_enabled;
    bool pending_exit;
} LevelResults;

static bool initialize_level_results(FILE *file, LevelImpact *impact,
                                     const AbLevelObject objects[23], AbVec2 rest,
                                     LevelResults *output) {
    uint32_t fields[12];
    CHECK(read_body_words(file, fields, 12));
    CHECK(fields[0] < 23 && fields[2] <= 1 && fields[4] <= 1 && fields[11] <= 1);
    CHECK(impact->records[fields[0]].leading_bird);
    *output =
        (LevelResults){.retirement = {.ready_timer = from_bits(fields[1]),
                                      .water_timer = from_bits(fields[3]),
                                      .ready_timer_present = fields[2] != 0,
                                      .water_timer_present = fields[4] != 0},
                       .bonus = {.bird_index = from_bits(fields[7]),
                                 .score_increment = from_bits(fields[9])},
                       .impact = impact,
                       .source_objects = objects,
                       .band = {.rest = rest},
                       .initialization = {.current_bird_index = from_bits(fields[7]),
                                          .birds_shot = 1,
                                          .current_bird_present = true},
                       .speed_threshold = from_bits(fields[5]),
                       .reset_timer = from_bits(fields[6]),
                       .initial_bonus_timer = from_bits(fields[10]),
                       .last_bird_index = from_bits(fields[8]),
                       .leading = fields[0],
                       .camera_targets_bird = fields[11] != 0,
                       .controller_enabled = true};
    output->current_object = SIZE_MAX;
    float last_index = 0;
    for (size_t index = 0; index < 23; ++index) {
        const AbDataValue *order = data_field(objects[index].placement, "startNumber");
        if (!objects[index].physics.controllable || !order ||
            order->kind != AB_DATA_NUMBER) {
            continue;
        }
        float start_number = from_bits(order->bits);
        if (start_number > last_index) {
            last_index = start_number;
        }
        if (start_number == output->initialization.current_bird_index) {
            CHECK(output->current_object == SIZE_MAX);
            output->current_object = index;
        }
    }
    CHECK(output->current_object < 23 && last_index == output->last_bird_index);
    output->held.name = impact->records[output->current_object].name;
    output->shot.dragged = &output->held;
    return true;
}

static bool results_next_bird(const LevelResults *state, float wanted) {
    for (size_t index = 0; index < 23; ++index) {
        const AbLevelObject *object = state->source_objects + index;
        const AbDataValue *order = data_field(object->placement, "startNumber");
        if (object->physics.controllable && order && order->kind == AB_DATA_NUMBER &&
            from_bits(order->bits) == wanted &&
            ab_world_has_body(state->world,
                              state->impact->records[index].instance.body)) {
            return true;
        }
    }
    return false;
}

static bool results_cancel_control(void *context, AbSlingControlCall *call,
                                   AbError *error) {
    LevelResults *state = context;
    if (call->operation == AB_SLING_CONTROL_QUERY_LASER) {
        call->audio_playing = false;
        return true;
    }
    CHECK(call->operation == AB_SLING_CONTROL_SET_POSITION &&
          call->bird == &state->held);
    size_t body = state->impact->records[state->current_object].instance.body;
    float angle = state->world->storage.bodies[body].motion.mass.angle;
    CHECK(ab_world_set_transform(state->world, body, call->position, angle, error));
    state->held.position = call->position;
    return true;
}

static bool results_complete_host(void *context, AbLevelCompleteCall *call,
                                  AbError *error) {
    LevelResults *state = context;
    switch (call->operation) {
        case AB_LEVEL_COMPLETE_NOTIFY:
        case AB_LEVEL_COMPLETE_CAMERA:
        case AB_LEVEL_COMPLETE_PLAY_AUDIO:
            return true;
        case AB_LEVEL_COMPLETE_CANCEL_DRAG:
            return ab_slingshot_cancel_drag(&state->band, &state->shot,
                                            results_cancel_control, state, error);
        case AB_LEVEL_COMPLETE_QUERY_BIRD:
            call->bird_present = results_next_bird(state, call->bird_index);
            return true;
        case AB_LEVEL_COMPLETE_RESOLVE_AUDIO:
            /* Audio/camera/event requests are boundaries, not presented effects in
             * this state-only projection. No replacement resource is supplied. */
            call->text = NULL;
            return true;
        case AB_LEVEL_COMPLETE_SCRIPT:
            return ab_fail(error, "Unexpected level-specific completion hook");
    }
    return ab_fail(error, "Unexpected completion boundary");
}

static bool results_state_matches(const LevelResults *state,
                                  const AbPhysicsWorld *world,
                                  const uint32_t expected[6], size_t frame) {
    bool alive =
        ab_world_has_body(world, state->impact->records[state->leading].instance.body);
    const AbBirdRetirementState *timer = &state->retirement;
    uint32_t actual[] = {
        state->completed,
        alive,
        alive && timer->ready_timer_present,
        alive && timer->ready_timer_present ? number(timer->ready_timer).bits : 0,
        alive && timer->water_timer_present,
        alive && timer->water_timer_present ? number(timer->water_timer).bits : 0};
    CHECK(compare_level_words(actual, expected, 6, frame, "results", "state", 0));
    return true;
}

static bool results_notify(void *context, AbError *error) {
    (void)error;
    LevelResults *state = context;
    ++state->notifications;
    return true;
}

static bool results_query_goals(void *context, bool *cleared, AbError *error) {
    LevelResults *state = context;
    return ab_level_check_goals(&state->goals, results_notify, state, cleared, error);
}

static bool results_has_movement(const LevelImpact *impact,
                                 const AbPhysicsWorld *world) {
    for (size_t index = 0; index < 23; ++index) {
        size_t body = impact->records[index].instance.body;
        if (!ab_world_has_body(world, body)) {
            continue;
        }
        const AbBodyMassState *mass = &world->storage.bodies[body].motion.mass;
        float squared = mass->linear_velocity.y * mass->linear_velocity.y +
                        mass->linear_velocity.x * mass->linear_velocity.x;
        /* Verified in ELF 0x3250a4..0x3250f0. The angular comparison is signed.
         * This scene has no joint, specialty-timer, bubble or ignore-table case. */
        if (squared > 9.0f || mass->angular_velocity > 0.1f) {
            return true;
        }
    }
    return false;
}

static bool advance_level_results(LevelResults *state, AbPhysicsWorld *world,
                                  float seconds) {
    LevelImpact *impact = state->impact;
    AbError error = {0};
    state->world = world;
    /* This capture dispatches the queued ending on the following Lua update,
     * after that update's physics steps. General event/mode dispatch is open. */
    if (state->pending_exit) {
        state->controller_enabled = false;
        return true;
    }
    size_t body = impact->records[state->leading].instance.body;
    if (ab_world_has_body(world, body)) {
        AbVec2 velocity = impact->objects[state->leading].damage.velocity;
        AbBirdRetirementInput input = {
            .seconds = seconds,
            .speed_squared = velocity.x * velocity.x + velocity.y * velocity.y,
            .speed_threshold = state->speed_threshold,
            .reset_timer = state->reset_timer,
            .controllable = true,
            .shot_literal_true = true,
            .camera_targets_bird = state->camera_targets_bird};
        AbBirdRetirementFrame result;
        CHECK(ab_bird_retirement_frame(&state->retirement, &input, &result));
        state->retirement = result.next;
        if (result.remove_bird) {
            CHECK(impact_remove_bird(impact, state->leading, state->current_object,
                                     &error));
            ++state->bird_removals;
        }
    }
    if (state->completed) {
        CHECK(ab_slingshot_hold_body(world,
                                     &impact->records[state->current_object].instance,
                                     state->band.rest, &error));
        AbLevelBonusGate gate;
        CHECK(ab_level_bonus_gate(&state->bonus, seconds,
                                  results_next_bird(state, state->bonus.bird_index),
                                  &gate));
        if (gate.append_score) {
            impact->score = impact->score + state->bonus.score_increment;
            ++state->bonuses;
        }
        state->bonus = gate.next;
        if (gate.before_ending) {
            AbLevelBeforeEnding before = {.timer = state->ending.timer,
                                          .script_called = state->before_script_called};
            CHECK(ab_level_before_ending(&before, seconds, NULL, NULL, &error));
            state->ending.timer = before.timer;
            state->before_script_called = before.script_called;
        }
        state->ending.moving_objects = results_has_movement(impact, world);
        AbLevelEndingGate ending;
        CHECK(ab_level_ending_delay(&state->ending, seconds, true, false, &ending));
        state->ending = ending.next;
        if (ending.queue_level_ended) {
            ++state->ending_events;
            state->pending_exit = true;
        }
        return true;
    }
    bool remaining[23];
    for (size_t index = 0; index < 23; ++index) {
        AbDataValue goal =
            ab_object_property(&impact->objects[index].definition, span("levelGoal"));
        remaining[index] =
            ab_world_has_body(world, impact->records[index].instance.body) &&
            goal.kind != AB_DATA_NIL &&
            (goal.kind != AB_DATA_BOOLEAN || goal.bits != 0);
    }
    state->goals.remaining_goals = remaining;
    state->goals.goal_count = 23;
    state->completion.moving_objects = results_has_movement(impact, world);
    CHECK(ab_level_check_complete(&state->completion, results_query_goals, state,
                                  &state->completed, &error));
    state->goals.remaining_goals = NULL;
    state->goals.goal_count = 0;
    if (state->completed) {
        CHECK(ab_level_complete_begin(&state->initialization, results_complete_host,
                                      state, &error));
        CHECK(state->initialization.completed && state->initialization.completing &&
              state->initialization.bonus_timer == state->initial_bonus_timer);
        state->bonus.completed = true;
        state->bonus.timer = state->initialization.bonus_timer;
        state->bonus.bird_index = state->initialization.remaining_bird_index;
        state->ending.timer = state->initialization.ending_timer;
        CHECK(ab_slingshot_hold_body(world,
                                     &impact->records[state->current_object].instance,
                                     state->band.rest, &error));
    }
    return true;
}

#endif
