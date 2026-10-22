#include "world_internal.h"

#include "ab/rotation.h"
#include "internal.h"

#include <math.h>

static bool mutable_body(const AbPhysicsWorld *world, size_t body) {
    return world && !world->failed && !world->locked && world->broadphase.tree.nodes &&
           ab_world_has_body(world, body);
}

static bool fail_body_change(AbPhysicsWorld *world, AbError *error) {
    world->failed = true;
    return ab_fail(error, "Physics body state change failed");
}

void ab_world_remove_body_contacts(AbPhysicsWorld *world, size_t body) {
    size_t index = 0;
    while (index < world->contact_count) {
        const AbWorldSolveContact *contact = world->storage.contacts + index;
        if (contact->first_body == body || contact->second_body == body) {
            ab_world_remove_contact(world, index);
        } else {
            ++index;
        }
    }
}

bool ab_world_set_transform(AbPhysicsWorld *world, size_t index, AbVec2 position,
                            float angle, AbError *error) {
    AbRotation rotation;
    if (!mutable_body(world, index) || !isfinite(position.x) || !isfinite(position.y) ||
        !ab_rotation_from_angle(angle, &rotation)) {
        return ab_fail(error, "Invalid physics body transform change");
    }
    AbBodyMassState *mass = &world->storage.bodies[index].motion.mass;
    AbRigidTransform transform = {position, rotation.sine, rotation.cosine};
    AbVec2 center = ab_physics_point(mass->local_center, transform);
    if (!isfinite(center.x) || !isfinite(center.y)) {
        return ab_fail(error, "Unsupported physics body transform range");
    }
    mass->transform = transform;
    mass->center = center;
    mass->previous_center = center;
    mass->angle = angle;
    mass->previous_angle = angle;
    for (size_t fixture = world->fixture_count; fixture > 0;) {
        AbWorldFixture *current =
            world->storage.fixtures + world->fixture_order[--fixture];
        if (current->body == index &&
            !ab_fixture_proxy_sync(&current->proxy, &current->shape, transform,
                                   transform, &world->broadphase, error)) {
            return fail_body_change(world, error);
        }
    }
    return true;
}

bool ab_world_set_active(AbPhysicsWorld *world, size_t index, bool active,
                         AbError *error) {
    if (!mutable_body(world, index)) {
        return ab_fail(error, "Invalid physics body activation change");
    }
    AbBodyMotion *body = &world->storage.bodies[index].motion;
    if (body->active == active) {
        return true;
    }
    body->active = active;
    for (size_t cursor = world->fixture_count; cursor > 0;) {
        size_t fixture = world->fixture_order[--cursor];
        AbWorldFixture *current = world->storage.fixtures + fixture;
        if (current->body != index) {
            continue;
        }
        bool okay =
            active ? ab_fixture_proxy_create(&current->proxy, &current->shape,
                                             body->mass.transform, &world->broadphase,
                                             fixture + 1, error)
                   : ab_fixture_proxy_destroy(&current->proxy, &world->broadphase);
        if (!okay) {
            return fail_body_change(world, error);
        }
    }
    if (!active) {
        ab_world_remove_body_contacts(world, index);
    }
    return true;
}

bool ab_world_set_body_type(AbPhysicsWorld *world, size_t index, AbBodyType type,
                            AbError *error) {
    if (!mutable_body(world, index) || (unsigned)type > AB_BODY_DYNAMIC) {
        return ab_fail(error, "Invalid physics body type change");
    }
    AbBodyMotion *body = &world->storage.bodies[index].motion;
    if (body->mass.type == type) {
        return true;
    }
    body->mass.type = type;
    if (!ab_world_reset_body_mass(world, index, world->fixture_count)) {
        return fail_body_change(world, error);
    }
    if (type == AB_BODY_STATIC) {
        body->mass.linear_velocity = (AbVec2){0, 0};
        body->mass.angular_velocity = 0;
        body->mass.previous_center = body->mass.center;
        body->mass.previous_angle = body->mass.angle;
        if (!ab_world_synchronize_body(world, index, error)) {
            return fail_body_change(world, error);
        }
    }
    ab_world_wake_body(body);
    body->force = (AbVec2){0, 0};
    body->torque = 0;
    ab_world_remove_body_contacts(world, index);
    for (size_t fixture = world->fixture_count; fixture > 0;) {
        AbWorldFixture *current =
            world->storage.fixtures + world->fixture_order[--fixture];
        if (current->body == index && current->proxy.active &&
            !ab_broadphase_touch(&world->broadphase, current->proxy.proxy, error)) {
            return fail_body_change(world, error);
        }
    }
    return true;
}

bool ab_world_set_fixed_rotation(AbPhysicsWorld *world, size_t index, bool fixed,
                                 AbError *error) {
    if (!mutable_body(world, index)) {
        return ab_fail(error, "Invalid physics body rotation change");
    }
    AbBodyMotion *body = &world->storage.bodies[index].motion;
    if (body->mass.fixed_rotation == fixed) {
        return true;
    }
    body->mass.fixed_rotation = fixed;
    body->mass.angular_velocity = 0;
    if (!ab_world_reset_body_mass(world, index, world->fixture_count)) {
        return fail_body_change(world, error);
    }
    return true;
}
