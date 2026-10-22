#include "ab/collision_damage.h"

#include <math.h>

static bool finite_velocity(AbVec2 velocity) {
    return isfinite(velocity.x) && isfinite(velocity.y);
}

bool ab_collision_velocity_offer(AbDeferredVelocity *state, AbVec2 velocity) {
    if (!state || !finite_velocity(velocity) ||
        (state->pending && !finite_velocity(state->velocity))) {
        return false;
    }
    if (state->pending) {
        float next_speed = velocity.y * velocity.y + velocity.x * velocity.x;
        float previous_speed = state->velocity.y * state->velocity.y +
                               state->velocity.x * state->velocity.x;
        if (!(next_speed < previous_speed)) {
            return true;
        }
    }
    state->velocity = velocity;
    state->pending = true;
    return true;
}

bool ab_collision_velocity_flush(AbDeferredVelocity *state, AbBodyMotion *body) {
    if (!state) {
        return false;
    }
    if (!state->pending) {
        return true;
    }
    if (!ab_body_velocity_set(body, state->velocity)) {
        return false;
    }
    state->pending = false;
    return true;
}
