#include "ab/physics_prismatic.h"

#include "joint_solver_internal.h"
#include "prismatic_internal.h"

#include <math.h>
#include <string.h>

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static bool valid_inputs(const AbPrismaticDefinition *definition,
                         const AbPrismaticState *state,
                         const AbPositionState positions[2]) {
    if (!definition || !state || !positions ||
        !finite_point(definition->local_anchors[0]) ||
        !finite_point(definition->local_anchors[1]) ||
        !finite_point(definition->local.axis) ||
        !finite_point(definition->local.perpendicular) ||
        !isfinite(definition->reference_angle) ||
        !isfinite(definition->lower_translation) ||
        !isfinite(definition->upper_translation)) {
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

static float arm(AbVec2 displacement, AbVec2 direction) {
    float result = displacement.y * direction.x;
    result = -result;
    result += displacement.x * direction.y;
    return result;
}

static float projection(AbVec2 displacement, AbVec2 direction) {
    float result = displacement.y * direction.y;
    result += displacement.x * direction.x;
    return result;
}

static bool prepare_geometry(const AbPrismaticDefinition *definition,
                             const AbPrismaticState *state,
                             const AbPositionState positions[2],
                             AbPrismaticState *geometry, AbVec2 *displacement) {
    AbVec2 offsets[2];
    AbJointSolverBody frame = {0};
    for (size_t body = 0; body < 2; ++body) {
        frame = (AbJointSolverBody){state->local_centers[body], positions[body].angle,
                                    state->inverse_mass[body],
                                    state->inverse_inertia[body]};
        if (!ab_joint_solver_offset(definition->local_anchors[body], &frame,
                                    offsets + body)) {
            return false;
        }
        geometry->inverse_mass[body] = state->inverse_mass[body];
        geometry->inverse_inertia[body] = state->inverse_inertia[body];
    }
    frame.angle = positions[0].angle;
    frame.local_center = (AbVec2){0, 0};
    if (!ab_joint_solver_offset(definition->local.axis, &frame, &geometry->axis) ||
        !ab_joint_solver_offset(definition->local.perpendicular, &frame,
                                &geometry->perpendicular)) {
        return false;
    }
    float y = positions[1].center.y + offsets[1].y;
    y -= positions[0].center.y;
    y -= offsets[0].y;
    float x = positions[1].center.x + offsets[1].x;
    x -= positions[0].center.x;
    x -= offsets[0].x;
    AbVec2 span = {offsets[0].x + x, offsets[0].y + y};
    geometry->axis_arms[0] = arm(span, geometry->axis);
    geometry->axis_arms[1] = arm(offsets[1], geometry->axis);
    geometry->perpendicular_arms[0] = arm(span, geometry->perpendicular);
    geometry->perpendicular_arms[1] = arm(offsets[1], geometry->perpendicular);
    *displacement = (AbVec2){x, y};
    return finite_point(*displacement);
}

static bool limit_right(const AbPrismaticDefinition *definition, float translation,
                        float *linear_error, float *right) {
    if (!definition->enable_limit) {
        return false;
    }
    float width = definition->upper_translation - definition->lower_translation;
    width = width > 0 ? width : -width;
    float error;
    if (width < 0.1f) {
        /* The target's equal-limit row uses translation itself, including when
         * the narrow bounds are nonzero. Keep its constructor version's rule. */
        *right = translation >= 0.2f    ? -0.2f
                 : translation >= -0.2f ? -translation
                                        : 0.2f;
        error = translation > 0 ? translation : -translation;
    } else if (translation <= definition->lower_translation) {
        float delta = translation - definition->lower_translation;
        delta += 0.05f;
        *right = delta >= 0 ? -0.0f : delta >= -0.2f ? -delta : 0.2f;
        error = definition->lower_translation - translation;
    } else if (translation >= definition->upper_translation) {
        float delta = translation - definition->upper_translation;
        error = delta;
        delta -= 0.05f;
        *right = delta >= 0.2f ? -0.2f : delta >= 0 ? -delta : -0.0f;
    } else {
        return false;
    }
    if (*linear_error <= error) {
        *linear_error = error;
    }
    return true;
}

static bool apply_impulse(const AbPrismaticState *geometry, AbVec3 impulse,
                          AbPositionState positions[2]) {
    float x = impulse.z * geometry->axis.x;
    x += impulse.x * geometry->perpendicular.x;
    float y = impulse.z * geometry->axis.y;
    y += impulse.x * geometry->perpendicular.y;
    for (size_t body = 0; body < 2; ++body) {
        float angular = impulse.y;
        angular += impulse.x * geometry->perpendicular_arms[body];
        angular += impulse.z * geometry->axis_arms[body];
        angular *= geometry->inverse_inertia[body];
        float linear_x = geometry->inverse_mass[body] * x;
        float linear_y = geometry->inverse_mass[body] * y;
        if (body == 0) {
            positions[body].center.x -= linear_x;
            positions[body].center.y -= linear_y;
            positions[body].angle -= angular;
        } else {
            positions[body].center.x += linear_x;
            positions[body].center.y += linear_y;
            positions[body].angle += angular;
        }
        if (!finite_point(positions[body].center) || !isfinite(positions[body].angle)) {
            return false;
        }
    }
    return true;
}

AbPositionResult ab_prismatic_position_solve(const AbPrismaticDefinition *definition,
                                             const AbPrismaticState *state,
                                             AbPositionState positions[2],
                                             AbError *error) {
    if (!valid_inputs(definition, state, positions)) {
        ab_fail(error, "Invalid prismatic position definition/state/storage");
        return AB_POSITION_INVALID;
    }
    AbPrismaticState geometry = {0};
    AbVec2 displacement;
    if (!prepare_geometry(definition, state, positions, &geometry, &displacement)) {
        ab_fail(error, "Unsupported prismatic position rotation or geometry overflow");
        return AB_POSITION_INVALID;
    }
    float perpendicular = projection(displacement, geometry.perpendicular);
    float angle = positions[1].angle - positions[0].angle;
    angle -= definition->reference_angle;
    float linear_error = perpendicular > 0 ? perpendicular : -perpendicular;
    float angular_error = angle > 0 ? angle : -angle;
    float right_z = 0;
    float translation = projection(displacement, geometry.axis);
    bool active = limit_right(definition, translation, &linear_error, &right_z);
    if (!isfinite(linear_error) || !isfinite(angular_error) || !isfinite(translation)) {
        ab_fail(error, "Prismatic position error overflow");
        return AB_POSITION_INVALID;
    }
    AbSymmetric3 matrix = ab_prismatic_constraint_matrix(&geometry);
    AbVec3 impulse;
    if (active) {
        if (!ab_symmetric3_solve(&matrix, (AbVec3){-perpendicular, -angle, right_z},
                                 &impulse, error)) {
            return AB_POSITION_INVALID;
        }
    } else {
        AbVec2 reduced;
        if (!ab_symmetric3_solve22(&matrix, (AbVec2){-perpendicular, -angle}, &reduced,
                                   error)) {
            return AB_POSITION_INVALID;
        }
        impulse = (AbVec3){reduced.x, reduced.y, 0};
    }
    AbPositionState updated[2] = {positions[0], positions[1]};
    if (!apply_impulse(&geometry, impulse, updated)) {
        ab_fail(error, "Prismatic position impulse overflow");
        return AB_POSITION_INVALID;
    }
    memcpy(positions, updated, sizeof(updated));
    return linear_error <= 0.05f && angular_error <= 0x1.1df46cp-5f
               ? AB_POSITION_RESOLVED
               : AB_POSITION_UNRESOLVED;
}
