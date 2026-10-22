#include "ab/goal_animation.h"

#include <math.h>

static bool advance_timer(float *timer, float seconds, AbError *error) {
    if (!isfinite(*timer) || !isfinite(seconds)) {
        return ab_fail(error, "Nonfinite goal animation timer");
    }
    float next = *timer - seconds;
    if (!isfinite(next)) {
        return ab_fail(error, "Goal animation timer overflow");
    }
    *timer = next;
    return true;
}

static bool reset_timer(const AbGoalAnimationControls *controls, int minimum,
                        int maximum, float *timer, AbError *error) {
    int draw;
    if (!controls || !controls->random_integer ||
        !controls->random_integer(controls->context, minimum, maximum, &draw) ||
        draw < minimum || (int64_t)draw > (int64_t)maximum + 1) {
        return ab_fail(error, "Invalid goal animation random boundary");
    }
    *timer = (float)draw / 10.0f;
    return true;
}

static bool set_face(const AbGoalAnimationControls *controls, AbGoalFace face,
                     AbError *error) {
    if (!controls || !controls->set_sprite) {
        return ab_fail(error, "Missing goal animation sprite boundary");
    }
    return controls->set_sprite(controls->context, face, error);
}

bool ab_goal_expression_update(AbGoalAnimation *state, float seconds, bool eligible,
                               const AbGoalAnimationControls *controls,
                               AbError *error) {
    if (!state) {
        return ab_fail(error, "Missing goal animation state");
    }
    if (!eligible) {
        return true;
    }
    if (!controls || !isfinite(state->strength) || !isfinite(controls->base_strength) ||
        (state->special_expression && !isfinite(controls->damage1_min))) {
        return ab_fail(error, "Invalid goal animation strength boundary");
    }
    float percent = state->strength / controls->base_strength;
    percent = percent * 100.0f;
    if (state->special_expression && percent > controls->damage1_min) {
        if (!advance_timer(&state->special_timer, seconds, error)) {
            return false;
        }
        if (state->special_timer > 0) {
            return set_face(controls, AB_GOAL_FACE_SPECIAL, error);
        }
        state->special_expression = false;
        return true;
    }
    if (!controls->disable_smiling) {
        bool lost;
        if (!controls->lose_condition) {
            return ab_fail(error, "Missing goal animation lose-condition boundary");
        }
        if (!controls->lose_condition(controls->context, &lost, error)) {
            return false;
        }
        if (lost) {
            return set_face(controls, AB_GOAL_FACE_SMILE, error);
        }
    }
    if (!advance_timer(&state->blink_timer, seconds, error)) {
        return false;
    }
    if (!(state->blink_timer < 0)) {
        return true;
    }
    bool was_blinking = state->blink_on;
    if (!reset_timer(controls, 1, was_blinking ? 30 : 4, &state->blink_timer, error) ||
        !set_face(controls, was_blinking ? AB_GOAL_FACE_DAMAGE : AB_GOAL_FACE_BLINK,
                  error)) {
        return false;
    }
    state->blink_on = !was_blinking;
    return true;
}

bool ab_goal_oink_update(AbGoalAnimation *state, float seconds, bool eligible,
                         const AbGoalAnimationControls *controls, AbError *error) {
    if (!state) {
        return ab_fail(error, "Missing goal animation state");
    }
    if (!eligible) {
        return true;
    }
    if (!advance_timer(&state->oink_timer, seconds, error)) {
        return false;
    }
    if (!(state->oink_timer < 0)) {
        return true;
    }
    if (!reset_timer(controls, 10, 60, &state->oink_timer, error)) {
        return false;
    }
    float distance;
    if (!controls->camera_distance) {
        return ab_fail(error, "Missing goal animation camera-distance boundary");
    }
    if (!controls->camera_distance(controls->context, &distance, error)) {
        return false;
    }
    if (!isfinite(distance)) {
        return ab_fail(error, "Nonfinite goal animation camera distance");
    }
    float volume = 1.0f - distance / 1000.0f;
    if (volume > 0) {
        if (!controls->play_oink) {
            return ab_fail(error, "Missing goal animation audio boundary");
        }
        return controls->play_oink(controls->context, volume, error);
    }
    return true;
}
