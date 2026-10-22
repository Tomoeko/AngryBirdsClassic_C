#ifndef AB_PHYSICS_CONTACT_H
#define AB_PHYSICS_CONTACT_H

#include "ab/physics_geometry.h"

#include <stdint.h>

typedef enum {
    AB_CONTACT_CIRCLES,
    AB_CONTACT_FACE_FIRST,
    AB_CONTACT_FACE_SECOND,
} AbContactKind;

typedef struct {
    AbVec2 local_point;
    float normal_impulse;
    float tangent_impulse;
    uint32_t feature_id;
} AbContactPoint;

typedef struct {
    AbContactPoint points[2];
    AbVec2 local_normal;
    AbVec2 local_point;
    AbContactKind kind;
    uint32_t point_count;
} AbManifold;

typedef struct {
    AbVec2 normal;
    AbVec2 points[2];
    float separations[2];
} AbWorldManifold;

/* Geometry contracts from physics_geometry.h apply. Output must be initialized.
 * Queries set point_count, write active geometric fields, and preserve impulses
 * and inactive fields. A zero count makes the other geometric fields inactive.
 * Warm starting, world-space manifolds, and solver integration are separate.
 */
void ab_collide_circles(AbManifold *output, AbCircle first,
                        AbRigidTransform first_transform, AbCircle second,
                        AbRigidTransform second_transform);
void ab_collide_polygon_circle(AbManifold *output, const AbPolygon *polygon,
                               AbRigidTransform polygon_transform, AbCircle circle,
                               AbRigidTransform circle_transform);
void ab_collide_polygons(AbManifold *output, const AbPolygon *first,
                         AbRigidTransform first_transform, const AbPolygon *second,
                         AbRigidTransform second_transform);
/* Preserve output when point_count is zero; otherwise update the normal and
 * active points/separations. The normal points from the first body to the second.
 */
void ab_world_manifold(AbWorldManifold *output, const AbManifold *manifold,
                       AbRigidTransform first_transform, float first_radius,
                       AbRigidTransform second_transform, float second_radius);

#endif
