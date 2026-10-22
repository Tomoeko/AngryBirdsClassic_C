#include "ab/physics_step.h"

#include "ab/rotation.h"

#include <math.h>

bool ab_body_initialize(const AbBodyDefinition *definition, AbBodyMotion *output) {
    if (!definition || !output ||
        (definition->type != AB_BODY_STATIC && definition->type != AB_BODY_KINEMATIC &&
         definition->type != AB_BODY_DYNAMIC) ||
        !isfinite(definition->position.x) || !isfinite(definition->position.y) ||
        !isfinite(definition->linear_velocity.x) ||
        !isfinite(definition->linear_velocity.y) ||
        !isfinite(definition->angular_velocity) ||
        !isfinite(definition->linear_damping) || definition->linear_damping < 0 ||
        !isfinite(definition->angular_damping) || definition->angular_damping < 0 ||
        !isfinite(definition->gravity_scale)) {
        return false;
    }
    AbRotation rotation;
    if (!ab_rotation_from_angle(definition->angle, &rotation)) {
        return false;
    }
    float mass = definition->type == AB_BODY_DYNAMIC ? 1.0f : 0.0f;
    *output = (AbBodyMotion){
        .mass =
            {
                .type = definition->type,
                .fixed_rotation = definition->fixed_rotation,
                .transform = {definition->position, rotation.sine, rotation.cosine},
                .previous_center = definition->position,
                .center = definition->position,
                .previous_angle = definition->angle,
                .angle = definition->angle,
                .linear_velocity = definition->linear_velocity,
                .angular_velocity = definition->angular_velocity,
                .mass = mass,
                .inverse_mass = mass,
            },
        .linear_damping = definition->linear_damping,
        .angular_damping = definition->angular_damping,
        .gravity_scale = definition->gravity_scale,
        .awake = definition->awake,
        .sleep_allowed = definition->sleep_allowed,
        .bullet = definition->bullet,
        .active = definition->active,
        .user_data = definition->user_data,
    };
    return true;
}
