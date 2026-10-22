#include "ab/physics_revolute.h"

#include "joint_solver_internal.h"

#include <math.h>
#include <string.h>

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static bool finite_impulse(AbVec3 impulse) {
    return isfinite(impulse.x) && isfinite(impulse.y) && isfinite(impulse.z);
}

static bool valid_body(const AbJointSolverBody *body, AbVelocity velocity) {
    return finite_point(body->local_center) && isfinite(body->angle) &&
           isfinite(body->inverse_mass) && body->inverse_mass >= 0 &&
           isfinite(body->inverse_inertia) && body->inverse_inertia >= 0 &&
           finite_point(velocity.linear) && isfinite(velocity.angular);
}

static bool valid_matrix(AbSymmetric3 matrix) {
    return isfinite(matrix.xx) && isfinite(matrix.xy) && isfinite(matrix.xz) &&
           isfinite(matrix.yy) && isfinite(matrix.yz) && isfinite(matrix.zz);
}

static void classify_limit(const AbRevoluteDefinition *definition,
                           const AbJointSolverBody bodies[2], AbRevoluteState *state) {
    if (!definition->enable_motor || state->matrix.zz == 0) {
        state->motor_impulse = 0;
    }
    if (!definition->enable_limit || state->matrix.zz == 0) {
        state->limit = AB_JOINT_LIMIT_INACTIVE;
        return;
    }
    float width = definition->upper_angle - definition->lower_angle;
    width = width > 0 ? width : -width;
    if (width < 0x1.1df46cp-4f) {
        state->limit = AB_JOINT_LIMIT_EQUAL;
        return;
    }
    float angle = bodies[1].angle - bodies[0].angle;
    angle -= definition->reference_angle;
    AbJointLimitState following =
        angle <= definition->lower_angle   ? AB_JOINT_LIMIT_LOWER
        : angle >= definition->upper_angle ? AB_JOINT_LIMIT_UPPER
                                           : AB_JOINT_LIMIT_INACTIVE;
    if (following == AB_JOINT_LIMIT_INACTIVE || state->limit != following) {
        state->impulse.z = 0;
    }
    state->limit = following;
}

static bool warm_start(AbRevoluteState *state, float ratio, AbVelocity velocities[2]) {
    state->impulse = (AbVec3){ratio * state->impulse.x, ratio * state->impulse.y,
                              ratio * state->impulse.z};
    state->motor_impulse = ratio * state->motor_impulse;
    if (!finite_impulse(state->impulse) || !isfinite(state->motor_impulse)) {
        return false;
    }
    for (size_t body = 0; body < 2; ++body) {
        float cross = state->offsets[body].y * state->impulse.x;
        cross = -cross;
        cross += state->offsets[body].x * state->impulse.y;
        float angular = state->motor_impulse + cross;
        angular += state->impulse.z;
        angular *= state->inverse_inertia[body];
        float x = state->inverse_mass[body] * state->impulse.x;
        float y = state->inverse_mass[body] * state->impulse.y;
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
    return true;
}

bool ab_revolute_velocity_initialize(const AbRevoluteDefinition *definition,
                                     const AbJointSolverBody bodies[2],
                                     AbJointStep step, AbRevoluteState *state,
                                     AbVelocity velocities[2], AbError *error) {
    if (!definition || !bodies || !state || !velocities ||
        !finite_point(definition->local_anchors[0]) ||
        !finite_point(definition->local_anchors[1]) ||
        !isfinite(definition->reference_angle) || !isfinite(definition->lower_angle) ||
        !isfinite(definition->upper_angle) || !isfinite(definition->motor_speed) ||
        !isfinite(definition->maximum_motor_torque) ||
        definition->maximum_motor_torque < 0 || !isfinite(step.seconds) ||
        step.seconds < 0 || !isfinite(step.time_ratio) ||
        !valid_body(bodies, velocities[0]) || !valid_body(bodies + 1, velocities[1]) ||
        (step.warm_start &&
         (!finite_impulse(state->impulse) || !isfinite(state->motor_impulse) ||
          (unsigned)state->limit > AB_JOINT_LIMIT_EQUAL))) {
        return ab_fail(error, "Invalid revolute definition/body/step/storage");
    }
    AbRevoluteState following = {0};
    if (step.warm_start) {
        following.impulse = state->impulse;
        following.motor_impulse = state->motor_impulse;
        following.limit = state->limit;
    }
    for (size_t body = 0; body < 2; ++body) {
        following.local_centers[body] = bodies[body].local_center;
        following.inverse_mass[body] = bodies[body].inverse_mass;
        following.inverse_inertia[body] = bodies[body].inverse_inertia;
        if (!ab_joint_solver_offset(definition->local_anchors[body], bodies + body,
                                    following.offsets + body)) {
            return ab_fail(error, "Unsupported revolute rotation or offset overflow");
        }
    }
    following.matrix = ab_joint_solver_matrix(bodies, following.offsets);
    following.motor_mass = following.matrix.zz;
    if (following.motor_mass > 0) {
        following.motor_mass = 1.0f / following.motor_mass;
    }
    if (!valid_matrix(following.matrix) || !isfinite(following.motor_mass)) {
        return ab_fail(error, "Revolute constraint matrix overflow");
    }
    classify_limit(definition, bodies, &following);
    AbVelocity updated[2] = {velocities[0], velocities[1]};
    if (step.warm_start && !warm_start(&following, step.time_ratio, updated)) {
        return ab_fail(error, "Revolute warm-start overflow");
    }
    *state = following;
    memcpy(velocities, updated, sizeof(updated));
    return true;
}
