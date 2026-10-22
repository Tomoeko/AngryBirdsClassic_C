#ifndef AB_BIRD_COLLISION_PROJECTILES_H
#define AB_BIRD_COLLISION_PROJECTILES_H

#include "ab/bird_collision_sprites.h"

typedef struct {
    AbSpan first, second;
    AbBirdCollisionObject *first_object, *second_object;
} AbBirdCollisionProjectileInput;

typedef struct {
    AbBirdCollisionObject **items;
    size_t count, capacity;
    bool present;
} AbBirdCollisionGrenades;

typedef struct {
    void *context;
    bool (*definition)(void *context, AbSpan current_name,
                       AbBirdCollisionSpriteDefinition **output, AbError *error);
} AbBirdCollisionProjectileHost;

typedef struct {
    bool first_present, return_early;
} AbBirdCollisionProjectileResult;

/* Bird callback instructions 737..826. Both boomerang checks and both collision
 * flag checks use the current second sensor gate. Truthy controllable requests
 * a definition and clears boomerangActive only for specialty BOOMERANG. Then
 * truthy shot+controllable assigns collision=true for each eligible contact.
 *
 * Read the current dense flyingGrenades length after definition callbacks and
 * traverse backward. A matching grenade explodes unless the opposite contact's
 * controllable is Boolean true. An exploded first name becomes locally absent;
 * the second name stays present. A still-matching first/second name with a
 * Boolean-true opposite controller returns from the enclosing bird callback.
 * Earlier queued explosions survive that return. This component does not
 * remove a list item or alter either cached object's name.
 *
 * Borrow stable plain objects, non-nil string names and bounded dense list
 * storage. Aliased entries are allowed. Definition callbacks may mutate later
 * fields/list state; do not reenter or release cached storage. count is the
 * measured dense length, limited to 2^24, and present=false models a missing
 * table. No allocation, Lua, arbitrary table-length semantics or explosion
 * physics. Output commits only on success; preceding fields/callbacks survive
 * failure. The caller carries first_present into later contact effects and
 * skips the enclosing callback's remaining work when return_early is true.
 */
bool ab_bird_collision_projectiles(const AbBirdCollisionProjectileInput *input,
                                   AbBirdCollisionGrenades *grenades,
                                   const AbBirdCollisionProjectileHost *host,
                                   AbBirdCollisionProjectileResult *output,
                                   AbError *error);

#endif
