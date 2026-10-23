#ifndef AB_GAME_BOX_H
#define AB_GAME_BOX_H

#include "ab/game_frame.h"
#include "ab/level_properties.h"

typedef struct {
    AbSpan name;
    AbVec2 position, size;
    float density, friction, restitution, z_order;
    bool collision;
} AbGameBoxInput;

typedef struct {
    AbObjectBody physics;
    AbGameFrameObject frame;
    AbLevelPropertyObject properties;
    float initial_mass;
} AbGameBox;

/* Native GameLua.createBoxLua before renderer/script/definition assignments.
 * Reuse recovered box fixture construction and publish the raw physical table.
 * Dimensions must be positive, other material values finite/nonnegative. The
 * ignored sprite/controllable parameters have no inputs here. The native box
 * constructor does not register a controllable-vector entry. The caller owns stable
 * name and output storage, object-map/vector insertion and later adapter registration.
 * No allocation beyond existing bounded world growth. Output commits only on
 * success; a late failure retains preceding world changes. */
bool ab_game_box_create(AbPhysicsWorld *world, AbGameBoxInput input,
                        uintptr_t fixture_user_data, AbGameBox *output, AbError *error);

#endif
