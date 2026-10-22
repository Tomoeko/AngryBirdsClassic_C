#ifndef AB_PHYSICS_INTERNAL_H
#define AB_PHYSICS_INTERNAL_H

#include "ab/physics_geometry.h"

static inline AbVec2 ab_physics_rotate(AbVec2 point, AbRigidTransform transform) {
    /* VNMLS negates the accumulator before adding the rounded product. Writing
     * the equivalent subtraction preserves +0 for equal positive products.
     */
    float x = transform.cosine * point.x - transform.sine * point.y;
    float y = transform.cosine * point.y + point.x * transform.sine;
    return (AbVec2){x, y};
}

static inline AbVec2 ab_physics_point(AbVec2 point, AbRigidTransform transform) {
    AbVec2 rotated = ab_physics_rotate(point, transform);
    return (AbVec2){rotated.x + transform.position.x, rotated.y + transform.position.y};
}

static inline AbVec2 ab_physics_inverse(AbVec2 point, AbRigidTransform transform) {
    float dy = point.y - transform.position.y;
    float dx = point.x - transform.position.x;
    return (AbVec2){dy * transform.sine + dx * transform.cosine,
                    dy * transform.cosine - transform.sine * dx};
}

#endif
