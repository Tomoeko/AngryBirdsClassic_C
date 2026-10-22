#include "ab/object_forces.h"

#include <math.h>

static bool valid_sprite(AbSpan sprite) {
    return !sprite.size || sprite.data;
}

bool ab_object_force_interval_plan(const AbObjectForceInterval *state, float seconds,
                                   bool disabled, AbObjectForceIntervalPlan *output,
                                   AbError *error) {
    if (!state || !output) {
        return ab_fail(error, "Missing object force interval state or output");
    }
    AbObjectForceIntervalPlan plan = {.next = *state};
    if (disabled) {
        *output = plan;
        return true;
    }
    if (!isfinite(seconds) || !isfinite(state->interval) ||
        (state->timer_present && !isfinite(state->timer))) {
        return ab_fail(error, "Invalid object force interval numbers");
    }
    float timer = state->timer_present ? state->timer : state->interval;
    timer = timer - seconds;
    if (timer <= 0) {
        if (!valid_sprite(state->sprite) || !valid_sprite(state->applied_sprite)) {
            return ab_fail(error, "Invalid object force pulse sprites");
        }
        timer = timer + state->interval;
        plan.pulse = true;
        plan.sprite_requested = true;
        plan.next.original_sprite = state->sprite;
        plan.next.original_present = true;
        plan.next.sprite = state->applied_sprite;
    } else if ((double)timer < (double)state->interval * 0.8 &&
               state->original_present) {
        if (!valid_sprite(state->original_sprite)) {
            return ab_fail(error, "Invalid original object force sprite");
        }
        plan.sprite_requested = true;
        plan.next.sprite = state->original_sprite;
    }
    if (!isfinite(timer)) {
        return ab_fail(error, "Object force interval exceeds finite range");
    }
    plan.next.timer = timer;
    plan.next.timer_present = true;
    *output = plan;
    return true;
}
