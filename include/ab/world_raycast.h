#ifndef AB_WORLD_RAYCAST_H
#define AB_WORLD_RAYCAST_H

#include "ab/physics_world.h"

typedef struct {
    size_t fixture, body;
    AbVec2 point, normal;
    float fraction;
} AbWorldRaycastHit;

typedef bool (*AbWorldRaycastCallback)(void *context, const AbWorldRaycastHit *hit,
                                       float *next_fraction, AbError *error);

/* Native world ray traversal over live circle/polygon proxies, including sensors
 * without collision-filter tests. Hit callbacks use weighted endpoint arithmetic
 * and the original tree order/clipping policy. Return zero to stop, a positive
 * fraction to replace the limit, or negative/NaN to ignore clipping. Endpoints
 * may contain any binary32 value; shapes/transforms remain valid world inputs
 * and the initial fraction is finite and nonnegative. No allocation or world mutation;
 * callbacks must not reenter or modify the world/tree. Earlier callbacks survive
 * failure; optional final_fraction commits only on success. */
bool ab_world_raycast(const AbPhysicsWorld *world, AbRay ray,
                      AbWorldRaycastCallback callback, void *context,
                      float *final_fraction, AbError *error);

/* Original RaycastFindNearestCallback: retain every reported fixture and clip
 * to its fraction. A later equal-fraction hit replaces the earlier one; a zero
 * fraction stops immediately. On success found is written, and output commits
 * only when a hit exists. Missing-hit point/normal storage remains untouched.
 * Output pointers are disjoint from each other and borrowed world storage. */
bool ab_world_raycast_nearest(const AbPhysicsWorld *world, AbRay ray,
                              AbWorldRaycastHit *output, bool *found, AbError *error);

#endif
