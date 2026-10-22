#include "ab/physics_sweep.h"

#include "ab/rotation.h"
#include "internal.h"

#include <math.h>

bool ab_sweep_transform(const AbSweep *sweep, float fraction,
                        AbRigidTransform *output) {
    if (!sweep || !output || !isfinite(fraction) || !isfinite(sweep->local_center.x) ||
        !isfinite(sweep->local_center.y) || !isfinite(sweep->previous_center.x) ||
        !isfinite(sweep->previous_center.y) || !isfinite(sweep->center.x) ||
        !isfinite(sweep->center.y) || !isfinite(sweep->previous_angle) ||
        !isfinite(sweep->angle)) {
        return false;
    }
    float remaining = 1.0f - fraction;
    AbVec2 center = {fraction * sweep->center.x + remaining * sweep->previous_center.x,
                     fraction * sweep->center.y + remaining * sweep->previous_center.y};
    float angle = fraction * sweep->angle + remaining * sweep->previous_angle;
    AbRotation rotation;
    if (!ab_rotation_from_angle(angle, &rotation)) {
        return false;
    }
    AbRigidTransform result = {.sine = rotation.sine, .cosine = rotation.cosine};
    AbVec2 offset = ab_physics_rotate(sweep->local_center, result);
    result.position = (AbVec2){center.x - offset.x, center.y - offset.y};
    if (!isfinite(result.position.x) || !isfinite(result.position.y)) {
        return false;
    }
    *output = result;
    return true;
}

bool ab_sweep_normalize(AbSweep *sweep) {
    if (!sweep || !isfinite(sweep->previous_angle) || !isfinite(sweep->angle)) {
        return false;
    }
    const float full_turn = 0x1.921fb6p+2f;
    float offset = full_turn * floorf(sweep->previous_angle / full_turn);
    float previous = sweep->previous_angle - offset;
    float current = sweep->angle - offset;
    if (!isfinite(previous) || !isfinite(current)) {
        return false;
    }
    sweep->previous_angle = previous;
    sweep->angle = current;
    return true;
}

bool ab_sweep_advance(AbSweep *sweep, float alpha) {
    if (!sweep || !isfinite(alpha) || alpha < 0 || alpha > 1 ||
        !isfinite(sweep->alpha) || sweep->alpha < 0 || sweep->alpha > 1 ||
        !isfinite(sweep->previous_center.x) || !isfinite(sweep->previous_center.y) ||
        !isfinite(sweep->center.x) || !isfinite(sweep->center.y) ||
        !isfinite(sweep->previous_angle) || !isfinite(sweep->angle)) {
        return false;
    }
    if (alpha <= sweep->alpha) {
        return true;
    }
    float fraction = (alpha - sweep->alpha) / (1.0f - sweep->alpha);
    AbVec2 center = {
        sweep->previous_center.x +
            fraction * (sweep->center.x - sweep->previous_center.x),
        sweep->previous_center.y +
            fraction * (sweep->center.y - sweep->previous_center.y),
    };
    float angle =
        sweep->previous_angle + fraction * (sweep->angle - sweep->previous_angle);
    if (!isfinite(center.x) || !isfinite(center.y) || !isfinite(angle)) {
        return false;
    }
    sweep->previous_center = center;
    sweep->previous_angle = angle;
    sweep->alpha = alpha;
    return true;
}
