#include "ab/physics_weld.h"

#include "joint_solver_internal.h"

#include <math.h>
#include <string.h>

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static bool finite_impulse(AbVec3 impulse) {
    return isfinite(impulse.x) && isfinite(impulse.y) && isfinite(impulse.z);
}

static bool valid_body(const AbJointSolverBody *body, const AbVelocity *velocity) {
    return finite_point(body->local_center) && isfinite(body->angle) &&
           isfinite(body->inverse_mass) && body->inverse_mass >= 0.0f &&
           isfinite(body->inverse_inertia) && body->inverse_inertia >= 0.0f &&
           finite_point(velocity->linear) && isfinite(velocity->angular);
}

static bool prepare_mass(const AbWeldDefinition *definition,
                         const AbJointSolverBody bodies[2], AbJointStep step,
                         AbWeldState *state, AbError *error) {
    AbSymmetric3 matrix = ab_joint_solver_matrix(bodies, state->offsets);
    if (definition->frequency <= 0.0f) {
        return matrix.zz != 0.0f
                   ? ab_symmetric3_inverse(&matrix, &state->inverse, error)
                   : ab_symmetric3_inverse22(&matrix, &state->inverse, error);
    }
    if (!ab_symmetric3_inverse22(&matrix, &state->inverse, error)) {
        return false;
    }
    float mass = matrix.zz > 0.0f ? 1.0f / matrix.zz : 0.0f;
    float omega = definition->frequency * 0x1.921fb6p+2f;
    float stiffness = mass * omega;
    stiffness *= omega;
    float damping = mass + mass;
    damping *= definition->damping_ratio;
    damping *= omega;
    float denominator = damping;
    denominator += step.seconds * stiffness;
    denominator *= step.seconds;
    state->gamma = denominator != 0.0f ? 1.0f / denominator : 0.0f;
    float angle = bodies[1].angle - bodies[0].angle;
    angle -= definition->reference_angle;
    float bias = angle * step.seconds;
    bias *= stiffness;
    bias *= state->gamma;
    state->bias = bias;
    float angular = matrix.zz + state->gamma;
    state->inverse.zz = angular != 0.0f ? 1.0f / angular : 0.0f;
    return isfinite(state->gamma) && isfinite(state->bias) &&
                   isfinite(state->inverse.zz)
               ? true
               : ab_fail(error, "Weld spring arithmetic overflow");
}

static bool warm_start(const AbJointSolverBody bodies[2], AbWeldState *state,
                       AbVec3 previous, float ratio, AbVelocity velocities[2]) {
    state->impulse =
        (AbVec3){ratio * previous.x, ratio * previous.y, ratio * previous.z};
    if (!finite_impulse(state->impulse)) {
        return false;
    }
    for (size_t body = 0; body < 2; ++body) {
        float cross = state->impulse.x * state->offsets[body].y;
        cross = -cross;
        cross += state->impulse.y * state->offsets[body].x;
        float angular = state->impulse.z + cross;
        AbVec2 linear = {bodies[body].inverse_mass * state->impulse.x,
                         bodies[body].inverse_mass * state->impulse.y};
        angular *= bodies[body].inverse_inertia;
        if (body == 0) {
            velocities[body].linear.x -= linear.x;
            velocities[body].linear.y -= linear.y;
            velocities[body].angular -= angular;
        } else {
            velocities[body].linear.x += linear.x;
            velocities[body].linear.y += linear.y;
            velocities[body].angular += angular;
        }
        if (!finite_point(velocities[body].linear) ||
            !isfinite(velocities[body].angular)) {
            return false;
        }
    }
    return true;
}

bool ab_weld_velocity_initialize(const AbWeldDefinition *definition,
                                 const AbJointSolverBody bodies[2], AbJointStep step,
                                 AbWeldState *state, AbVelocity velocities[2],
                                 AbError *error) {
    if (!definition || !bodies || !state || !velocities ||
        !finite_point(definition->local_anchors[0]) ||
        !finite_point(definition->local_anchors[1]) ||
        !isfinite(definition->reference_angle) || !isfinite(definition->frequency) ||
        !isfinite(definition->damping_ratio) || !isfinite(step.seconds) ||
        step.seconds < 0.0f || !isfinite(step.time_ratio) ||
        !valid_body(bodies, velocities) || !valid_body(bodies + 1, velocities + 1) ||
        (step.warm_start && !finite_impulse(state->impulse))) {
        return ab_fail(error, "Invalid weld definition/body/step/storage");
    }
    AbWeldState following = {0};
    for (size_t body = 0; body < 2; ++body) {
        following.local_centers[body] = bodies[body].local_center;
        following.inverse_mass[body] = bodies[body].inverse_mass;
        following.inverse_inertia[body] = bodies[body].inverse_inertia;
        if (!ab_joint_solver_offset(definition->local_anchors[body], bodies + body,
                                    following.offsets + body)) {
            return ab_fail(error, "Unsupported weld rotation or offset overflow");
        }
    }
    if (!prepare_mass(definition, bodies, step, &following, error)) {
        return false;
    }
    AbVelocity updated[2] = {velocities[0], velocities[1]};
    if (step.warm_start &&
        !warm_start(bodies, &following, state->impulse, step.time_ratio, updated)) {
        return ab_fail(error, "Weld warm-start arithmetic overflow");
    }
    *state = following;
    memcpy(velocities, updated, sizeof(updated));
    return true;
}
