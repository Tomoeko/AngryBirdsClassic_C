#ifndef AB_BIRD_COLLISION_DAMAGE_H
#define AB_BIRD_COLLISION_DAMAGE_H

#include "ab/bird_collision_notify.h"

typedef struct {
    void *context;
    AbDataValue block_table;
    bool (*sprites)(void *context, AbBirdCollisionObject *object, AbDataValue blocks,
                    AbDamageSpriteNames **output, AbError *error);
    bool (*particles)(void *context, AbBirdCollisionObject *object, AbError *error);
} AbBirdCollisionDamageHost;

/* Final bird callback instructions 947..973. A truthy second sensor or literal
 * Boolean-true controllable skips. Otherwise query getDamageSprite for the cached
 * second object and current blockTable.blocks, even with zero contact damage.
 * Compare its returned sprite against current damage_sprite after the host call.
 * A change assigns damage/blink/smile fields before the damage-particle helper.
 * No setSprite or damage audio occurs in this phase.
 *
 * Borrow stable plain object/catalog/descriptor storage. Callbacks may change
 * fields and later bindings; this phase does not repeat its initial gates after
 * the query. Bind particles to the first-party damage-state helper. No allocation,
 * Lua, rendering or particle simulation. Used table metadata must be valid;
 * missing required hosts/descriptors reject at their reached boundary, retaining
 * earlier writes and callbacks. Scalar numeric equality preserves IEEE behavior,
 * including equal signed zeros and unequal NaNs. Do not reenter/release storage.
 */
bool ab_bird_collision_damage_refresh(AbBirdCollisionObject *second,
                                      AbBirdCollisionDamageHost *host, AbError *error);

#endif
