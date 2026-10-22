#include "world_internal.h"

#include "internal.h"

bool ab_world_set_fixture_filter(AbPhysicsWorld *world, size_t index,
                                 AbCollisionFilter filter, AbError *error) {
    if (!world || world->failed || !world->broadphase.tree.nodes ||
        !ab_world_has_fixture(world, index)) {
        return ab_fail(error, "Invalid physics fixture filter change");
    }
    AbWorldFixture *fixture = world->storage.fixtures + index;
    fixture->filter = filter;
    for (size_t contact = 0; contact < world->contact_count; ++contact) {
        AbFixturePair pair = world->storage.pairs[contact];
        if (pair.first == index || pair.second == index) {
            world->storage.contacts[contact].state.flags |= 8;
        }
    }
    if (fixture->proxy.active &&
        !ab_broadphase_touch(&world->broadphase, fixture->proxy.proxy, error)) {
        world->failed = true;
        return false;
    }
    return true;
}

bool ab_world_set_fixture_sensor(AbPhysicsWorld *world, size_t index, bool sensor) {
    if (!world || world->failed || !world->broadphase.tree.nodes ||
        !ab_world_has_fixture(world, index)) {
        return false;
    }
    AbWorldFixture *fixture = world->storage.fixtures + index;
    if (fixture->sensor != sensor) {
        ab_world_wake_body(&world->storage.bodies[fixture->body].motion);
        fixture->sensor = sensor;
    }
    return true;
}
