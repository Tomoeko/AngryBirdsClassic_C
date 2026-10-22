#ifndef AB_BIRD_COLLISION_SPECIALTY_H
#define AB_BIRD_COLLISION_SPECIALTY_H

#include "ab/bird_collision_prelude.h"
#include "ab/bird_collision_sprites.h"
#include "ab/ui_pointer.h"

typedef bool (*AbBirdCollisionPrint)(void *context, AbUiPointerValue value,
                                     AbError *error);

typedef struct {
    AbSpan first, second;
    float force, damage;
    AbVec2 normal;
} AbBirdCollisionNamedInput;

typedef struct {
    void *context;
    bool (*definition)(void *context, AbSpan current_name,
                       AbBirdCollisionSpriteDefinition **output, AbError *error);
    bool (*object)(void *context, AbSpan current_name, AbBirdCollisionObject **output,
                   AbError *error);
    /* Pure lookup in the enclosing callback's cached game environment. Function
     * values carry a nonzero identity. Query again after the second print. */
    bool (*binding)(void *context, AbDataValue key, AbUiPointerValue *output,
                    AbError *error);
    AbBirdCollisionPrint print;
    /* Required named function, without a receiver. The first argument is the
     * current flyingBird value, which may be NULL after a print callback. The
     * remaining arguments are both incoming names, force, damage and normal;
     * point values are not passed. Function identity is captured before args. */
    bool (*call)(void *context, AbUiPointerValue function,
                 AbBirdCollisionObject *flying_argument,
                 const AbBirdCollisionNamedInput *contact, AbError *error);
    bool (*sprite)(void *context, AbSpan current_name, AbDataValue value,
                   AbError *error);
    bool (*hat)(void *context, AbSpan current_name, AbBirdCollisionHat **output,
                AbError *error);
    bool (*release_hat)(void *context, AbSpan current_name, AbError *error);
} AbBirdCollisionSpecialtyHost;

/* Bird callback instructions 500..736 over plain typed data. The second sensor
 * must be falsy and the current flying name must match either incoming name.
 * Cache definition.specialty before optional print/print/named collision work.
 * The named channel runs only for a falsy current collision flag and truthy
 * definition.onFlyingBirdCollided. Its two environment lookups bracket the second
 * print.
 *
 * Non-BOMB/BUBBLES defaults can clear specialty availability and update sprites.
 * New BOMB collisions set a truthy explosionTimer or 1.5, damageSprite and sprite,
 * then enable specialty availability after setSprite returns. BUBBLES writes a
 * 1.5 pinkTimer only when inBubble is falsy, pinkTimer is nil and collision is
 * not Boolean true. All continuing specialties then assign collision=true.
 * Finally release an attached hat only when its current world object exists.
 *
 * Read current flying binding and later fields after callbacks. Preserve the
 * cached specialty and snapshot incoming contact arguments; points are unused.
 * C function return status reports host failure, never a Lua return value.
 * Missing required hosts/metadata reject when reached, preserving prior effects.
 * Names/tables/objects stay borrowed and live; lookups are pure. Do not reenter.
 * No allocation, Lua, actual sprite resources or ability physics run here.
 */
bool ab_bird_collision_specialty(const AbBlockCollisionInput *contact,
                                 AbBirdCollisionObject *second,
                                 AbBirdCollisionSpriteState *state,
                                 const AbBirdCollisionSpecialtyHost *host,
                                 AbError *error);

#endif
