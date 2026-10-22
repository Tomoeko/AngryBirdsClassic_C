#include "world_internal.h"

#include <string.h>

bool ab_world_pair_allowed(const AbPhysicsWorld *world, size_t first, size_t second) {
    const AbWorldFixture *first_fixture = world->storage.fixtures + first;
    const AbWorldFixture *second_fixture = world->storage.fixtures + second;
    if (first_fixture->body == second_fixture->body ||
        !ab_body_can_collide(
            world->storage.bodies[second_fixture->body].motion.mass.type,
            world->storage.bodies[first_fixture->body].motion.mass.type,
            first_fixture->body, NULL, 0)) {
        return false;
    }
    for (size_t cursor = 0; cursor < world->joint_count; ++cursor) {
        const AbWorldSolveJoint *joint =
            world->storage.joints + world->joint_order[cursor];
        if (!joint->collide_connected &&
            ((joint->first_body == first_fixture->body &&
              joint->second_body == second_fixture->body) ||
             (joint->first_body == second_fixture->body &&
              joint->second_body == first_fixture->body))) {
            return false;
        }
    }
    if (world->listener.filter) {
        return world->listener.filter(world->listener.context, world, first, second);
    }
    return ab_fixture_filters_collide(first_fixture->filter, second_fixture->filter);
}

static bool has_contact(const AbPhysicsWorld *world, size_t first, size_t second) {
    for (size_t contact = 0; contact < world->contact_count; ++contact) {
        AbFixturePair pair = world->storage.pairs[contact];
        if ((pair.first == first && pair.second == second) ||
            (pair.first == second && pair.second == first)) {
            return true;
        }
    }
    return false;
}

static float shape_radius(const AbPhysicsShape *shape) {
    return shape->kind == AB_SHAPE_CIRCLE ? shape->data.circle.radius
                                          : shape->data.polygon.radius;
}

static void add_pair(void *context, AbBroadPair proxy_pair, uintptr_t first_payload,
                     uintptr_t second_payload) {
    (void)proxy_pair;
    AbPhysicsWorld *world = context;
    if (world->failed) {
        return;
    }
    if (!first_payload || !ab_world_has_fixture(world, first_payload - 1) ||
        !second_payload || !ab_world_has_fixture(world, second_payload - 1)) {
        world->failed = true;
        return;
    }
    size_t first = first_payload - 1, second = second_payload - 1;
    if (has_contact(world, first, second) ||
        !ab_world_pair_allowed(world, first, second)) {
        return;
    }
    if (world->contact_count == world->storage.contact_capacity) {
        world->failed = true;
        return;
    }
    AbWorldFixture *first_fixture = world->storage.fixtures + first;
    AbWorldFixture *second_fixture = world->storage.fixtures + second;
    if (first_fixture->shape.kind == AB_SHAPE_CIRCLE &&
        second_fixture->shape.kind == AB_SHAPE_POLYGON) {
        size_t swapped = first;
        first = second;
        second = swapped;
        first_fixture = world->storage.fixtures + first;
        second_fixture = world->storage.fixtures + second;
    }
    AbWorldSolveContact contact = {
        .first_body = first_fixture->body,
        .second_body = second_fixture->body,
        .first_radius = shape_radius(&first_fixture->shape),
        .second_radius = shape_radius(&second_fixture->shape),
        .sensor = first_fixture->sensor || second_fixture->sensor,
        .impact_fraction = 1,
    };
    if (!ab_contact_state_initialize(
            &contact.state, first_fixture->friction, second_fixture->friction,
            first_fixture->restitution, second_fixture->restitution)) {
        world->failed = true;
        return;
    }
    memmove(world->storage.contacts + 1, world->storage.contacts,
            world->contact_count * sizeof(contact));
    memmove(world->storage.pairs + 1, world->storage.pairs,
            world->contact_count * sizeof(*world->storage.pairs));
    world->storage.contacts[0] = contact;
    world->storage.pairs[0] = (AbFixturePair){first, second};
    if (!contact.sensor) {
        ab_world_wake_body(&world->storage.bodies[contact.first_body].motion);
        ab_world_wake_body(&world->storage.bodies[contact.second_body].motion);
    }
    ++world->contact_count;
}

bool ab_world_find_contacts(AbPhysicsWorld *world, AbError *error) {
    if (!ab_broadphase_update(&world->broadphase, add_pair, world, error)) {
        world->failed = true;
        return false;
    }
    if (world->failed) {
        return ab_fail(error, "World contact capacity or material failure");
    }
    return true;
}

void ab_world_remove_contact(AbPhysicsWorld *world, size_t index) {
    AbWorldSolveContact *contact = world->storage.contacts + index;
    if ((contact->state.flags & AB_CONTACT_TOUCHING) && world->listener.end) {
        world->listener.end(world->listener.context, world, index);
    }
    AbFixturePair pair = world->storage.pairs[index];
    bool sensor = world->storage.fixtures[pair.first].sensor ||
                  world->storage.fixtures[pair.second].sensor;
    if (contact->state.manifold.point_count && !sensor) {
        ab_world_wake_body(&world->storage.bodies[contact->first_body].motion);
        ab_world_wake_body(&world->storage.bodies[contact->second_body].motion);
    }
    size_t moved = --world->contact_count - index;
    memmove(contact, contact + 1, moved * sizeof(*contact));
    memmove(world->storage.pairs + index, world->storage.pairs + index + 1,
            moved * sizeof(*world->storage.pairs));
}
