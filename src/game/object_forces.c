#include "ab/object_forces.h"

#include "ab/script_math.h"

#include "force_internal.h"

#include <math.h>

bool ab_object_force_at(AbBodyMotion *body, AbVec2 force, AbVec2 point,
                        AbError *error) {
    if (!body) {
        return ab_fail(error, "Missing direct force body");
    }
    if (body->mass.type != AB_BODY_DYNAMIC) {
        return true;
    }
    return apply_force_at(body, force, point, error);
}

static bool configured_force(const AbBodyMotion *body, const AbObjectForces *settings,
                             AbVec2 *output) {
    if (settings->relative_present) {
        if (!isfinite(settings->relative_force)) {
            return false;
        }
        if (fabsf(settings->relative_force) > 0) {
            return ab_script_polar(body->mass.angle, settings->relative_force, output);
        }
    }
    if (!isfinite(settings->object_mass) ||
        (settings->force_x_present && !isfinite(settings->force.x)) ||
        (settings->force_y_present && !isfinite(settings->force.y))) {
        return false;
    }
    *output = (AbVec2){
        settings->force_x_present ? settings->object_mass * settings->force.x : 0,
        settings->force_y_present ? settings->force.y * settings->object_mass : 0};
    return isfinite(output->x) && isfinite(output->y);
}

static bool limit_force(const AbBodyMotion *body, const AbObjectForces *settings,
                        AbVec2 *force) {
    const AbVec2 velocity = body->mass.linear_velocity;
    if (!isfinite(settings->target_velocity) || !isfinite(velocity.x) ||
        !isfinite(velocity.y)) {
        return false;
    }
    if (settings->target_velocity <= fabsf(velocity.x) && velocity.x * force->x >= 0) {
        force->x = 0;
    }
    if (settings->target_velocity <= fabsf(velocity.y) && force->y * velocity.y >= 0) {
        force->y = 0;
    }
    return true;
}

bool ab_object_forces_apply(AbBodyMotion *body, const AbObjectForces *settings,
                            AbError *error) {
    if (!body || !settings || (unsigned)body->mass.type > AB_BODY_DYNAMIC) {
        return ab_fail(error, "Invalid ordinary object force state");
    }
    if (body->mass.type != AB_BODY_DYNAMIC) {
        return true;
    }
    AbBodyMotion next = *body;
    if (!settings->gravity_enabled) {
        if (!isfinite(body->mass.mass) || !isfinite(settings->gravity.x) ||
            !isfinite(settings->gravity.y)) {
            return ab_fail(error, "Invalid object gravity cancellation");
        }
        AbVec2 gravity = {-(settings->gravity.x * body->mass.mass),
                          -(settings->gravity.y * body->mass.mass)};
        if (!apply_center_force(&next, gravity, error)) {
            return false;
        }
    }
    if (!settings->disabled) {
        AbVec2 force;
        if (!configured_force(&next, settings, &force)) {
            return ab_fail(error, "Invalid configured object force");
        }
        if (force.x != 0 || force.y != 0) {
            if (settings->target_present && !limit_force(&next, settings, &force)) {
                return ab_fail(error, "Invalid object force velocity limit");
            }
            if (settings->impulse) {
                if (!ab_body_impulse_apply(&next, force, &next.mass.center)) {
                    return ab_fail(error, "Invalid object force pulse impulse");
                }
            } else if (!apply_center_force(&next, force, error)) {
                return false;
            }
        }
    }
    *body = next;
    return true;
}
