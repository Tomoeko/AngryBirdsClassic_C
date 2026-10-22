#include "ab/physics_step.h"

#include <math.h>

static bool finite_vector(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static void wake_body(AbBodyMotion *body) {
    if (!body->awake) {
        body->awake = true;
        body->sleep_time = 0;
    }
}

bool ab_body_velocity_set(AbBodyMotion *body, AbVec2 velocity) {
    if (!body || (unsigned)body->mass.type > AB_BODY_DYNAMIC ||
        !finite_vector(velocity)) {
        return false;
    }
    if (body->mass.type != AB_BODY_STATIC) {
        float squared = velocity.y * velocity.y + velocity.x * velocity.x;
        if (squared > 0) {
            wake_body(body);
        }
        body->mass.linear_velocity = velocity;
    }
    return true;
}

bool ab_body_angular_velocity_set(AbBodyMotion *body, float velocity) {
    if (!body || (unsigned)body->mass.type > AB_BODY_DYNAMIC || !isfinite(velocity)) {
        return false;
    }
    if (body->mass.type != AB_BODY_STATIC) {
        if (velocity * velocity > 0) {
            wake_body(body);
        }
        body->mass.angular_velocity = velocity;
    }
    return true;
}

bool ab_body_impulse_apply(AbBodyMotion *body, AbVec2 impulse, const AbVec2 *point) {
    if (!body || (unsigned)body->mass.type > AB_BODY_DYNAMIC ||
        !finite_vector(impulse) || (point && !finite_vector(*point))) {
        return false;
    }
    if (body->mass.type != AB_BODY_DYNAMIC) {
        return true;
    }
    const AbBodyMassState *mass = &body->mass;
    AbVec2 position = point ? *point : mass->transform.position;
    if (!finite_vector(position) || !finite_vector(mass->center) ||
        !finite_vector(mass->linear_velocity) || !isfinite(mass->angular_velocity) ||
        !isfinite(mass->inverse_mass) || mass->inverse_mass < 0 ||
        !isfinite(mass->inverse_inertia) || mass->inverse_inertia < 0) {
        return false;
    }
    AbVec2 offset = {position.x - mass->center.x, position.y - mass->center.y};
    /* Native VNMLS negates the accumulated product before adding the next
     * product. Negating a completed subtraction changes signed-zero results. */
    float moment = impulse.x * offset.y;
    moment = -moment + impulse.y * offset.x;
    AbVec2 velocity = {mass->linear_velocity.x + impulse.x * mass->inverse_mass,
                       mass->linear_velocity.y + impulse.y * mass->inverse_mass};
    float angular = mass->angular_velocity + moment * mass->inverse_inertia;
    if (!finite_vector(velocity) || !isfinite(angular)) {
        return false;
    }
    wake_body(body);
    body->mass.linear_velocity = velocity;
    body->mass.angular_velocity = angular;
    return true;
}
