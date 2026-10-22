#include "ab/bird_shot.h"

#include <math.h>

static bool valid_vector(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static bool valid_bird(const AbShotBird *bird) {
    return bird && bird->name && valid_vector(bird->position) && isfinite(bird->mass) &&
           bird->mass > 0;
}

static bool read_definition(AbShotHost host, void *context, AbShotBird *bird,
                            AbShotDefinition *definition, AbError *error) {
    if (!valid_bird(bird)) {
        return ab_fail(error, "Invalid shot bird after callback");
    }
    AbShotCall call = {.operation = AB_SHOT_READ_DEFINITION, .bird = bird};
    if (!host(context, &call, error)) {
        return false;
    }
    *definition = call.definition;
    return true;
}

static bool bird_call(AbShotHost host, void *context, AbShotOperation operation,
                      AbShotBird *bird, const char *text, AbError *error) {
    if (!valid_bird(bird)) {
        return ab_fail(error, "Invalid shot bird after callback");
    }
    AbShotCall call = {.operation = operation, .bird = bird, .text = text};
    return host(context, &call, error);
}

static bool launch_velocity(const AbShotBird *bird, const AbVec2 *impulse,
                            AbVec2 *output, AbError *error) {
    if (!valid_bird(bird) || !isfinite(bird->mass) || bird->mass <= 0 ||
        !valid_vector(*impulse)) {
        return ab_fail(error, "Invalid shot mass or impulse after callback");
    }
    float inverse_mass = 1.0f / bird->mass;
    AbVec2 result = {inverse_mass * impulse->x, inverse_mass * impulse->y};
    if (!valid_vector(result)) {
        return ab_fail(error, "Unsupported shot launch velocity");
    }
    *output = result;
    return true;
}

static bool shot_audio(AbShotState *state, AbShotHost host, void *context,
                       AbError *error) {
    AbShotDefinition definition = {0};
    if (!read_definition(host, context, state->dragged, &definition, error)) {
        return false;
    }
    if (!definition.launch_sound) {
        return true;
    }
    if (!read_definition(host, context, state->dragged, &definition, error)) {
        return false;
    }
    AbShotCall call = {.operation = AB_SHOT_RESOLVE_AUDIO,
                       .bird = state->dragged,
                       .text = definition.launch_sound};
    if (!host(context, &call, error)) {
        return false;
    }
    call.operation = AB_SHOT_PLAY_AUDIO;
    call.value = 1;
    if (!host(context, &call, error)) {
        return false;
    }
    return bird_call(host, context, AB_SHOT_PLAY_SLING_SOUND, state->dragged, NULL,
                     error);
}

static bool shot_sprite(AbShotState *state, AbShotHost host, void *context,
                        AbError *error) {
    AbShotDefinition definition = {0};
    if (!read_definition(host, context, state->flying, &definition, error)) {
        return false;
    }
    if (!definition.sprite_flying || !valid_bird(state->flying)) {
        return definition.sprite_flying == NULL;
    }
    if (state->flying->override_drawing) {
        return true;
    }
    if (!read_definition(host, context, state->flying, &definition, error)) {
        return false;
    }
    state->flying->sprite = definition.sprite_flying;
    return bird_call(host, context, AB_SHOT_SET_SPRITE, state->flying,
                     state->flying->sprite, error);
}

bool ab_bird_shot_publish(AbShotState *state, AbVec2 *position, AbVec2 *impulse,
                          AbShotHost host, AbShotPublisher publish, void *context,
                          AbError *error) {
    if (!state || !position || !impulse || !host || !valid_bird(state->dragged) ||
        !valid_vector(*position) || !valid_vector(*impulse) ||
        !isfinite(state->birds_shot) || !isfinite(state->bird_count) ||
        !isfinite(state->bird_reload_delay) || !isfinite(state->time) ||
        !isfinite(state->score)) {
        return ab_fail(error, "Invalid bird-shot transition");
    }
    AbShotCall call = {
        .operation = AB_SHOT_SET_POSITION, .bird = state->dragged, .vector = *position};
    if (!host(context, &call, error)) {
        return false;
    }
    call = (AbShotCall){.operation = AB_SHOT_SET_VELOCITY, .bird = state->dragged};
    if (!host(context, &call, error)) {
        return false;
    }
    call = (AbShotCall){.operation = AB_SHOT_READ_SHOT_COUNT};
    if (!host(context, &call, error) || !isfinite(call.value)) {
        return false;
    }
    float shot_count = call.value;
    call = (AbShotCall){.operation = AB_SHOT_READ_BACKWARDS_COUNT};
    if (!host(context, &call, error) || !isfinite(call.value)) {
        return false;
    }
    call = (AbShotCall){.operation = AB_SHOT_NOTIFY_BIRD,
                        .launch_position = position,
                        .launch_impulse = impulse,
                        .value = shot_count,
                        .backwards_count = call.value};
    if (!host(context, &call, error) || !valid_bird(state->dragged) ||
        !valid_vector(*impulse)) {
        return false;
    }
    call = (AbShotCall){.operation = AB_SHOT_APPLY_IMPULSE,
                        .bird = state->dragged,
                        .vector = *impulse,
                        .point = state->dragged->position};
    if (!host(context, &call, error) || !valid_bird(state->dragged) ||
        !valid_vector(*impulse)) {
        return false;
    }
    state->dragged->start_impulse = *impulse;
    if (publish && !publish(context, AB_SHOT_PUBLISH_START_IMPULSE, state, error))
        return false;
    if (state->has_level_hook &&
        !bird_call(host, context, AB_SHOT_LEVEL_HOOK, state->dragged, NULL, error)) {
        return false;
    }
    state->flying = state->dragged;
    if (publish && !publish(context, AB_SHOT_PUBLISH_FLYING, state, error))
        return false;
    AbShotDefinition definition = {0};
    if (!read_definition(host, context, state->flying, &definition, error)) {
        return false;
    }
    if (definition.on_shot && !bird_call(host, context, AB_SHOT_DEFINITION_HOOK,
                                         state->flying, definition.on_shot, error)) {
        return false;
    }
    if (!valid_bird(state->dragged)) {
        return ab_fail(error, "Invalid shot bird after hook");
    }
    state->tutorial_timer = 0;
    state->dragged->shot = true;
    state->dragged->has_collided = false;
    state->specialty_available = true;
    state->moving_above_tolerance = true;
    state->selection_played = false;
    state->bird_ready = false;
    state->bird_fired = true;
    state->birds_shot = state->birds_shot + 1.0f;
    state->next_bird_timer = state->bird_reload_delay;
    state->current_bird_name = NULL;
    if (publish && !publish(context, AB_SHOT_PUBLISH_FLAGS, state, error))
        return false;
    if (state->birds_shot == state->bird_count) {
        state->last_score_time = state->time;
        state->old_score = state->score;
        if (publish && !publish(context, AB_SHOT_PUBLISH_SCORE, state, error))
            return false;
        call = (AbShotCall){.operation = AB_SHOT_NOTIFY_ALL};
        if (!host(context, &call, error)) {
            return false;
        }
    }
    state->other_bird_count = 0;
    if (publish && !publish(context, AB_SHOT_PUBLISH_OTHER_BIRDS, state, error))
        return false;
    if (!shot_sprite(state, host, context, error) ||
        !bird_call(host, context, AB_SHOT_ATTACH_PARTICLES, state->dragged, NULL,
                   error) ||
        !shot_audio(state, host, context, error)) {
        return false;
    }
    if (!valid_bird(state->dragged)) {
        return ab_fail(error, "Invalid shot bird after presentation callback");
    }
    if (state->dragged->has_script) {
        call = (AbShotCall){.operation = AB_SHOT_SCRIPT_LAUNCH, .bird = state->dragged};
        if (!launch_velocity(state->dragged, impulse, &call.vector, error) ||
            !host(context, &call, error)) {
            return false;
        }
    }
    AbVec2 velocity;
    if (!launch_velocity(state->dragged, impulse, &velocity, error)) {
        return false;
    }
    state->dragged->launch_velocity = velocity;
    if (publish && !publish(context, AB_SHOT_PUBLISH_VELOCITY, state, error))
        return false;
    state->dragged = NULL;
    return true;
}

bool ab_bird_shot(AbShotState *state, AbVec2 *position, AbVec2 *impulse,
                  AbShotHost host, void *context, AbError *error) {
    return ab_bird_shot_publish(state, position, impulse, host, NULL, context, error);
}
