#include "ab/physics_revolute.h"

#include <math.h>
#include <string.h>

static bool finite_point(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static bool valid_state(const AbRevoluteState *state, const AbVelocity velocities[2]) {
    if (!state || !velocities || (unsigned)state->limit > AB_JOINT_LIMIT_EQUAL ||
        !isfinite(state->matrix.xx) || !isfinite(state->matrix.xy) ||
        !isfinite(state->matrix.xz) || !isfinite(state->matrix.yy) ||
        !isfinite(state->matrix.yz) || !isfinite(state->matrix.zz) ||
        !isfinite(state->motor_mass) || !isfinite(state->motor_impulse) ||
        !isfinite(state->impulse.x) || !isfinite(state->impulse.y) ||
        !isfinite(state->impulse.z)) {
        return false;
    }
    for (size_t body = 0; body < 2; ++body) {
        if (!finite_point(state->offsets[body]) ||
            !isfinite(state->inverse_mass[body]) || state->inverse_mass[body] < 0 ||
            !isfinite(state->inverse_inertia[body]) ||
            state->inverse_inertia[body] < 0 ||
            !finite_point(velocities[body].linear) ||
            !isfinite(velocities[body].angular)) {
            return false;
        }
    }
    return true;
}

static bool motor(const AbRevoluteDefinition *definition, AbRevoluteState *state,
                  float seconds, AbVelocity velocities[2]) {
    if (!definition->enable_motor || state->limit == AB_JOINT_LIMIT_EQUAL ||
        state->inverse_inertia[0] + state->inverse_inertia[1] == 0) {
        return true;
    }
    float speed = velocities[1].angular - velocities[0].angular;
    speed -= definition->motor_speed;
    float proposed = state->motor_impulse;
    proposed -= state->motor_mass * speed;
    float maximum = seconds * definition->maximum_motor_torque;
    if (!isfinite(proposed) || !isfinite(maximum)) {
        return false;
    }
    float minimum = -maximum;
    maximum = maximum > proposed ? proposed : maximum;
    maximum = minimum > maximum ? minimum : maximum;
    float delta = maximum - state->motor_impulse;
    state->motor_impulse = maximum;
    velocities[0].angular -= state->inverse_inertia[0] * delta;
    velocities[1].angular += state->inverse_inertia[1] * delta;
    return isfinite(velocities[0].angular) && isfinite(velocities[1].angular);
}

static AbVec2 anchor_speed(const AbRevoluteState *state,
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

static bool limit_impulse(AbRevoluteState *state, AbVec2 speed, float angular_speed,
                          AbVec3 *impulse, AbError *error) {
    AbVec3 solved;
    if (!ab_symmetric3_solve(&state->matrix, (AbVec3){speed.x, speed.y, angular_speed},
                             &solved, error)) {
        return false;
    }
    float total = state->impulse.z - solved.z;
    bool constrained = (state->limit == AB_JOINT_LIMIT_LOWER && total < 0) ||
                       (state->limit == AB_JOINT_LIMIT_UPPER && total > 0);
    if (!constrained) {
        *impulse = (AbVec3){-solved.x, -solved.y, -solved.z};
        state->impulse.x -= solved.x;
        state->impulse.y -= solved.y;
        state->impulse.z = total;
        return true;
    }
    float x = -speed.x;
    x += state->impulse.z * state->matrix.xz;
    float y = -speed.y;
    y += state->impulse.z * state->matrix.yz;
    AbVec2 linear;
    if (!ab_symmetric3_solve22(&state->matrix, (AbVec2){x, y}, &linear, error)) {
        return false;
    }
    *impulse = (AbVec3){linear.x, linear.y, -state->impulse.z};
    state->impulse.x += linear.x;
    state->impulse.y += linear.y;
    state->impulse.z = 0;
    return true;
}

static bool apply_impulse(const AbRevoluteState *state, AbVec3 impulse, bool coupled,
                          AbVelocity velocities[2]) {
    for (size_t body = 0; body < 2; ++body) {
        float angular = impulse.x * state->offsets[body].y;
        angular = -angular;
        angular += impulse.y * state->offsets[body].x;
        if (coupled) {
            angular += impulse.z;
        }
        angular *= state->inverse_inertia[body];
        float x = state->inverse_mass[body] * impulse.x;
        float y = state->inverse_mass[body] * impulse.y;
        if (body == 0) {
            velocities[body].linear.x -= x;
            velocities[body].linear.y -= y;
            velocities[body].angular -= angular;
        } else {
            velocities[body].linear.x += x;
            velocities[body].linear.y += y;
            velocities[body].angular += angular;
        }
        if (!finite_point(velocities[body].linear) ||
            !isfinite(velocities[body].angular)) {
            return false;
        }
    }
    return isfinite(state->impulse.x) && isfinite(state->impulse.y) &&
           isfinite(state->impulse.z);
}

bool ab_revolute_velocity_solve(const AbRevoluteDefinition *definition,
                                AbRevoluteState *state, float seconds,
                                AbVelocity velocities[2], AbError *error) {
    if (!definition || !isfinite(definition->motor_speed) ||
        !isfinite(definition->maximum_motor_torque) ||
        definition->maximum_motor_torque < 0 || !isfinite(seconds) || seconds < 0 ||
        !valid_state(state, velocities)) {
        return ab_fail(error, "Invalid revolute velocity definition/state/storage");
    }
    AbRevoluteState following = *state;
    AbVelocity updated[2] = {velocities[0], velocities[1]};
    if (!motor(definition, &following, seconds, updated)) {
        return ab_fail(error, "Revolute motor arithmetic overflow");
    }
    AbVec2 speed = anchor_speed(&following, updated);
    bool coupled = definition->enable_limit &&
                   following.limit != AB_JOINT_LIMIT_INACTIVE &&
                   following.inverse_inertia[0] + following.inverse_inertia[1] != 0;
    AbVec3 impulse;
    if (coupled) {
        float angular_speed = updated[1].angular - updated[0].angular;
        if (!limit_impulse(&following, speed, angular_speed, &impulse, error)) {
            return false;
        }
    } else {
        AbVec2 linear;
        if (!ab_symmetric3_solve22(&following.matrix, (AbVec2){-speed.x, -speed.y},
                                   &linear, error)) {
            return false;
        }
        impulse = (AbVec3){linear.x, linear.y, 0};
        following.impulse.x += linear.x;
        following.impulse.y += linear.y;
    }
    if (!apply_impulse(&following, impulse, coupled, updated)) {
        return ab_fail(error, "Revolute velocity impulse overflow");
    }
    *state = following;
    memcpy(velocities, updated, sizeof(updated));
    return true;
}
