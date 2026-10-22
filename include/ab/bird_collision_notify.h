#ifndef AB_BIRD_COLLISION_NOTIFY_H
#define AB_BIRD_COLLISION_NOTIFY_H

#include "ab/block_collision.h"

typedef struct {
    void *context;
    /* Bound gameObjectScript:onBirdCollision. The receiver remains implicit
     * here; notice.first/second are the actual two object-table arguments. */
    bool (*collision)(void *context, const AbBlockCollisionNotice *notice,
                      AbError *error);
} AbBirdCollisionScript;

typedef struct {
    void *context;
    /* Resolved plain collisionFunction(name, other_name), without a receiver. */
    bool (*call)(void *context, AbSpan name, AbSpan other_name, AbError *error);
} AbBirdCollisionFunction;

typedef struct {
    AbBlockCollisionObject object;
    AbDataValue projectile_feather, controllable;
    AbBirdCollisionScript *script;
    AbDataValue shot, has_collided, defence, strength;
    AbVec2 position;
    AbDataValue mighty_bait, mighty_eagle;
    AbBirdCollisionFunction collision_function;
    AbDataValue sprite, dont_update_damage_sprite, frozen_solid;
    AbDataValue collision, in_bubble, bomb_timer, pink_timer;
    AbDataValue boomerang_active, explode;
} AbBirdCollisionObject;

typedef struct {
    AbBlockCollisionInput contact;
    /* Both original point slots may be nil in enclosing museum single-bird
     * calls. Inactive point data is ignored, including nonfinite values. */
    bool point_present;
} AbBirdCollisionNotifyInput;

typedef struct {
    void *context;
    bool (*object)(void *context, AbSpan name, AbBirdCollisionObject **output,
                   AbError *error);
    /* Optional level hook receives only both objects, force and damage. Its
     * Lua return value is ignored, unlike the block callback's score gate. */
    bool (*level)(void *context, const AbBlockCollisionNotice *notice, AbError *error);
    bool (*definition)(void *context, AbSpan name, AbBlockCollisionDefinition **output,
                       AbError *error);
    /* A truthy definition.onCollision requires its named function. Unlike the
     * block callback, an absent binding is an error at this boundary. */
    bool (*named)(void *context, AbDataValue name, const AbBlockCollisionNotice *notice,
                  AbError *error);
    bool (*scripts)(void *context, const AbBlockCollisionNotice *notice,
                    AbError *error);
} AbBirdCollisionNotifyHost;

typedef struct {
    AbBirdCollisionObject *objects[2];
    AbBlockCollisionDefinition *definitions[2];
    bool projectile_skipped;
} AbBirdCollisionNotification;

/* Same prefix, retaining its original cached identities for continuation.
 * Definitions are absent on a projectile skip. Output commits on success. */
bool ab_bird_collision_notify_resolved(const AbBirdCollisionNotifyInput *input,
                                       const AbBirdCollisionNotifyHost *host,
                                       AbBirdCollisionNotification *output,
                                       AbError *error);

/* Notification prefix of CollisionCallbacks.birdCollision. Resolve both world
 * objects, then skip the whole callback if either projectileFeather is truthy.
 * Otherwise run the level hook, definitions, named callbacks and block-event
 * boundaries in original order. Second named/event calls negate the normal.
 *
 * The first game-object method requires the second not be a sensor. The second
 * method requires the first not be a sensor and receives reversed object-table
 * arguments only when the second object's current controllable is truthy.
 * Both methods retain incoming geometry and explicit point presence. Hosts may
 * change later fields, definitions and method bindings while storage stays live.
 *
 * No sensor recursion, hats, scoring, collision flags, specialty handling,
 * effects or physics run here; the enclosing bird handler must continue those
 * phases after a successful non-skipped prefix. Output commits only on success;
 * errors retain preceding host/state effects. Borrow plain typed objects/names
 * and stable input through the call; no allocation or Lua. Used numeric input
 * must be finite. Do not reenter or release cached objects/definitions in hosts.
 */
bool ab_bird_collision_notify(const AbBirdCollisionNotifyInput *input,
                              const AbBirdCollisionNotifyHost *host,
                              bool *projectile_skipped, AbError *error);

#endif
