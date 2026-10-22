#ifndef AB_CLASSIC_MUSEUM_COLLISIONS_H
#define AB_CLASSIC_MUSEUM_COLLISIONS_H

#include "ab/bird_collision_notify.h"
#include "ab/classic_block_collisions.h"

typedef struct {
    AbClassicLevel *level;
    AbClassicBlockCollisions *blocks;
    AbStringMap *dead_blocks;
    /* One stable physical-table value per initial object, in creation order.
     * Dead-map handles borrow these rows until that map is drained/released. */
    AbDataValue *physical_values;
    size_t physical_capacity;
    void *context;
    AbClassicJointBreakRequest break_joint;
    /* Complete CollisionCallbacks.birdCollision, after actual Museum damage,
     * joint and velocity work. Required on bird and bird-pair routes. */
    bool (*bird)(void *context, const AbBirdCollisionNotifyInput *input,
                 AbError *error);
    /* Complete Museum handler for a pair containing a glue definition. Its
     * primary damage and subsequent glue response must both be implemented. */
    bool (*glue)(void *context, AbGameContactCall *call, AbError *error);
} AbClassicMuseumCollisionConfig;

typedef struct {
    AbClassicMuseumCollisionConfig config;
    AbClassicLevelView level;
    AbDataEntry velocity_entries[3];
    AbDataTable velocity;
    bool calling;
} AbClassicMuseumCollisions;

/* Caller-owned stable ordinary Museum.onCollision binding. Borrow the same
 * level, block adapter and shared dead map; initialize supplied physical-value
 * rows without gameplay effects. No allocation, implicit hosts or stepping.
 * Close/release the dead map before its borrowed physical-value storage and
 * level; no separate close is needed after removing every callback reference.
 * Invalid open preserves the output and supplied rows. */
bool ab_classic_museum_collisions_open(AbClassicMuseumCollisionConfig config,
                                       AbClassicMuseumCollisions *output,
                                       AbError *error);

/* Direct AbClassicContacts collision service. Resolve cached definition collider
 * routes, delegate ordinary blocks to the actual adapter, and bind the recovered
 * single-bird/bird-pair numeric paths to canonical properties, actual joint
 * requests, body velocity and shared death map. A single-bird callback retains
 * the original point-to-normal/nil-point argument mapping; pairs retain geometry.
 * The full bird callback owns score/effect phases. Do not add score again here.
 *
 * Publish response velocity parameters in stable owner storage, consumed before
 * the next contact. Script-table absence and unsupported scalar/mutable catalog
 * bindings reject at their reached boundary. Names/physical identities remain
 * fixed; callbacks must not release storage, replace the listener or reenter.
 * Later failures retain earlier health/map/joint/velocity effects. Stop after a
 * failure. The required bird/glue callbacks are not supplied by this numeric/
 * routing owner; missing callbacks fail rather than dropping gameplay effects. */
bool ab_classic_museum_collisions_contact(void *context, AbGameContactCall *call,
                                          AbError *error);

#endif
