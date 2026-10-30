#ifndef AB_GAME_CIRCLE_H
#define AB_GAME_CIRCLE_H

#include "ab/game_frame.h"
#include "ab/level_properties.h"

typedef struct {
    AbSpan name;
    AbVec2 position;
    float radius, density, friction, restitution, z_order;
    bool controllable;
} AbGameCircleInput;

typedef struct {
    AbObjectBody physics;
    AbGameFrameObject frame;
    AbLevelPropertyObject properties;
    float initial_mass;
} AbGameCircle;

/* Native GameLua.createCircleLua, before renderer/script/clone assignments.
 * Create the actual circle body and fixture and publish its raw physical table.
 * The source's sprite argument is unused here: the raw sprite starts empty;
 * createRendererForGameObject and setSprite are separate operations. Keep name
 * bytes and output at stable addresses while borrowed. fixture_user_data is the
 * caller's stable physical object identity, as in object_body.h.
 *
 * No definition, GameObject script, birds/dynamic membership, events or queue
 * initialization is invented. The parent owns native object-map/vector insertion
 * and level slot registration. No allocation beyond bounded world growth. Inputs
 * validate before creation. A later failure retains world effects; discard the
 * session. Output may not overlap world storage and is published only on success.
 */
bool ab_game_circle_create(AbPhysicsWorld *world, AbGameCircleInput input,
                           uintptr_t fixture_user_data, AbGameCircle *output,
                           AbError *error);

/* GameLua.resizeCircle32195c: recreate the fixture, then publish radius and
 * current body mass in the same physical table. Refresh initial_mass to the
 * mass of this latest fixture construction. Borrowed table identity survives. */
bool ab_game_circle_resize(AbPhysicsWorld *world, AbGameCircle *circle, float radius,
                           AbError *error);

#endif
