#include "ab/physics_distance_joint.h"

#include "joint_solver_internal.h"

#include <math.h>
#include <string.h>

static bool finite_point(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static bool finite_velocity(AbVelocity value) {
    return finite_point(value.linear) && isfinite(value.angular);
}

static bool valid_masses(const AbDistanceState *state) {
    for (size_t body = 0; body < 2; ++body) {
        if (!isfinite(state->inverse_mass[body]) || state->inverse_mass[body] < 0 ||
            !isfinite(state->inverse_inertia[body]) ||
            state->inverse_inertia[body] < 0) {
            return false;
        }
    }
    return isfinite(state->mass);
}

static float cross(AbVec2 offset, AbVec2 impulse) {
    float result = offset.y * impulse.x;
    result = -result;
    result += offset.x * impulse.y;
    return result;
}

static bool apply_velocity(const AbDistanceState *state, float impulse,
                           AbVelocity velocities[2]) {
    AbVec2 linear = {impulse * state->axis.x, impulse * state->axis.y};
    for (size_t body = 0; body < 2; ++body) {
        float angular = cross(state->offsets[body], linear);
        angular *= state->inverse_inertia[body];
        float x = state->inverse_mass[body] * linear.x;
        float y = state->inverse_mass[body] * linear.y;
        if (body == 0) {
            velocities[body].linear.x -= x;
            velocities[body].linear.y -= y;
            velocities[body].angular -= angular;
        } else {
            velocities[body].linear.x += x;
            velocities[body].linear.y += y;
            velocities[body].angular += angular;
        }
        if (!finite_velocity(velocities[body])) {
            return false;
        }
    }
    return true;
}

static bool prepare_mass(const AbDistanceDefinition *definition,
                         const AbVec2 centers[2], AbJointStep step,
                         AbDistanceState *state) {
    float y = centers[1].y + state->offsets[1].y;
    y -= centers[0].y;
    y -= state->offsets[0].y;
    float x = centers[1].x + state->offsets[1].x;
    x -= centers[0].x;
    x -= state->offsets[0].x;
    float squared = y * y;
    squared += x * x;
    float length = sqrtf(squared);
    if (!isfinite(length)) {
        return false;
    }
    if (length > 0.05f) {
        float inverse = 1.0f / length;
        state->axis = (AbVec2){inverse * x, inverse * y};
    } else {
        state->axis = (AbVec2){0, 0};
    }
    float first = cross(state->offsets[0], state->axis);
    float second = cross(state->offsets[1], state->axis);
    float inverse_mass = state->inverse_mass[0];
    inverse_mass += (state->inverse_inertia[0] * first) * first;
    inverse_mass += state->inverse_mass[1];
    inverse_mass += (state->inverse_inertia[1] * second) * second;
    if (!isfinite(inverse_mass)) {
        return false;
    }
    state->mass = inverse_mass != 0 ? 1.0f / inverse_mass : 0;
    if (definition->frequency > 0) {
        float omega = definition->frequency * 0x1.921fb6p+2f;
        float stiffness = state->mass * omega;
        stiffness *= omega;
        float damping = state->mass + state->mass;
        damping *= definition->damping_ratio;
        damping *= omega;
        damping += step.seconds * stiffness;
        damping *= step.seconds;
        state->gamma = damping != 0 ? 1.0f / damping : 0;
        float denominator = state->gamma + inverse_mass;
        state->mass = denominator != 0 ? 1.0f / denominator : 0;
        float bias = length - definition->length;
        bias *= step.seconds;
        bias *= stiffness;
        state->bias = state->gamma * bias;
    }
    return isfinite(state->mass) && isfinite(state->gamma) && isfinite(state->bias);
}

bool ab_distance_velocity_initialize(const AbDistanceDefinition *definition,
                                     const AbJointSolverBody bodies[2],
                                     const AbVec2 centers[2], AbJointStep step,
                                     AbDistanceState *state, AbVelocity velocities[2],
                                     AbError *error) {
    if (!definition || !bodies || !centers || !state || !velocities ||
        !finite_point(definition->local_anchors[0]) ||
        !finite_point(definition->local_anchors[1]) || !isfinite(definition->length) ||
        definition->length < 0 || !isfinite(definition->frequency) ||
        !isfinite(definition->damping_ratio) || !isfinite(step.seconds) ||
        step.seconds < 0 || !isfinite(step.time_ratio) ||
        (step.warm_start && !isfinite(state->impulse))) {
        return ab_fail(error, "Invalid distance definition/step/storage");
    }
    AbDistanceState following = {0};
    for (size_t body = 0; body < 2; ++body) {
        if (!finite_point(centers[body]) || !finite_point(bodies[body].local_center) ||
            !isfinite(bodies[body].angle) || !finite_velocity(velocities[body])) {
            return ab_fail(error, "Invalid distance body/center/velocity");
        }
        following.local_centers[body] = bodies[body].local_center;
        following.inverse_mass[body] = bodies[body].inverse_mass;
        following.inverse_inertia[body] = bodies[body].inverse_inertia;
        if (!ab_joint_solver_offset(definition->local_anchors[body], bodies + body,
                                    following.offsets + body)) {
            return ab_fail(error, "Unsupported distance rotation or offset overflow");
        }
    }
    if (!valid_masses(&following) ||
        !prepare_mass(definition, centers, step, &following)) {
        return ab_fail(error, "Invalid distance masses or spring overflow");
    }
    AbVelocity updated[2] = {velocities[0], velocities[1]};
    if (step.warm_start) {
        following.impulse = state->impulse * step.time_ratio;
        if (!isfinite(following.impulse) ||
            !apply_velocity(&following, following.impulse, updated)) {
            return ab_fail(error, "Distance warm-start overflow");
        }
    }
    *state = following;
    memcpy(velocities, updated, sizeof(updated));
    return true;
}

bool ab_distance_velocity_solve(AbDistanceState *state, AbVelocity velocities[2],
                                AbError *error) {
    if (!state || !velocities || !valid_masses(state) || !finite_point(state->axis) ||
        !finite_point(state->offsets[0]) || !finite_point(state->offsets[1]) ||
        !isfinite(state->gamma) || !isfinite(state->bias) ||
        !isfinite(state->impulse) || !finite_velocity(velocities[0]) ||
        !finite_velocity(velocities[1])) {
        return ab_fail(error, "Invalid distance velocity constraint/storage");
    }
    AbVec2 relative[2];
    for (size_t body = 0; body < 2; ++body) {
        relative[body].y = velocities[body].linear.y;
        relative[body].y += velocities[body].angular * state->offsets[body].x;
        relative[body].x = velocities[body].linear.x;
        relative[body].x -= velocities[body].angular * state->offsets[body].y;
    }
    float speed = (relative[1].y - relative[0].y) * state->axis.y;
    speed += (relative[1].x - relative[0].x) * state->axis.x;
    speed += state->bias;
    speed += state->impulse * state->gamma;
    float correction = -(state->mass * speed);
    float impulse = state->impulse + correction;
    AbVelocity updated[2] = {velocities[0], velocities[1]};
    if (!isfinite(impulse) || !apply_velocity(state, correction, updated)) {
        return ab_fail(error, "Distance velocity arithmetic overflow");
    }
    state->impulse = impulse;
    memcpy(velocities, updated, sizeof(updated));
    return true;
}

AbPositionResult ab_distance_position_solve(const AbDistanceDefinition *definition,
                                            const AbDistanceState *state,
                                            AbPositionState positions[2],
                                            AbError *error) {
    if (!definition || !state || !positions || !isfinite(definition->frequency)) {
        ab_fail(error, "Invalid distance position definition/storage");
        return AB_POSITION_INVALID;
    }
    if (definition->frequency > 0) {
        return AB_POSITION_RESOLVED;
    }
    if (!valid_masses(state) || !isfinite(definition->length) ||
        definition->length < 0 || !finite_point(definition->local_anchors[0]) ||
        !finite_point(definition->local_anchors[1])) {
        ab_fail(error, "Invalid rigid distance position parameters");
        return AB_POSITION_INVALID;
    }
    AbVec2 offsets[2];
    for (size_t body = 0; body < 2; ++body) {
        AbJointSolverBody frame = {state->local_centers[body], positions[body].angle,
                                   state->inverse_mass[body],
                                   state->inverse_inertia[body]};
        if (!finite_point(positions[body].center) ||
            !finite_point(frame.local_center) ||
            !ab_joint_solver_offset(definition->local_anchors[body], &frame,
                                    offsets + body)) {
            ab_fail(error, "Invalid distance position/rotation or offset overflow");
            return AB_POSITION_INVALID;
        }
    }
    float y = positions[1].center.y + offsets[1].y;
    y -= positions[0].center.y;
    y -= offsets[0].y;
    float x = positions[1].center.x + offsets[1].x;
    x -= positions[0].center.x;
    x -= offsets[0].x;
    float squared = y * y;
    squared += x * x;
    float length = sqrtf(squared);
    if (!isfinite(length)) {
        ab_fail(error, "Distance position length overflow");
        return AB_POSITION_INVALID;
    }
    if (length >= 0x1p-23f) {
        float inverse = 1.0f / length;
        x *= inverse;
        y *= inverse;
    } else {
        length = 0;
    }
    float difference = length - definition->length;
    difference = difference >= 0.2f ? 0.2f : difference >= -0.2f ? difference : -0.2f;
    float impulse = -(state->mass * difference);
    AbVec2 linear = {impulse * x, impulse * y};
    AbPositionState updated[2] = {positions[0], positions[1]};
    for (size_t body = 0; body < 2; ++body) {
        float angular = cross(offsets[body], linear);
        angular *= state->inverse_inertia[body];
        float linear_x = state->inverse_mass[body] * linear.x;
        float linear_y = state->inverse_mass[body] * linear.y;
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
            ab_fail(error, "Distance position correction overflow");
            return AB_POSITION_INVALID;
        }
    }
    memcpy(positions, updated, sizeof(updated));
    float magnitude = difference > 0 ? difference : -difference;
    return magnitude < 0.05f ? AB_POSITION_RESOLVED : AB_POSITION_UNRESOLVED;
}
