#include "ab/collision_damage.h"

#include <math.h>

static bool valid_input(const AbBirdDamageInput *input) {
    return input && isfinite(input->velocity.x) && isfinite(input->velocity.y) &&
           isfinite(input->mass) && input->mass >= 0 &&
           isfinite(input->damage_multiplier) && isfinite(input->velocity_multiplier) &&
           isfinite(input->velocity_modifier) &&
           (!input->has_target_strength ||
            (isfinite(input->target_strength) &&
             (!input->target_script_present || isfinite(input->target_defence)) &&
             (!input->bird_script_present || input->target_script_present)));
}

static bool update_velocity(const AbBirdDamageInput *input, float force,
                            float proposed_strength, AbBirdDamage *result) {
    float scale;
    if (input->legacy_velocity) {
        scale = -proposed_strength / input->mass;
        scale = scale / force;
        scale = scale * 10.0f;
        scale = scale * 1.75f;
        scale = scale * input->velocity_modifier;
        result->velocity_change = AB_BIRD_VELOCITY_IMMEDIATE;
    } else {
        scale = (force - input->target_strength) / force;
        scale = scale * input->velocity_multiplier;
        scale = scale * input->velocity_modifier;
        result->velocity_change = AB_BIRD_VELOCITY_DEFERRED;
    }
    /* Lua math.min keeps its first operand when the second is not smaller. */
    if (1.0f < scale) {
        scale = 1;
    }
    result->velocity = (AbVec2){input->velocity.x * scale, input->velocity.y * scale};
    return isfinite(scale) && isfinite(result->velocity.x) &&
           isfinite(result->velocity.y);
}

bool ab_bird_collision_damage(const AbBirdDamageInput *input, AbBirdDamage *output) {
    if (!output || !valid_input(input)) {
        return false;
    }
    float squared =
        input->velocity.x * input->velocity.x + input->velocity.y * input->velocity.y;
    float joint_force = input->mass * sqrtf(squared);
    joint_force = joint_force * 0x1.99999ap-4f;
    AbBirdDamage result = {
        .joint_force = joint_force,
        .collision_force = joint_force * input->damage_multiplier,
        .remaining_strength = input->has_target_strength ? input->target_strength : 0,
    };
    if (!isfinite(result.joint_force) || !isfinite(result.collision_force)) {
        return false;
    }
    float damage = 0;
    if (input->has_target_strength) {
        float defence = input->target_script_present ? input->target_defence : 0;
        damage = result.collision_force - defence;
        if (input->bird_script_present && input->target_ignores_damage) {
            damage = 0;
        }
        if (0 < damage) {
            float proposed = input->target_strength - damage;
            result.remaining_strength = 0 < proposed ? proposed : 0;
            if (proposed <= 0) {
                result.mark_dead = true;
                result.disable_contact = true;
                if (!update_velocity(input, result.collision_force, proposed,
                                     &result)) {
                    return false;
                }
                damage = input->target_strength;
            }
        }
    }
    result.callback_damage = floorf(damage);
    if (!isfinite(result.remaining_strength) || !isfinite(result.callback_damage)) {
        return false;
    }
    *output = result;
    return true;
}
