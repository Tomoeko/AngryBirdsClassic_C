#include "ab/object_friction.h"

#include "force_internal.h"

#include <math.h>

static bool finite_vector(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static bool relative_projection(const AbBodyMotion *body, AbVec2 coefficients,
                                float *output) {
    const AbRigidTransform *transform = &body->mass.transform;
    if (!finite_vector(coefficients) || !isfinite(transform->sine) ||
        !isfinite(transform->cosine)) {
        return false;
    }
    float negative_sine = -transform->sine;
    float determinant = transform->sine * negative_sine;
    determinant = -determinant + transform->cosine * transform->cosine;
    if (determinant != 0) {
        determinant = 1 / determinant;
    }
    float vertical = coefficients.x * transform->sine;
    vertical = -vertical + coefficients.y * transform->cosine;
    float horizontal = coefficients.y * negative_sine;
    horizontal = -horizontal + coefficients.x * transform->cosine;
    vertical = determinant * vertical;
    horizontal = horizontal * determinant;
    float projection = body->mass.linear_velocity.y * vertical;
    projection = projection + body->mass.linear_velocity.x * horizontal;
    if (!isfinite(projection)) {
        return false;
    }
    *output = projection;
    return true;
}

bool ab_object_friction_apply(AbBodyMotion *body, const AbObjectFriction *settings,
                              AbError *error) {
    if (!body || !settings || (unsigned)body->mass.type > AB_BODY_DYNAMIC ||
        !isfinite(settings->air_resistance)) {
        return ab_fail(error, "Invalid object friction state");
    }
    if (body->mass.type != AB_BODY_DYNAMIC) {
        return true;
    }
    if (settings->air_resistance == 0) {
        if (!finite_vector(settings->relative)) {
            return ab_fail(error, "Invalid relative object friction");
        }
        if (settings->relative.x == 0 && settings->relative.y == 0) {
            return true;
        }
    }
    if (!finite_vector(body->mass.linear_velocity)) {
        return ab_fail(error, "Invalid object friction velocity");
    }
    float factor;
    if (settings->air_resistance != 0) {
        factor = -settings->air_resistance;
    } else {
        if (!relative_projection(body, settings->relative, &factor)) {
            return ab_fail(error, "Object friction projection exceeds finite range");
        }
        if (!(factor < 0)) {
            return true;
        }
    }
    const AbVec2 velocity = body->mass.linear_velocity;
    AbVec2 force = {velocity.x * factor, factor * velocity.y};
    return apply_center_force(body, force, error);
}
