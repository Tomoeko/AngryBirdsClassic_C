#ifndef AB_PHYSICS_FILTER_H
#define AB_PHYSICS_FILTER_H

#include "ab/physics_body.h"

#include <stdint.h>

typedef struct {
    uint16_t category;
    uint16_t mask;
    int16_t group;
} AbCollisionFilter;

typedef struct {
    size_t other_body;
    bool collide_connected;
} AbJointCollisionRule;

/* Native engine eligibility only; the game's additional rules are separate.
 * The supplied joint rules belong to the first body. At least one body must
 * be dynamic, and every matching joint must permit connected-body collision.
 */
bool ab_body_can_collide(AbBodyType first, AbBodyType second, size_t other_body,
                         const AbJointCollisionRule *rules, size_t rule_count);
bool ab_fixture_filters_collide(AbCollisionFilter first, AbCollisionFilter second);

#endif
