#include "ab/physics_weld.h"

#include <math.h>
#include <string.h>

static bool finite_point(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static bool finite_impulse(AbVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool valid_state(const AbWeldState *state, const AbVelocity velocities[2]) {
    if (!finite_impulse(state->impulse) || !isfinite(state->gamma) ||
        !isfinite(state->bias)) {
        return false;
    }
    const float matrix[] = {state->inverse.xx, state->inverse.xy, state->inverse.xz,
                            state->inverse.yy, state->inverse.yz, state->inverse.zz};
    for (size_t index = 0; index < 6; ++index) {
        if (!isfinite(matrix[index])) {
            return false;
        }
    }
    for (size_t body = 0; body < 2; ++body) {
        if (!finite_point(state->offsets[body]) ||
            !isfinite(state->inverse_mass[body]) || state->inverse_mass[body] < 0.0f ||
            !isfinite(state->inverse_inertia[body]) ||
            state->inverse_inertia[body] < 0.0f ||
            !finite_point(velocities[body].linear) ||
            !isfinite(velocities[body].angular)) {
            return false;
        }
    }
    return true;
}

static AbVec2 relative_velocity(const AbWeldState *state,
                                const AbVelocity velocities[2]) {
    float y = velocities[1].linear.y;
    y += velocities[1].angular * state->offsets[1].x;
    y -= velocities[0].linear.y;
    y -= velocities[0].angular * state->offsets[0].x;
    float x = velocities[1].linear.x;
    x -= velocities[1].angular * state->offsets[1].y;
    x -= velocities[0].linear.x;
    x += velocities[0].angular * state->offsets[0].y;
    return (AbVec2){x, y};
}

static void apply_impulse(const AbWeldState *state, AbVec2 impulse,
                          float positive_angular, AbVelocity velocities[2]) {
    for (size_t body = 0; body < 2; ++body) {
        float angular = impulse.x * state->offsets[body].y;
        angular = -angular;
        angular += state->offsets[body].x * impulse.y;
        angular -= positive_angular;
        angular *= state->inverse_inertia[body];
        AbVec2 linear = {state->inverse_mass[body] * impulse.x,
                         state->inverse_mass[body] * impulse.y};
        if (body == 0) {
            velocities[body].linear.x -= linear.x;
            velocities[body].linear.y -= linear.y;
            velocities[body].angular -= angular;
        } else {
            velocities[body].linear.x += linear.x;
            velocities[body].linear.y += linear.y;
            velocities[body].angular += angular;
        }
    }
}

bool ab_weld_velocity_solve(const AbWeldDefinition *definition, AbWeldState *state,
                            AbVelocity velocities[2], AbError *error) {
    if (!definition || !state || !velocities || !isfinite(definition->frequency) ||
        !valid_state(state, velocities)) {
        return ab_fail(error, "Invalid weld velocity constraint/storage");
    }
    AbWeldState following = *state;
    AbVelocity updated[2] = {velocities[0], velocities[1]};
    bool spring = definition->frequency > 0.0f;
    if (spring) {
        float angular = updated[1].angular - updated[0].angular;
        angular += following.bias;
        angular += following.impulse.z * following.gamma;
        float impulse = following.inverse.zz * angular;
        impulse = -impulse;
        updated[0].angular -= following.inverse_inertia[0] * impulse;
        updated[1].angular += following.inverse_inertia[1] * impulse;
        following.impulse.z += impulse;
    }
    AbVec2 relative = relative_velocity(&following, updated);
    float x = relative.y * following.inverse.xy;
    x += relative.x * following.inverse.xx;
    float y = relative.y * following.inverse.yy;
    y += relative.x * following.inverse.xy;
    float z = 0.0f;
    if (!spring) {
        float angular = updated[1].angular - updated[0].angular;
        x += angular * following.inverse.xz;
        y += angular * following.inverse.yz;
        z = relative.y * following.inverse.yz;
        z += relative.x * following.inverse.xz;
        z += angular * following.inverse.zz;
        following.impulse.z -= z;
    }
    apply_impulse(&following, (AbVec2){-x, -y}, z, updated);
    following.impulse.x -= x;
    following.impulse.y -= y;
    if (!finite_impulse(following.impulse) || !finite_point(updated[0].linear) ||
        !finite_point(updated[1].linear) || !isfinite(updated[0].angular) ||
        !isfinite(updated[1].angular)) {
        return ab_fail(error, "Weld velocity arithmetic overflow");
    }
    *state = following;
    memcpy(velocities, updated, sizeof(updated));
    return true;
}
