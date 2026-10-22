#include "ab/physics_revolute.h"

#include "joint_solver_internal.h"

#include <math.h>
#include <string.h>

static bool finite_point(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static bool valid_inputs(const AbRevoluteDefinition *definition,
                         const AbRevoluteState *state,
                         const AbPositionState positions[2]) {
    if (!definition || !state || !positions ||
        (unsigned)state->limit > AB_JOINT_LIMIT_EQUAL ||
        !finite_point(definition->local_anchors[0]) ||
        !finite_point(definition->local_anchors[1]) ||
        !isfinite(definition->reference_angle) || !isfinite(definition->lower_angle) ||
        !isfinite(definition->upper_angle) || !isfinite(state->motor_mass)) {
        return false;
    }
    for (size_t body = 0; body < 2; ++body) {
        if (!finite_point(positions[body].center) || !isfinite(positions[body].angle) ||
            !finite_point(state->local_centers[body]) ||
            !isfinite(state->inverse_mass[body]) || state->inverse_mass[body] < 0 ||
            !isfinite(state->inverse_inertia[body]) ||
            state->inverse_inertia[body] < 0) {
            return false;
        }
    }
    return true;
}

static float correct_limit(const AbRevoluteDefinition *definition,
                           const AbRevoluteState *state, AbPositionState positions[2]) {
    if (!definition->enable_limit || state->limit == AB_JOINT_LIMIT_INACTIVE ||
        state->inverse_inertia[0] + state->inverse_inertia[1] == 0) {
        return 0;
    }
    const float slop = 0x1.1df46cp-5f, maximum = 0x1.1df46cp-3f;
    float angle = positions[1].angle - positions[0].angle;
    angle -= definition->reference_angle;
    float correction, angular_error;
    if (state->limit == AB_JOINT_LIMIT_EQUAL) {
        angle -= definition->lower_angle;
        correction = angle >= maximum ? maximum : angle >= -maximum ? angle : -maximum;
        angular_error = correction > 0 ? correction : -correction;
    } else if (state->limit == AB_JOINT_LIMIT_LOWER) {
        angle -= definition->lower_angle;
        correction = angle + slop;
        correction = correction >= 0          ? 0
                     : correction >= -maximum ? correction
                                              : -maximum;
        angular_error = -angle;
    } else {
        angle -= definition->upper_angle;
        correction = angle - slop;
        correction = correction >= maximum ? maximum : correction >= 0 ? correction : 0;
        angular_error = angle;
    }
    float impulse = -(state->motor_mass * correction);
    positions[0].angle -= impulse * state->inverse_inertia[0];
    positions[1].angle += impulse * state->inverse_inertia[1];
    return angular_error;
}

static AbSymmetric3 point_matrix(const AbRevoluteState *state,
                                 const AbVec2 offsets[2]) {
    float first_x = state->inverse_inertia[0] * offsets[0].x;
    float second_x = state->inverse_inertia[1] * offsets[1].x;
    float masses = state->inverse_mass[0] + state->inverse_mass[1];
    float xx = masses;
    xx += (state->inverse_inertia[0] * offsets[0].y) * offsets[0].y;
    xx += (state->inverse_inertia[1] * offsets[1].y) * offsets[1].y;
    float yy = masses;
    yy += first_x * offsets[0].x;
    yy += second_x * offsets[1].x;
    float xy = second_x * offsets[1].y;
    xy = -xy;
    xy += (-first_x) * offsets[0].y;
    return (AbSymmetric3){.xx = xx, .xy = xy, .yy = yy};
}

AbPositionResult ab_revolute_position_solve(const AbRevoluteDefinition *definition,
                                            const AbRevoluteState *state,
                                            AbPositionState positions[2],
                                            AbError *error) {
    if (!valid_inputs(definition, state, positions)) {
        ab_fail(error, "Invalid revolute position definition/state/storage");
        return AB_POSITION_INVALID;
    }
    AbPositionState updated[2] = {positions[0], positions[1]};
    float angular_error = correct_limit(definition, state, updated);
    if (!isfinite(angular_error)) {
        ab_fail(error, "Revolute angular correction overflow");
        return AB_POSITION_INVALID;
    }
    AbVec2 offsets[2];
    for (size_t body = 0; body < 2; ++body) {
        AbJointSolverBody frame = {state->local_centers[body], updated[body].angle,
                                   state->inverse_mass[body],
                                   state->inverse_inertia[body]};
        if (!ab_joint_solver_offset(definition->local_anchors[body], &frame,
                                    offsets + body)) {
            ab_fail(error, "Unsupported revolute position rotation or offset overflow");
            return AB_POSITION_INVALID;
        }
    }
    float y = updated[1].center.y + offsets[1].y;
    y -= updated[0].center.y;
    y -= offsets[0].y;
    float x = updated[1].center.x + offsets[1].x;
    x -= updated[0].center.x;
    x -= offsets[0].x;
    float squared = y * y;
    squared += x * x;
    float position_error = sqrtf(squared);
    if (!isfinite(position_error)) {
        ab_fail(error, "Revolute linear position error overflow");
        return AB_POSITION_INVALID;
    }
    AbSymmetric3 matrix = point_matrix(state, offsets);
    AbVec2 solved;
    if (!ab_symmetric3_solve22(&matrix, (AbVec2){x, y}, &solved, error)) {
        return AB_POSITION_INVALID;
    }
    AbVec2 impulse = {-solved.x, -solved.y};
    for (size_t body = 0; body < 2; ++body) {
        float angular = offsets[body].y * impulse.x;
        angular = -angular;
        angular += offsets[body].x * impulse.y;
        angular *= state->inverse_inertia[body];
        float linear_x = state->inverse_mass[body] * impulse.x;
        float linear_y = state->inverse_mass[body] * impulse.y;
        if (body == 0) {
            updated[body].center.x -= linear_x;
            updated[body].center.y -= linear_y;
            updated[body].angle -= angular;
        } else {
            updated[body].center.x += linear_x;
            updated[body].center.y += linear_y;
            updated[body].angle += angular;
        }
        if (!finite_point(updated[body].center) || !isfinite(updated[body].angle)) {
            ab_fail(error, "Revolute position impulse overflow");
            return AB_POSITION_INVALID;
        }
    }
    memcpy(positions, updated, sizeof(updated));
    return position_error <= 0.05f && angular_error <= 0x1.1df46cp-5f
               ? AB_POSITION_RESOLVED
               : AB_POSITION_UNRESOLVED;
}
