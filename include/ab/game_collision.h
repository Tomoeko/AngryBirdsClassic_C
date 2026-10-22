#ifndef AB_GAME_COLLISION_H
#define AB_GAME_COLLISION_H

#include "ab/physics_filter.h"

typedef enum {
    AB_COLLIDER_BASIC,
    AB_COLLIDER_EAGLE,
    AB_COLLIDER_STATIC,
    AB_COLLIDER_BIRD,
    AB_COLLIDER_PIG,
    AB_COLLIDER_GHOST,
    AB_COLLIDER_EGG,
    AB_COLLIDER_INVISIBLE,
    AB_COLLIDER_PORTAL,
    AB_COLLIDER_NONE,
    AB_COLLIDER_GLUE,
    AB_COLLIDER_BUBBLE,
    AB_GAME_COLLIDER_TYPES,
} AbGameColliderType;

typedef struct {
    bool allowed[AB_GAME_COLLIDER_TYPES][AB_GAME_COLLIDER_TYPES];
} AbGameCollisionMatrix;

typedef struct {
    uint32_t collider;
    bool enabled;
} AbGameCollisionObject;

/* Initialize all entries as allowed, then apply native classic rules. Setup
 * alone preserves pairs that the native setup method does not write. Pair
 * updates are symmetric; eligibility still reads the supplied ordered entry.
 * These functions configure eligibility only, without contact handlers.
 */
bool ab_game_collision_initialize(AbGameCollisionMatrix *matrix);
bool ab_game_collision_setup(AbGameCollisionMatrix *matrix);
bool ab_game_collision_set(AbGameCollisionMatrix *matrix, uint32_t first,
                           uint32_t second, bool allowed);

/* GameLua fixture eligibility above body/joint eligibility. Matching nonzero
 * fixture groups return before game metadata; otherwise both masks must pass.
 * Missing either object permits collision after mask checks. With both objects,
 * use the ordered collider matrix entry and both enabled flags. Matrix contents
 * are supplied from recovered game configuration; this does not invent rules.
 * Invalid matrix/indices reject when that branch is needed. No mutation occurs.
 */
bool ab_game_collision_allowed(AbCollisionFilter first, AbCollisionFilter second,
                               const AbGameCollisionObject *first_object,
                               const AbGameCollisionObject *second_object,
                               const AbGameCollisionMatrix *matrix);

#endif
