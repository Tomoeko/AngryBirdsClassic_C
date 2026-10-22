#include "ab/physics_prismatic.h"

#include "joint_solver_internal.h"
#include "prismatic_internal.h"

#include <math.h>
#include <string.h>

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static bool finite_impulse(AbVec3 impulse) {
    return isfinite(impulse.x) && isfinite(impulse.y) && isfinite(impulse.z);
}

bool ab_prismatic_axes_prepare(AbVec2 local_axis, AbPrismaticAxes *output,
                               AbError *error) {
    if (!output || !finite_point(local_axis)) {
        return ab_fail(error, "Invalid prismatic local axis/storage");
    }
    float squared = local_axis.y * local_axis.y;
    squared += local_axis.x * local_axis.x;
    float length = sqrtf(squared);
    if (!isfinite(length)) {
        return ab_fail(error, "Prismatic axis length overflow");
    }
    if (length >= 0x1p-23f) {
        float inverse = 1.0f / length;
        local_axis.y = inverse * local_axis.y;
        local_axis.x = inverse * local_axis.x;
    }
    *output = (AbPrismaticAxes){local_axis, {-local_axis.y, local_axis.x}};
    return true;
}

static bool valid_body(const AbJointSolverBody *body, AbVec2 center,
                       AbVelocity velocity) {
    return finite_point(body->local_center) && finite_point(center) &&
           isfinite(body->angle) && isfinite(body->inverse_mass) &&
           body->inverse_mass >= 0 && isfinite(body->inverse_inertia) &&
           body->inverse_inertia >= 0 && finite_point(velocity.linear) &&
           isfinite(velocity.angular);
}

static bool valid_matrix(AbSymmetric3 matrix) {
    return isfinite(matrix.xx) && isfinite(matrix.xy) && isfinite(matrix.xz) &&
           isfinite(matrix.yy) && isfinite(matrix.yz) && isfinite(matrix.zz);
}

static float arm(AbVec2 displacement, AbVec2 axis) {
    float result = displacement.y * axis.x;
    result = -result;
    result += displacement.x * axis.y;
    return result;
}

static void classify_limit(const AbPrismaticDefinition *definition, AbVec2 displacement,
                           AbPrismaticState *state) {
    if (!definition->enable_motor) {
        state->motor_impulse = 0;
    }
    if (!definition->enable_limit) {
        state->limit = AB_JOINT_LIMIT_INACTIVE;
        state->impulse.z = 0;
        return;
    }
    float width = definition->upper_translation - definition->lower_translation;
    width = width > 0 ? width : -width;
    if (width < 0.1f) {
        state->limit = AB_JOINT_LIMIT_EQUAL;
        return;
    }
    float translation = displacement.y * state->axis.y;
    translation += displacement.x * state->axis.x;
    AbJointLimitState following =
        translation <= definition->lower_translation   ? AB_JOINT_LIMIT_LOWER
        : translation >= definition->upper_translation ? AB_JOINT_LIMIT_UPPER
                                                       : AB_JOINT_LIMIT_INACTIVE;
    if (following == AB_JOINT_LIMIT_INACTIVE || state->limit != following) {
        state->impulse.z = 0;
    }
    state->limit = following;
}

static bool warm_start(AbPrismaticState *state, float ratio, AbVelocity velocities[2]) {
    state->impulse = (AbVec3){ratio * state->impulse.x, ratio * state->impulse.y,
                              ratio * state->impulse.z};
    state->motor_impulse *= ratio;
    if (!finite_impulse(state->impulse) || !isfinite(state->motor_impulse)) {
        return false;
    }
    float axis_impulse = state->motor_impulse + state->impulse.z;
    float x = axis_impulse * state->axis.x;
    x += state->perpendicular.x * state->impulse.x;
    float y = axis_impulse * state->axis.y;
    y += state->perpendicular.y * state->impulse.x;
    for (size_t body = 0; body < 2; ++body) {
        float angular = state->impulse.y;
        angular += state->perpendicular_arms[body] * state->impulse.x;
        angular += axis_impulse * state->axis_arms[body];
        angular *= state->inverse_inertia[body];
        float linear_x = state->inverse_mass[body] * x;
        float linear_y = state->inverse_mass[body] * y;
        if (body == 0) {
            velocities[body].linear.x -= linear_x;
            velocities[body].linear.y -= linear_y;
            velocities[body].angular -= angular;
        } else {
            velocities[body].linear.x += linear_x;
            velocities[body].linear.y += linear_y;
            velocities[body].angular += angular;
        }
        if (!finite_point(velocities[body].linear) ||
            !isfinite(velocities[body].angular)) {
            return false;
        }
    }
    return true;
}

bool ab_prismatic_velocity_initialize(const AbPrismaticDefinition *definition,
                                      const AbJointSolverBody bodies[2],
                                      const AbVec2 centers[2], AbJointStep step,
                                      AbPrismaticState *state, AbVelocity velocities[2],
                                      AbError *error) {
    if (!definition || !bodies || !centers || !state || !velocities ||
        !finite_point(definition->local_anchors[0]) ||
        !finite_point(definition->local_anchors[1]) ||
        !finite_point(definition->local.axis) ||
        !finite_point(definition->local.perpendicular) ||
        !isfinite(definition->reference_angle) ||
        !isfinite(definition->lower_translation) ||
        !isfinite(definition->upper_translation) ||
        !isfinite(definition->maximum_motor_force) ||
        definition->maximum_motor_force < 0 || !isfinite(definition->motor_speed) ||
        !isfinite(step.seconds) || step.seconds < 0 || !isfinite(step.time_ratio) ||
        !valid_body(bodies, centers[0], velocities[0]) ||
        !valid_body(bodies + 1, centers[1], velocities[1]) ||
        (step.warm_start &&
         (!finite_impulse(state->impulse) || !isfinite(state->motor_impulse) ||
          (unsigned)state->limit > AB_JOINT_LIMIT_EQUAL))) {
        return ab_fail(error, "Invalid prismatic definition/body/step/storage");
    }
    AbPrismaticState following = {0};
    if (step.warm_start) {
        following.impulse = state->impulse;
        following.motor_impulse = state->motor_impulse;
        following.limit = state->limit;
    }
    AbVec2 offsets[2];
    for (size_t body = 0; body < 2; ++body) {
        following.local_centers[body] = bodies[body].local_center;
        following.inverse_mass[body] = bodies[body].inverse_mass;
        following.inverse_inertia[body] = bodies[body].inverse_inertia;
        if (!ab_joint_solver_offset(definition->local_anchors[body], bodies + body,
                                    offsets + body)) {
            return ab_fail(error, "Unsupported prismatic rotation or offset overflow");
        }
    }
    AbJointSolverBody axis_frame = bodies[0];
    axis_frame.local_center = (AbVec2){0, 0};
    if (!ab_joint_solver_offset(definition->local.axis, &axis_frame, &following.axis) ||
        !ab_joint_solver_offset(definition->local.perpendicular, &axis_frame,
                                &following.perpendicular)) {
        return ab_fail(error, "Prismatic rotated axis overflow");
    }
    float y = centers[1].y - centers[0].y;
    y = offsets[1].y + y;
    y -= offsets[0].y;
    float x = centers[1].x - centers[0].x;
    x = offsets[1].x + x;
    x -= offsets[0].x;
    AbVec2 span = {offsets[0].x + x, offsets[0].y + y};
    following.axis_arms[0] = arm(span, following.axis);
    following.axis_arms[1] = arm(offsets[1], following.axis);
    following.perpendicular_arms[0] = arm(span, following.perpendicular);
    following.perpendicular_arms[1] = arm(offsets[1], following.perpendicular);
    following.matrix = ab_prismatic_constraint_matrix(&following);
    following.motor_mass =
        following.matrix.zz > 0 ? 1.0f / following.matrix.zz : following.matrix.zz;
    if (!valid_matrix(following.matrix) || !isfinite(following.motor_mass)) {
        return ab_fail(error, "Prismatic constraint matrix overflow");
    }
    classify_limit(definition, (AbVec2){x, y}, &following);
    AbVelocity updated[2] = {velocities[0], velocities[1]};
    if (step.warm_start && !warm_start(&following, step.time_ratio, updated)) {
        return ab_fail(error, "Prismatic warm-start overflow");
    }
    *state = following;
    memcpy(velocities, updated, sizeof(updated));
    return true;
}
