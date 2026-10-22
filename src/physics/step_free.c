#include "ab/physics_step.h"

#include "ab/rotation.h"
#include "internal.h"
#include "step_internal.h"

#include <float.h>
#include <math.h>

void ab_step_apply_forces(AbBodyMotion *body, const AbFreeIslandStep *step) {
    AbBodyMassState *mass = &body->mass;
    mass->previous_center = mass->center;
    mass->previous_angle = mass->angle;
    if (mass->type != AB_BODY_DYNAMIC) {
        return;
    }
    float linear_factor = 1.0f / (1.0f + step->seconds * body->linear_damping);
    float angular_factor = 1.0f / (1.0f + step->seconds * body->angular_damping);
    AbVec2 acceleration = {mass->inverse_mass * body->force.x,
                           mass->inverse_mass * body->force.y};
    acceleration.x = acceleration.x + body->gravity_scale * step->gravity.x;
    acceleration.y = acceleration.y + body->gravity_scale * step->gravity.y;
    float angular_factor_force = step->seconds * mass->inverse_inertia;
    mass->linear_velocity.x =
        linear_factor * (mass->linear_velocity.x + step->seconds * acceleration.x);
    mass->linear_velocity.y =
        linear_factor * (mass->linear_velocity.y + step->seconds * acceleration.y);
    mass->angular_velocity =
        angular_factor * (mass->angular_velocity + angular_factor_force * body->torque);
}

void ab_step_move_body(AbBodyMotion *body, const AbFreeIslandStep *step) {
    AbBodyMassState *mass = &body->mass;
    AbVec2 translation = {step->seconds * mass->linear_velocity.x,
                          step->seconds * mass->linear_velocity.y};
    float distance_squared = translation.y * translation.y;
    distance_squared = distance_squared + translation.x * translation.x;
    if (distance_squared > step->maximum_translation_squared) {
        float ratio = step->maximum_translation / sqrtf(distance_squared);
        mass->linear_velocity.x = mass->linear_velocity.x * ratio;
        mass->linear_velocity.y = mass->linear_velocity.y * ratio;
        translation.x = step->seconds * mass->linear_velocity.x;
        translation.y = step->seconds * mass->linear_velocity.y;
    }
    float rotation = step->seconds * mass->angular_velocity;
    if (rotation * rotation > 0x1.3bd3cep+1f) {
        float ratio = 0x1.921fb6p+0f / fabsf(rotation);
        mass->angular_velocity = mass->angular_velocity * ratio;
        rotation = step->seconds * mass->angular_velocity;
    }
    mass->center.x = mass->center.x + translation.x;
    mass->center.y = mass->center.y + translation.y;
    mass->angle = mass->angle + rotation;
}

bool ab_step_sync_body(AbBodyMotion *body) {
    AbBodyMassState *mass = &body->mass;
    AbRotation orientation;
    if (!ab_rotation_from_angle(mass->angle, &orientation)) {
        return false;
    }
    mass->transform.sine = orientation.sine;
    mass->transform.cosine = orientation.cosine;
    AbVec2 offset = ab_physics_rotate(mass->local_center, mass->transform);
    mass->transform.position =
        (AbVec2){mass->center.x - offset.x, mass->center.y - offset.y};
    return true;
}

void ab_step_sleep(AbBodyMotion *bodies, size_t count, const AbFreeIslandStep *step,
                   bool position_solved) {
    float minimum_time = FLT_MAX;
    for (size_t index = 0; index < count; ++index) {
        AbBodyMotion *body = bodies + index;
        if (body->mass.type == AB_BODY_STATIC) {
            continue;
        }
        float angular = body->mass.angular_velocity;
        AbVec2 linear = body->mass.linear_velocity;
        float linear_squared = linear.y * linear.y + linear.x * linear.x;
        if (!body->sleep_allowed || angular * angular > 0x1.3f6a22p-10f ||
            linear_squared > 0x1.47ae16p-7f) {
            body->sleep_time = 0;
            minimum_time = 0;
        } else {
            body->sleep_time = step->seconds + body->sleep_time;
            if (body->sleep_time <= minimum_time) {
                minimum_time = body->sleep_time;
            }
        }
    }
    if (minimum_time < 0.25f || !position_solved) {
        return;
    }
    for (size_t index = 0; index < count; ++index) {
        AbBodyMotion *body = bodies + index;
        body->sleep_time = 0;
        body->awake = false;
        body->mass.linear_velocity = (AbVec2){0, 0};
        body->mass.angular_velocity = 0;
        body->force = (AbVec2){0, 0};
        body->torque = 0;
    }
}

bool ab_step_controls_valid(const AbBodyMotion *bodies, size_t count,
                            const AbFreeIslandStep *step) {
    if (!step || (count && !bodies) || !isfinite(step->seconds) || step->seconds < 0 ||
        !isfinite(step->gravity.x) || !isfinite(step->gravity.y) ||
        !isfinite(step->maximum_translation) || step->maximum_translation < 0 ||
        !isfinite(step->maximum_translation_squared) ||
        step->maximum_translation_squared < 0) {
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        if (bodies[index].mass.type != AB_BODY_STATIC &&
            bodies[index].mass.type != AB_BODY_KINEMATIC &&
            bodies[index].mass.type != AB_BODY_DYNAMIC) {
            return false;
        }
    }
    return true;
}

bool ab_island_step_free(AbBodyMotion *bodies, size_t count,
                         const AbFreeIslandStep *step) {
    if (!ab_step_controls_valid(bodies, count, step)) {
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        ab_step_apply_forces(bodies + index, step);
    }
    for (size_t index = 0; index < count; ++index) {
        ab_step_move_body(bodies + index, step);
        if (!ab_step_sync_body(bodies + index)) {
            return false;
        }
    }
    if (step->allow_sleep) {
        ab_step_sleep(bodies, count, step, step->position_iterations != 0);
    }
    return true;
}
