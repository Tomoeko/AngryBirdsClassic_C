#ifndef AB_PHYSICS_GEOMETRY_H
#define AB_PHYSICS_GEOMETRY_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    float x;
    float y;
} AbVec2;

typedef struct {
    float x, y, z;
} AbVec3;

typedef struct {
    AbVec2 position;
    float sine;
    float cosine;
} AbRigidTransform;

typedef struct {
    float radius;
    AbVec2 center;
} AbCircle;

typedef struct {
    float mass;
    AbVec2 center;
    float inertia;
} AbMass;

typedef struct {
    AbVec2 lower;
    AbVec2 upper;
} AbAabb;

enum { AB_POLYGON_VERTICES = 8 };

typedef struct {
    AbVec2 vertices[AB_POLYGON_VERTICES];
    AbVec2 normals[AB_POLYGON_VERTICES];
    size_t count;
    float radius;
    AbVec2 centroid;
} AbPolygon;

typedef struct {
    AbVec2 first;
    AbVec2 second;
    float max_fraction;
} AbRay;

typedef struct {
    AbVec2 normal;
    float fraction;
} AbRayHit;

/* Pure binary32 geometry. Callers validate finite inputs and nonnegative radius
 * and density. Rotation components are supplied without implicit normalization.
 * Use round-to-nearest with gradual underflow and compile without contraction.
 * Operation order follows the pinned ARMv7 routines. Box half-extents are positive.
 */
AbMass ab_circle_mass(AbCircle circle, float density);
AbAabb ab_circle_bounds(AbCircle circle, AbRigidTransform transform);
bool ab_circle_contains(AbCircle circle, AbRigidTransform transform, AbVec2 point);

/* Polygon queries require 3..8 counterclockwise convex vertices with outward
 * unit normals. Mass requires positive area. The skin radius expands bounds,
 * while point containment and mass use the original polygon edges.
 */
AbPolygon ab_polygon_box(float half_width, float half_height, float radius);
/* The target uses only the first eight points, retains the first welded point,
 * and falls back to a unit-half-extent box for a hull with fewer than 3 vertices.
 * Invalid arguments or nonfinite arithmetic fail with a cleared output.
 */
bool ab_polygon_build(const AbVec2 *vertices, size_t count, float radius,
                      AbPolygon *output);
AbMass ab_polygon_mass(const AbPolygon *polygon, float density);
AbAabb ab_polygon_bounds(const AbPolygon *polygon, AbRigidTransform transform);
bool ab_polygon_contains(const AbPolygon *polygon, AbRigidTransform transform,
                         AbVec2 point);
/* Segment queries preserve output on a miss and exclude an exit from an
 * interior starting point. max_fraction is finite and nonnegative.
 */
bool ab_circle_raycast(AbCircle circle, AbRigidTransform transform, AbRay ray,
                       AbRayHit *output);
bool ab_polygon_raycast(const AbPolygon *polygon, AbRigidTransform transform, AbRay ray,
                        AbRayHit *output);

#endif
