#include "world_internal.h"

#include "fixture_internal.h"

#include <math.h>
#include <string.h>

bool ab_world_has_body(const AbPhysicsWorld *world, size_t body) {
    return world && body < world->body_slot_count && world->body_alive &&
           world->body_alive[body];
}

bool ab_world_has_fixture(const AbPhysicsWorld *world, size_t fixture) {
    return world && fixture < world->fixture_slot_count && world->fixture_alive &&
           world->fixture_alive[fixture];
}

bool ab_world_mutable(const AbPhysicsWorld *world) {
    return world && !world->failed && !world->locked && world->broadphase.tree.nodes;
}

size_t ab_world_unused_slot(const bool *alive, size_t count, size_t capacity) {
    for (size_t slot = 0; slot < count; ++slot) {
        if (!alive[slot]) {
            return slot;
        }
    }
    return count < capacity ? count : SIZE_MAX;
}

bool ab_world_fixture_valid(const AbPhysicsWorld *world,
                            const AbWorldFixture *fixture) {
    if (!fixture || fixture->body >= world->storage.body_capacity ||
        !isfinite(fixture->density) || fixture->density < 0 ||
        !isfinite(fixture->friction) || fixture->friction < 0 ||
        !isfinite(fixture->restitution)) {
        return false;
    }
    AbAabb bounds;
    return ab_fixture_shape_bounds(
        &fixture->shape, world->storage.bodies[fixture->body].motion.mass.transform,
        &bounds);
}

bool ab_world_attach_fixture(AbPhysicsWorld *world, size_t index, AbError *error) {
    AbWorldFixture *fixture = world->storage.fixtures + index;
    const AbBodyMotion *body = &world->storage.bodies[fixture->body].motion;
    fixture->proxy = (AbFixtureProxy){.proxy = -1};
    if (body->active &&
        !ab_fixture_proxy_create(&fixture->proxy, &fixture->shape, body->mass.transform,
                                 &world->broadphase, index + 1, error)) {
        return false;
    }
    world->fixture_order[world->fixture_count++] = index;
    world->fixture_alive[index] = true;
    if (index >= world->fixture_slot_count) {
        world->fixture_slot_count = index + 1;
    }
    return fixture->density <= 0 ||
           ab_world_reset_body_mass(world, fixture->body, world->fixture_count);
}

bool ab_world_create_body(AbPhysicsWorld *world, const AbBodyDefinition *definition,
                          size_t *output, AbError *error) {
    AbBodyMotion motion;
    if (!output || !ab_world_mutable(world) ||
        !ab_body_initialize(definition, &motion)) {
        return ab_fail(error, "Invalid physics body creation");
    }
    size_t slot = ab_world_unused_slot(world->body_alive, world->body_slot_count,
                                       world->storage.body_capacity);
    if (slot == SIZE_MAX) {
        return ab_fail(error, "Physics body capacity exhausted");
    }
    world->storage.bodies[slot] = (AbWorldSolveBody){.motion = motion};
    memmove(world->body_order + 1, world->body_order,
            world->body_count * sizeof(*world->body_order));
    world->body_order[0] = slot;
    world->body_alive[slot] = true;
    ++world->body_count;
    if (slot >= world->body_slot_count) {
        world->body_slot_count = slot + 1;
    }
    *output = slot;
    return true;
}

bool ab_world_create_fixture(AbPhysicsWorld *world, const AbWorldFixture *definition,
                             size_t *output, AbError *error) {
    if (!output || !ab_world_mutable(world) || !definition ||
        !ab_world_has_body(world, definition->body) ||
        !ab_world_fixture_valid(world, definition)) {
        return ab_fail(error, "Invalid physics fixture creation");
    }
    size_t slot = ab_world_unused_slot(world->fixture_alive, world->fixture_slot_count,
                                       world->storage.fixture_capacity);
    if (slot == SIZE_MAX) {
        return ab_fail(error, "Physics fixture capacity exhausted");
    }
    world->storage.fixtures[slot] = *definition;
    if (!ab_world_attach_fixture(world, slot, error)) {
        world->failed = true;
        return ab_fail(error, "Cannot attach created physics fixture");
    }
    world->new_fixtures = true;
    *output = slot;
    return true;
}

void ab_world_unlink_slot(size_t *order, size_t *count, size_t slot) {
    for (size_t index = 0; index < *count; ++index) {
        if (order[index] == slot) {
            --*count;
            memmove(order + index, order + index + 1,
                    (*count - index) * sizeof(*order));
            return;
        }
    }
}

static bool remove_fixture(AbPhysicsWorld *world, size_t index, AbError *error) {
    AbWorldFixture *fixture = world->storage.fixtures + index;
    if (!ab_fixture_proxy_destroy(&fixture->proxy, &world->broadphase)) {
        world->failed = true;
        return ab_fail(error, "Cannot remove physics fixture proxies");
    }
    world->fixture_alive[index] = false;
    *fixture = (AbWorldFixture){.proxy = {.proxy = -1}};
    return true;
}

bool ab_world_destroy_fixture(AbPhysicsWorld *world, size_t index, AbError *error) {
    if (!ab_world_mutable(world) || !ab_world_has_fixture(world, index)) {
        return ab_fail(error, "Invalid physics fixture destruction");
    }
    size_t body = world->storage.fixtures[index].body;
    /* Native removal unlinks the fixture before delivering contact end events. */
    ab_world_unlink_slot(world->fixture_order, &world->fixture_count, index);
    size_t contact = 0;
    while (contact < world->contact_count) {
        AbFixturePair pair = world->storage.pairs[contact];
        if (pair.first == index || pair.second == index) {
            ab_world_remove_contact(world, contact);
        } else {
            ++contact;
        }
    }
    if (!remove_fixture(world, index, error)) {
        return false;
    }
    if (!ab_world_reset_body_mass(world, body, world->fixture_count)) {
        world->failed = true;
        return ab_fail(error, "Cannot reset mass after physics fixture removal");
    }
    return true;
}

bool ab_world_destroy_body(AbPhysicsWorld *world, size_t index, AbError *error) {
    if (!ab_world_mutable(world) || !ab_world_has_body(world, index)) {
        return ab_fail(error, "Invalid physics body destruction");
    }
    if (!ab_world_remove_body_joints(world, index, error)) {
        return false;
    }
    ab_world_remove_body_contacts(world, index);
    for (size_t cursor = world->fixture_count; cursor > 0;) {
        size_t fixture = world->fixture_order[--cursor];
        if (world->storage.fixtures[fixture].body != index) {
            continue;
        }
        if (world->listener.destroy_fixture) {
            world->listener.destroy_fixture(world->listener.context, world, fixture);
        }
        if (!remove_fixture(world, fixture, error)) {
            return false;
        }
        ab_world_unlink_slot(world->fixture_order, &world->fixture_count, fixture);
    }
    ab_world_unlink_slot(world->body_order, &world->body_count, index);
    world->body_alive[index] = false;
    world->storage.bodies[index] = (AbWorldSolveBody){0};
    return true;
}
