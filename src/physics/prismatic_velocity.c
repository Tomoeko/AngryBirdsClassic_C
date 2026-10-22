#include "ab/physics_prismatic.h"

#include <math.h>
#include <string.h>

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static bool valid_state(const AbPrismaticState *state, const AbVelocity velocities[2]) {
    if (!state || !velocities || (unsigned)state->limit > AB_JOINT_LIMIT_EQUAL ||
        !finite_point(state->axis) || !finite_point(state->perpendicular) ||
        !isfinite(state->matrix.xx) || !isfinite(state->matrix.xy) ||
        !isfinite(state->matrix.xz) || !isfinite(state->matrix.yy) ||
        !isfinite(state->matrix.yz) || !isfinite(state->matrix.zz) ||
        !isfinite(state->motor_mass) || !isfinite(state->motor_impulse) ||
        !isfinite(state->impulse.x) || !isfinite(state->impulse.y) ||
        !isfinite(state->impulse.z)) {
        return false;
    }
    for (size_t body = 0; body < 2; ++body) {
        if (!isfinite(state->inverse_mass[body]) || state->inverse_mass[body] < 0 ||
            !isfinite(state->inverse_inertia[body]) ||
            state->inverse_inertia[body] < 0 || !isfinite(state->axis_arms[body]) ||
            !isfinite(state->perpendicular_arms[body]) ||
            !finite_point(velocities[body].linear) ||
            !isfinite(velocities[body].angular)) {
            return false;
        }
    }
    return true;
}

static bool apply_velocity(const AbPrismaticState *state, AbVec2 linear,
                           const float angular[2], AbVelocity velocities[2]) {
    for (size_t body = 0; body < 2; ++body) {
        float x = state->inverse_mass[body] * linear.x;
        float y = state->inverse_mass[body] * linear.y;
        float turn = state->inverse_inertia[body] * angular[body];
        if (body == 0) {
            velocities[body].linear.x -= x;
            velocities[body].linear.y -= y;
            velocities[body].angular -= turn;
        } else {
            velocities[body].linear.x += x;
            velocities[body].linear.y += y;
            velocities[body].angular += turn;
        }
        if (!finite_point(velocities[body].linear) ||
            !isfinite(velocities[body].angular)) {
            return false;
        }
    }
    return true;
}

static float constraint_speed(const AbVelocity velocities[2], AbVec2 direction,
                              const float arms[2]) {
    float result = (velocities[1].linear.y - velocities[0].linear.y) * direction.y;
    result += (velocities[1].linear.x - velocities[0].linear.x) * direction.x;
    result += velocities[1].angular * arms[1];
    result -= velocities[0].angular * arms[0];
    return result;
}

static bool motor(const AbPrismaticDefinition *definition, AbPrismaticState *state,
                  float seconds, AbVelocity velocities[2]) {
    if (!definition->enable_motor || state->limit == AB_JOINT_LIMIT_EQUAL) {
        return true;
    }
    float speed = constraint_speed(velocities, state->axis, state->axis_arms);
    float difference = definition->motor_speed - speed;
    float proposed = state->motor_impulse;
    proposed += difference * state->motor_mass;
    float maximum = seconds * definition->maximum_motor_force;
    if (!isfinite(proposed) || !isfinite(maximum)) {
        return false;
    }
    float minimum = -maximum;
    maximum = maximum > proposed ? proposed : maximum;
    maximum = minimum > maximum ? minimum : maximum;
    float delta = maximum - state->motor_impulse;
    state->motor_impulse = maximum;
    AbVec2 linear = {delta * state->axis.x, delta * state->axis.y};
    float angular[] = {state->axis_arms[0] * delta, state->axis_arms[1] * delta};
    return apply_velocity(state, linear, angular, velocities);
}

static bool limit_impulse(AbPrismaticState *state, AbVec2 speed, float axial_speed,
                          AbVec3 *impulse, AbError *error) {
    AbVec3 solved;
    if (!ab_symmetric3_solve(&state->matrix, (AbVec3){-speed.x, -speed.y, -axial_speed},
                             &solved, error)) {
        return false;
    }
    AbVec3 previous = state->impulse;
    state->impulse.x += solved.x;
    state->impulse.y += solved.y;
    state->impulse.z += solved.z;
    if ((state->limit == AB_JOINT_LIMIT_LOWER && state->impulse.z <= 0) ||
        (state->limit == AB_JOINT_LIMIT_UPPER && state->impulse.z >= 0)) {
        state->impulse.z = 0;
    }
    float delta = state->impulse.z - previous.z;
    float x = -speed.x;
    x -= delta * state->matrix.xz;
    float y = -speed.y;
    y -= delta * state->matrix.yz;
    AbVec2 reduced;
    if (!ab_symmetric3_solve22(&state->matrix, (AbVec2){x, y}, &reduced, error)) {
        return false;
    }
    state->impulse.x = previous.x + reduced.x;
    state->impulse.y = previous.y + reduced.y;
    *impulse = (AbVec3){state->impulse.x - previous.x, state->impulse.y - previous.y,
                        state->impulse.z - previous.z};
    return true;
}

static bool apply_impulse(const AbPrismaticState *state, AbVec3 impulse, bool coupled,
                          AbVelocity velocities[2]) {
    AbVec2 linear;
    float angular[2];
    if (coupled) {
        linear.x = impulse.z * state->axis.x;
        linear.x += impulse.x * state->perpendicular.x;
        linear.y = impulse.z * state->axis.y;
        linear.y += impulse.x * state->perpendicular.y;
    } else {
        linear = (AbVec2){impulse.x * state->perpendicular.x,
                          impulse.x * state->perpendicular.y};
    }
    for (size_t body = 0; body < 2; ++body) {
        angular[body] = impulse.y;
        angular[body] += impulse.x * state->perpendicular_arms[body];
        if (coupled) {
            angular[body] += impulse.z * state->axis_arms[body];
        }
    }
    return apply_velocity(state, linear, angular, velocities) &&
           isfinite(state->impulse.x) && isfinite(state->impulse.y) &&
           isfinite(state->impulse.z);
}

bool ab_prismatic_velocity_solve(const AbPrismaticDefinition *definition,
                                 AbPrismaticState *state, float seconds,
                                 AbVelocity velocities[2], AbError *error) {
    if (!definition || !isfinite(definition->motor_speed) ||
        !isfinite(definition->maximum_motor_force) ||
        definition->maximum_motor_force < 0 || !isfinite(seconds) || seconds < 0 ||
        !valid_state(state, velocities)) {
        return ab_fail(error, "Invalid prismatic velocity definition/state/storage");
    }
    AbPrismaticState following = *state;
    AbVelocity updated[2] = {velocities[0], velocities[1]};
    if (!motor(definition, &following, seconds, updated)) {
        return ab_fail(error, "Prismatic motor arithmetic overflow");
    }
    float perpendicular = constraint_speed(updated, following.perpendicular,
                                           following.perpendicular_arms);
    float angular = updated[1].angular - updated[0].angular;
    bool coupled =
        definition->enable_limit && following.limit != AB_JOINT_LIMIT_INACTIVE;
    AbVec3 impulse;
    if (coupled) {
        float axial = constraint_speed(updated, following.axis, following.axis_arms);
        if (!limit_impulse(&following, (AbVec2){perpendicular, angular}, axial,
                           &impulse, error)) {
            return false;
        }
    } else {
        AbVec2 reduced;
        if (!ab_symmetric3_solve22(&following.matrix,
                                   (AbVec2){-perpendicular, -angular}, &reduced,
                                   error)) {
            return false;
        }
        impulse = (AbVec3){reduced.x, reduced.y, 0};
        following.impulse.x += reduced.x;
        following.impulse.y += reduced.y;
    }
    if (!apply_impulse(&following, impulse, coupled, updated)) {
        return ab_fail(error, "Prismatic velocity impulse overflow");
    }
    *state = following;
    memcpy(velocities, updated, sizeof(updated));
    return true;
}
