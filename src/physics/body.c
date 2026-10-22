#include "ab/physics_body.h"

#include "internal.h"

static void update_mass_center(AbBodyMassState *body, AbVec2 local_center) {
    AbVec2 old_center = body->center;
    body->local_center = local_center;
    body->center = ab_physics_point(local_center, body->transform);
    body->previous_center = body->center;
    float dx = body->center.x - old_center.x;
    float dy = body->center.y - old_center.y;
    body->linear_velocity.y = body->linear_velocity.y + body->angular_velocity * dx;
    body->linear_velocity.x = body->linear_velocity.x - body->angular_velocity * dy;
}

void ab_body_mass_set(AbBodyMassState *body, AbMass mass, bool world_locked) {
    if (world_locked || body->type != AB_BODY_DYNAMIC) {
        return;
    }
    body->mass = mass.mass > 0 ? mass.mass : 1.0f;
    body->inverse_mass = 1.0f / body->mass;
    body->inertia = 0;
    body->inverse_inertia = 0;
    if (mass.inertia > 0 && !body->fixed_rotation) {
        float distance_squared = mass.center.y * mass.center.y;
        distance_squared = distance_squared + mass.center.x * mass.center.x;
        body->inertia = mass.inertia - distance_squared * body->mass;
        body->inverse_inertia = 1.0f / body->inertia;
    }
    update_mass_center(body, mass.center);
}

void ab_body_mass_reset(AbBodyMassState *body, const AbFixtureMass *fixtures,
                        size_t count) {
    body->mass = 0;
    body->inverse_mass = 0;
    body->inertia = 0;
    body->inverse_inertia = 0;
    body->local_center = (AbVec2){0, 0};
    if (body->type != AB_BODY_DYNAMIC) {
        body->center = body->transform.position;
        body->previous_center = body->transform.position;
        body->previous_angle = body->angle;
        return;
    }
    AbVec2 center = {0};
    for (size_t index = 0; index < count; ++index) {
        if (fixtures[index].density == 0) {
            continue;
        }
        AbMass data = fixtures[index].data;
        body->mass = body->mass + data.mass;
        center.x = center.x + data.mass * data.center.x;
        center.y = center.y + data.mass * data.center.y;
        body->inertia = body->inertia + data.inertia;
    }
    if (body->mass > 0) {
        body->inverse_mass = 1.0f / body->mass;
        center.x = center.x * body->inverse_mass;
        center.y = center.y * body->inverse_mass;
    } else {
        body->mass = 1;
        body->inverse_mass = 1;
    }
    if (body->inertia > 0 && !body->fixed_rotation) {
        float distance_squared = center.y * center.y + center.x * center.x;
        body->inertia = body->inertia - distance_squared * body->mass;
        body->inverse_inertia = 1.0f / body->inertia;
    } else {
        body->inertia = 0;
        body->inverse_inertia = 0;
    }
    update_mass_center(body, center);
}
