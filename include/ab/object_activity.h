#ifndef AB_OBJECT_ACTIVITY_H
#define AB_OBJECT_ACTIVITY_H

#include "ab/base.h"
#include "ab/physics_geometry.h"

typedef struct {
    AbVec2 velocity;
    float angular_velocity;
    float mass;
    float rolling_coefficient;
    float previous_rolling_volume;
    uint32_t collider, shape;
    bool awake, previous_awake, has_joints;
    bool pink_timer_present, bomb_timer_present, shoot_delay_present;
    bool bubble_is_boolean, bubble;
    bool ignored_is_boolean;
} AbObjectActivityInput;

typedef struct {
    bool has_awake;
    bool moving;
    bool above_tolerance;
    bool rolling_request;
    float rolling_volume;
} AbObjectActivityPlan;

/* Resolved ordinary GameLua.update post-property activity and rolling-volume
 * arithmetic. Timer presence means non-nil, including false, and can mark bird
 * collider 3 moving even without a current/previous awake body. A Boolean ignore
 * entry suppresses moving regardless of its value. It does not suppress awake
 * or above-tolerance flags. Bubble motion requires a Boolean true value.
 *
 * Velocity is the snapshot retained across updateObjectProperties. Other inputs
 * resolve their later native reads; this pure plan does not reproduce callbacks
 * that mutate these boundaries. Rolling requests address non-bird circles
 * (shape zero), including an empty sound name. previous_rolling_volume is that
 * sound key's current shared accumulator. The caller performs ordered map
 * lookup/store, aggregate ORs and actual audio work, then records old awake at
 * the end of the enclosing object pass. Portals, joint updates and full frame
 * scheduling remain separate. No allocation or state mutation. Used numeric
 * inputs must be finite; inactive branches ignore unused numbers. Preserve
 * nearest rounding, gradual underflow and separate float operations. Invalid
 * inputs preserve output.
 */
bool ab_object_activity_plan(const AbObjectActivityInput *input,
                             AbObjectActivityPlan *output, AbError *error);

#endif
