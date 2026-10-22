#ifndef AB_BIRD_COLLISION_SPRITES_H
#define AB_BIRD_COLLISION_SPRITES_H

#include "ab/bird_collision_notify.h"

typedef struct {
    AbDataValue specialty, sprite_collision;
    AbDataValue on_flying_bird_collided, specialty_available_after_collision;
    AbDataValue explosion_timer, sprite_special1;
} AbBirdCollisionSpriteDefinition;

typedef struct {
    AbDataValue bird_specialty_available;
    AbBirdCollisionObject *flying_bird;
} AbBirdCollisionSpriteState;

typedef struct {
    void *context;
    bool (*definition)(void *context, AbSpan current_name,
                       AbBirdCollisionSpriteDefinition **output, AbError *error);
    /* Pure lookup of the current world binding; it may differ from the cached
     * contact object after an earlier definition callback. */
    bool (*object)(void *context, AbSpan current_name, AbBirdCollisionObject **output,
                   AbError *error);
    bool (*sprite)(void *context, AbSpan current_name, AbDataValue value,
                   AbError *error);
} AbBirdCollisionSpriteHost;

/* Bird callback instructions 368..499. For each cached object in contact order,
 * require the current SECOND sensor be falsy and that object's shot be truthy.
 * Assign hasCollided=true before querying its current name's definition.
 * A non-nil spriteCollision and falsy dontUpdateDamageSprite enable selection.
 * GRENADE requires current specialty availability and flying-bird pointer
 * identity with the cached object; other specialties require the SECOND
 * object's frozenSolid be falsy. Both sides use these same second-object gates.
 *
 * Assign the selected sprite into the freshly resolved world object, then call
 * setSprite(current_name, current_world_sprite). False and numeric sprite values
 * remain non-nil. Callbacks may change later fields, definitions, world bindings
 * or flying state. Borrow stable plain typed storage and keys, retain cached
 * objects/definitions, and do not reenter. State is required only for GRENADE.
 * No allocation, Lua, visual resource lookup or actual rendering. Missing
 * required hosts/objects reject at their reached boundary, retaining earlier
 * flags, sprite writes and callbacks. The enclosing handler owns later phases.
 */
bool ab_bird_collision_sprites(AbBirdCollisionObject *first,
                               AbBirdCollisionObject *second,
                               AbBirdCollisionSpriteState *state,
                               const AbBirdCollisionSpriteHost *host, AbError *error);

#endif
