#ifndef AB_BIRD_COLLISION_MIGHTY_H
#define AB_BIRD_COLLISION_MIGHTY_H

#include "ab/bird_collision_notify.h"

typedef struct {
    AbSpan first, second, flying_name;
    AbBirdCollisionObject *first_object, *second_object;
    bool flying_present;
} AbBirdCollisionMightyInput;

typedef struct {
    void *context;
    /* Pure current world lookup. NULL output denotes an absent object. */
    bool (*object)(void *context, AbSpan name, AbBirdCollisionObject **output,
                   AbError *error);
} AbBirdCollisionMightyHost;

/* Bird callback instructions 301..367: when flyingBird is present, resolve its
 * current world object before comparing incoming names. Only truthy
 * isMightyBait OR isMightyEagle enables this collisionFunction dispatch. Match
 * the flying name to the first/second incoming name and require the opposite
 * object's sensor be falsy. Invoke collisionFunction(flying_name, other_name)
 * without a receiver; an eligible missing/non-callable binding is an error.
 * Script return values have no effect. No shot or controllable gate applies.
 *
 * Continue only a successful prior notification/hat/score prefix. This phase
 * does not set collision flags, sprites or abilities. A missing flyingBird
 * skips all other inputs/hosts. Borrow plain stable names/objects; world lookup
 * is pure, and hosts may mutate state after request arguments are captured.
 * No metatables, allocation or Lua. Failure retains preceding host effects.
 */
bool ab_bird_collision_mighty(const AbBirdCollisionMightyInput *input,
                              const AbBirdCollisionMightyHost *host, AbError *error);

#endif
