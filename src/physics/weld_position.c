#include "ab/physics_weld.h"

#include "joint_solver_internal.h"

#include <math.h>
#include <string.h>

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static bool valid_inputs(const AbWeldDefinition *definition, const AbWeldState *state,
                         const AbPositionState positions[2]) {
    if (!definition || !state || !positions ||
        !finite_point(definition->local_anchors[0]) ||
        !finite_point(definition->local_anchors[1]) ||
        !isfinite(definition->reference_angle) || !isfinite(definition->frequency)) {
        return false;
    }
    for (size_t body = 0; body < 2; ++body) {
        if (!finite_point(positions[body].center) || !isfinite(positions[body].angle) ||
            !finite_point(state->local_centers[body]) ||
            !isfinite(state->inverse_mass[body]) || state->inverse_mass[body] < 0.0f ||
            !isfinite(state->inverse_inertia[body]) ||
            state->inverse_inertia[body] < 0.0f) {
            return false;
        }
    }
    return true;
}

static bool correction(const AbWeldDefinition *definition, const AbSymmetric3 *matrix,
                       AbVec2 error, float angle, AbVec3 *impulse, AbError *failure) {
    if (definition->frequency <= 0.0f && matrix->zz > 0.0f) {
        AbVec3 solved;
        if (!ab_symmetric3_solve(matrix, (AbVec3){error.x, error.y, angle}, &solved,
                                 failure)) {
            return false;
        }
        *impulse = (AbVec3){-solved.x, -solved.y, -solved.z};
        return true;
    }
    AbVec2 solved;
    if (!ab_symmetric3_solve22(matrix, error, &solved, failure)) {
        return false;
    }
    *impulse = (AbVec3){-solved.x, -solved.y, 0.0f};
    return true;
}

AbPositionResult ab_weld_position_solve(const AbWeldDefinition *definition,
                                        const AbWeldState *state,
                                        AbPositionState positions[2], AbError *error) {
    if (!valid_inputs(definition, state, positions)) {
        ab_fail(error, "Invalid weld position constraint/storage");
        return AB_POSITION_INVALID;
    }
    AbJointSolverBody bodies[2];
    AbVec2 offsets[2];
    for (size_t body = 0; body < 2; ++body) {
        bodies[body] = (AbJointSolverBody){
            state->local_centers[body], positions[body].angle,
            state->inverse_mass[body], state->inverse_inertia[body]};
        if (!ab_joint_solver_offset(definition->local_anchors[body], bodies + body,
                                    offsets + body)) {
            ab_fail(error, "Unsupported weld position rotation or offset overflow");
            return AB_POSITION_INVALID;
        }
    }
    AbSymmetric3 matrix = ab_joint_solver_matrix(bodies, offsets);
    float y = offsets[1].y + positions[1].center.y;
    y -= positions[0].center.y;
    y -= offsets[0].y;
    float x = offsets[1].x + positions[1].center.x;
    x -= positions[0].center.x;
    x -= offsets[0].x;
    float squared = y * y;
    squared += x * x;
    float position_error = sqrtf(squared);
    float angle = 0.0f, angular_error = 0.0f;
    if (definition->frequency <= 0.0f) {
        angle = positions[1].angle - positions[0].angle;
        angle -= definition->reference_angle;
        angular_error = angle > 0.0f ? angle : -angle;
    }
    if (!isfinite(position_error) || !isfinite(angular_error)) {
        ab_fail(error, "Weld position error overflow");
        return AB_POSITION_INVALID;
    }
    AbVec3 impulse;
    if (!correction(definition, &matrix, (AbVec2){x, y}, angle, &impulse, error)) {
        return AB_POSITION_INVALID;
    }
    AbPositionState updated[2] = {positions[0], positions[1]};
    for (size_t body = 0; body < 2; ++body) {
        float cross = offsets[body].y * impulse.x;
        cross = -cross;
        cross += impulse.y * offsets[body].x;
        float angular = impulse.z + cross;
        angular *= state->inverse_inertia[body];
        AbVec2 linear = {state->inverse_mass[body] * impulse.x,
                         state->inverse_mass[body] * impulse.y};
        if (body == 0) {
            updated[body].center.x -= linear.x;
            updated[body].center.y -= linear.y;
            updated[body].angle -= angular;
        } else {
            updated[body].center.x += linear.x;
            updated[body].center.y += linear.y;
            updated[body].angle += angular;
        }
        if (!finite_point(updated[body].center) || !isfinite(updated[body].angle)) {
            ab_fail(error, "Weld position correction overflow");
            return AB_POSITION_INVALID;
        }
    }
    memcpy(positions, updated, sizeof(updated));
    /* Preserve the original angular threshold's exact binary32 literal. */
    const uint32_t angular_word = UINT32_C(0x3d0efa36);
    float angular_limit;
    memcpy(&angular_limit, &angular_word, sizeof(angular_limit));
    return position_error <= 0.05f && angular_error <= angular_limit
               ? AB_POSITION_RESOLVED
               : AB_POSITION_UNRESOLVED;
}
