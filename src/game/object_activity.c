#include "ab/object_activity.h"

#include <math.h>

static bool rolling_volume(const AbObjectActivityInput *input, float *output,
                           AbError *error) {
    if (!isfinite(input->angular_velocity) || !isfinite(input->mass) ||
        !isfinite(input->rolling_coefficient) ||
        !isfinite(input->previous_rolling_volume)) {
        return ab_fail(error, "Unsupported object rolling-volume input");
    }
    /* Native conditional negation retains negative zero. */
    float angular = input->angular_velocity;
    if (angular < 0) {
        angular = -angular;
    }
    float volume = angular * input->rolling_coefficient;
    volume = volume / 400.0f;
    volume = volume * input->mass;
    if (volume > 1) {
        volume = 1;
    }
    float previous = input->previous_rolling_volume;
    if (previous < volume) {
        previous = volume;
    }
    *output = previous;
    return true;
}

bool ab_object_activity_plan(const AbObjectActivityInput *input,
                             AbObjectActivityPlan *output, AbError *error) {
    if (!input || !output) {
        return ab_fail(error, "Missing object activity input or output");
    }
    AbObjectActivityPlan plan = {.has_awake = input->awake || input->previous_awake,
                                 .moving = input->collider == 3 &&
                                           (input->pink_timer_present ||
                                            input->bomb_timer_present ||
                                            input->shoot_delay_present)};
    if (plan.has_awake) {
        if (!isfinite(input->velocity.x) || !isfinite(input->velocity.y)) {
            return ab_fail(error, "Unsupported object activity velocity");
        }
        float squared = input->velocity.y * input->velocity.y;
        squared = squared + input->velocity.x * input->velocity.x;
        plan.above_tolerance =
            (double)squared > 0.0005 && input->collider != 3 && !input->has_joints;
        if (squared > 9) {
            plan.moving = true;
        } else {
            if (!isfinite(input->angular_velocity)) {
                return ab_fail(error, "Unsupported object activity angular velocity");
            }
            if ((input->angular_velocity > 0.1f && !input->has_joints) ||
                (input->bubble_is_boolean && input->bubble)) {
                plan.moving = true;
            }
        }
        if (input->collider != 3 && input->shape == 0) {
            if (!rolling_volume(input, &plan.rolling_volume, error)) {
                return false;
            }
            plan.rolling_request = true;
        }
    }
    if (plan.moving && input->ignored_is_boolean) {
        plan.moving = false;
    }
    *output = plan;
    return true;
}
