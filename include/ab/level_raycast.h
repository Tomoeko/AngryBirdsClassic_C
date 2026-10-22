#ifndef AB_LEVEL_RAYCAST_H
#define AB_LEVEL_RAYCAST_H

#include "ab/level_bodies.h"
#include "ab/world_raycast.h"

typedef struct {
    bool hit;
    AbDataValue name;
    AbVec2 point, normal;
} AbLevelRaycast;

/* GameLua.raycast's unit-length query boundary over stable level bindings.
 * Resolve nonzero fixture user data by identity against the borrowed objects;
 * never dereference arbitrary user data. A zero pointer yields a nil name.
 * An optional removed mask rejects stale bindings, including a live orphan
 * body retained during locked removal. On a miss, only hit is written; the
 * original returns one Boolean and supplies no name/point/normal values.
 * Output commits only on success. All binary32 endpoint values are supported,
 * including the original predictor's NaN rays. World shape contracts apply. No
 * allocation, world mutation, script name lookup or implicit unit conversion. */
bool ab_level_bodies_raycast(const AbPhysicsWorld *world, const AbLevelBodies *bodies,
                             const bool *removed, AbVec2 first, AbVec2 second,
                             AbLevelRaycast *output, AbError *error);

#endif
