#include "ab/physics_filter.h"

bool ab_body_can_collide(AbBodyType first, AbBodyType second, size_t other_body,
                         const AbJointCollisionRule *rules, size_t rule_count) {
    if ((unsigned)first > AB_BODY_DYNAMIC || (unsigned)second > AB_BODY_DYNAMIC ||
        (rule_count && !rules) ||
        (first != AB_BODY_DYNAMIC && second != AB_BODY_DYNAMIC)) {
        return false;
    }
    for (size_t index = 0; index < rule_count; ++index) {
        if (rules[index].other_body == other_body && !rules[index].collide_connected) {
            return false;
        }
    }
    return true;
}

bool ab_fixture_filters_collide(AbCollisionFilter first, AbCollisionFilter second) {
    if (first.group && first.group == second.group) {
        return first.group > 0;
    }
    return (first.category & second.mask) != 0 && (second.category & first.mask) != 0;
}
