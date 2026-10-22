#ifndef AB_GAME_FORCE_INTERNAL_H
#define AB_GAME_FORCE_INTERNAL_H

#include "ab/base.h"
#include "ab/physics_step.h"

#include <math.h>

/* Keep the native subtraction/product order, including signed-zero torque.
 * Callers have already selected a dynamic body. */
static bool apply_force_at(AbBodyMotion *body, AbVec2 force, AbVec2 point,
                           AbError *error) {
    if (!isfinite(body->mass.center.x) || !isfinite(body->mass.center.y) ||
        !isfinite(body->force.x) || !isfinite(body->force.y) ||
        !isfinite(body->torque) || !isfinite(force.x) || !isfinite(force.y) ||
        !isfinite(point.x) || !isfinite(point.y)) {
        return ab_fail(error, "Invalid accumulated object force");
    }
    AbVec2 offset = {point.x - body->mass.center.x, point.y - body->mass.center.y};
    float torque = force.x * offset.y;
    torque = -torque + force.y * offset.x;
    torque = body->torque + torque;
    force = (AbVec2){body->force.x + force.x, body->force.y + force.y};
    if (!isfinite(force.x) || !isfinite(force.y) || !isfinite(torque)) {
        return ab_fail(error, "Object force exceeds finite range");
    }
    if (!body->awake) {
        body->awake = true;
        body->sleep_time = 0;
    }
    body->force = force;
    body->torque = torque;
    return true;
}

static bool apply_center_force(AbBodyMotion *body, AbVec2 force, AbError *error) {
    return apply_force_at(body, force, body->mass.center, error);
}

#endif
