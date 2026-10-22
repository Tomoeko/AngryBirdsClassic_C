#ifndef AB_BIRD_COLLISION_H
#define AB_BIRD_COLLISION_H

#include "ab/bird_collision_damage.h"
#include "ab/bird_collision_effects.h"
#include "ab/bird_collision_mighty.h"
#include "ab/bird_collision_projectiles.h"
#include "ab/bird_collision_specialty.h"

typedef struct {
    AbBirdCollisionPreludeState *prelude;
    AbBirdCollisionSpriteState *sprites;
    AbBirdCollisionGrenades *grenades;
} AbBirdCollisionState;

typedef struct {
    const AbBirdCollisionNotifyHost *notifications;
    const AbBirdCollisionPreludeHost *prelude;
    const AbBirdCollisionSpriteHost *sprites;
    const AbBirdCollisionSpecialtyHost *specialty;
    const AbBirdCollisionProjectileHost *projectiles;
    const AbBirdCollisionEffectHost *effects;
    AbBirdCollisionDamageHost *damage;
    void *definition_context;
    /* Pure view of an already cached notification definition. Never requery
     * the definition registry here: prior callbacks may have replaced it. */
    AbBirdCollisionEffectDefinition *(*effect_definition)(
        void *context, AbBlockCollisionDefinition *cached_definition);
} AbBirdCollisionHost;

/* Compose every recovered bird callback phase in original order. Resolve and
 * retain both entry objects/definitions through notifications, then honor the
 * projectile and related-hat returns. Mighty handling uses the current flying
 * name and fresh world metadata. Continue shot sprites, specialty, boomerang /
 * grenade processing, contact effects and final second-object damage refresh.
 * A grenade early return skips both final phases; a cleared first local name
 * skips contact effects but still permits second-object damage refresh.
 *
 * Incoming names/numbers/point presence are captured once. Owner hosts must
 * expose consistent live registries and callback bindings across phase views;
 * cached objects/definitions remain borrowed even if their bindings change.
 * Sprite-state availability is canonical. Temporarily bind the prelude's read
 * to it and restore the previous internal binding on every exit. Hosts may
 * mutate later fields/bindings, but must not reenter or release storage.
 * No allocation, Lua, fabricated resources or owned gameplay/session occurs.
 * Missing required phase state/hosts fail when reached, retaining prior effects.
 */
bool ab_bird_collision_callback(const AbBirdCollisionNotifyInput *input,
                                AbBirdCollisionState state, AbBirdCollisionHost *host,
                                AbError *error);

#endif
