#ifndef AB_PHYSICS_DISTANCE_H
#define AB_PHYSICS_DISTANCE_H

#include "ab/physics_geometry.h"

#include <stdint.h>

typedef struct {
    const AbVec2 *vertices;
    size_t count;
    float radius;
} AbDistanceProxy;

typedef struct {
    float metric;
    uint16_t count;
    uint8_t first[3];
    uint8_t second[3];
} AbDistanceCache;

typedef struct {
    AbVec2 first;
    AbVec2 second;
    float distance;
    uint32_t iterations;
} AbDistanceResult;

/* Convex support queries over 1..8 borrowed vertices with nonnegative radii.
 * A zero cache count starts cold; otherwise active indices must fit each proxy.
 * Preserve inactive cache indices. Numeric geometry contracts apply. Arrays,
 * cache and output are disjoint. Invalid arguments/nonfinite results preserve
 * cache and output. The bounded query performs at most 20 support iterations,
 * uses supplied transforms without normalization, and allocates no memory.
 */
bool ab_shape_distance(AbDistanceProxy first, AbRigidTransform first_transform,
                       AbDistanceProxy second, AbRigidTransform second_transform,
                       bool use_radii, AbDistanceCache *cache,
                       AbDistanceResult *output);

/* Cold distance query with radii; overlap uses the target's strict tolerance.
 * A failed query preserves overlap. Other distance contracts apply.
 */
bool ab_shape_overlap(AbDistanceProxy first, AbRigidTransform first_transform,
                      AbDistanceProxy second, AbRigidTransform second_transform,
                      bool *overlap);

#endif
