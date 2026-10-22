#ifndef AB_OBJECT_TRANSFORM_H
#define AB_OBJECT_TRANSFORM_H

#include "ab/game_frame.h"

/* Resolved GameLua::setPosition/setRotation on an existing script object.
 * A missing body succeeds without using the requested numeric values. Otherwise
 * update the physics transform first, then the script field, rendered component
 * and both history components. Position preserves all cached angles; rotation
 * preserves all cached positions and normalizes only the requested angle.
 * Body type, velocities and awake state are retained. The caller resolves the
 * name, owns script fields and maintains frame/body membership. World must be
 * healthy/unlocked, and used numbers finite. No coordinate conversion or
 * allocation. Initial validation preserves state; later world failure can retain
 * partial physics changes. Script/frame storage must be disjoint from the world.
 */
bool ab_object_position_set(AbPhysicsWorld *world, AbGameFrameObject *frame,
                            AbVec2 *script_position, AbVec2 position, AbError *error);
bool ab_object_rotation_set(AbPhysicsWorld *world, AbGameFrameObject *frame,
                            float *script_angle, float angle, AbError *error);

#endif
