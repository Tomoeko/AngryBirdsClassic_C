#include "world_internal.h"

#include "fixture_internal.h"

#include <math.h>
#include <stdlib.h>

void ab_world_wake_body(AbBodyMotion *body) {
    if (!body->awake) {
        body->awake = true;
        body->sleep_time = 0;
    }
}

void ab_world_close(AbPhysicsWorld *world) {
    if (world) {
        ab_broadphase_close(&world->broadphase);
        free(world->body_order);
        free(world->fixture_order);
        free(world->body_alive);
        free(world->fixture_alive);
        free(world->joint_order);
        free(world->joint_alive);
        *world = (AbPhysicsWorld){0};
    }
}

static bool valid_storage(AbWorldStorage storage) {
    return storage.body_count <= storage.body_capacity &&
           storage.joint_count <= storage.joint_capacity &&
           storage.fixture_count <= storage.fixture_capacity &&
           storage.body_capacity <= SIZE_MAX / sizeof(*storage.bodies) &&
           storage.fixture_capacity <= SIZE_MAX / sizeof(*storage.fixtures) &&
           storage.body_capacity <= SIZE_MAX / sizeof(size_t) &&
           storage.fixture_capacity <= SIZE_MAX / sizeof(size_t) &&
           storage.joint_capacity <= SIZE_MAX / sizeof(*storage.joints) &&
           storage.joint_capacity <= SIZE_MAX / sizeof(size_t) &&
           storage.joint_capacity <= SIZE_MAX / 2 &&
           storage.contact_capacity <= SIZE_MAX / sizeof(*storage.contacts) &&
           storage.contact_capacity <= SIZE_MAX / sizeof(*storage.pairs) &&
           (!storage.body_capacity || storage.bodies) &&
           (!storage.fixture_capacity || (storage.fixtures && storage.mass_scratch)) &&
           (!storage.contact_capacity || (storage.contacts && storage.pairs)) &&
           (!storage.joint_capacity || storage.joints) &&
           storage.mass_capacity >= storage.fixture_capacity && storage.solve &&
           storage.solve->body_capacity >= storage.body_capacity &&
           storage.solve->contact_capacity >= storage.contact_capacity &&
           storage.solve->joint_capacity >= storage.joint_capacity &&
           (!storage.joint_capacity ||
            (storage.solve->joints && storage.solve->joint_order &&
             storage.solve->joint_heads && storage.solve->joint_edge_next &&
             storage.solve->solver.body_capacity >= storage.body_capacity &&
             storage.solve->solver.positions && storage.solve->solver.velocities));
}

bool ab_world_reset_body_mass(AbPhysicsWorld *world, size_t body,
                              size_t fixture_limit) {
    AbWorldStorage *storage = &world->storage;
    size_t count = 0;
    for (size_t index = fixture_limit; index > 0;) {
        AbWorldFixture *fixture = storage->fixtures + world->fixture_order[--index];
        if (fixture->body != body) {
            continue;
        }
        AbFixtureMass *mass = storage->mass_scratch + count++;
        *mass = (AbFixtureMass){.density = fixture->density};
        if (fixture->density != 0) {
            if (fixture->shape.kind == AB_SHAPE_CIRCLE) {
                mass->data =
                    ab_circle_mass(fixture->shape.data.circle, fixture->density);
            } else {
                mass->data =
                    ab_polygon_mass(&fixture->shape.data.polygon, fixture->density);
            }
            if (!isfinite(mass->data.mass) || !isfinite(mass->data.center.x) ||
                !isfinite(mass->data.center.y) || !isfinite(mass->data.inertia)) {
                return false;
            }
        }
    }
    ab_body_mass_reset(&storage->bodies[body].motion.mass, storage->mass_scratch,
                       count);
    return true;
}

bool ab_world_open(AbPhysicsWorld *world, AbWorldDefinition definition,
                   AbWorldStorage storage, AbError *error) {
    if (!world) {
        return ab_fail(error, "Missing physics world output");
    }
    if (!storage.body_capacity) {
        storage.body_capacity = storage.body_count;
    }
    if (!storage.fixture_capacity) {
        storage.fixture_capacity = storage.fixture_count;
    }
    if (!storage.joint_capacity) {
        storage.joint_capacity = storage.joint_count;
    }
    *world = (AbPhysicsWorld){.storage = storage, .definition = definition};
    if (!valid_storage(storage) || !isfinite(definition.gravity.x) ||
        !isfinite(definition.gravity.y) || !isfinite(definition.maximum_translation) ||
        definition.maximum_translation < 0 ||
        !isfinite(definition.maximum_translation_squared) ||
        definition.maximum_translation_squared < 0) {
        world->failed = true;
        return ab_fail(error, "Invalid physics world storage or gravity");
    }
    for (size_t body = 0; body < storage.body_count; ++body) {
        if ((unsigned)storage.bodies[body].motion.mass.type > AB_BODY_DYNAMIC) {
            world->failed = true;
            return ab_fail(error, "Invalid physics body type");
        }
    }
    for (size_t index = 0; index < storage.fixture_count; ++index) {
        const AbWorldFixture *fixture = storage.fixtures + index;
        if (fixture->body >= storage.body_count ||
            !ab_world_fixture_valid(world, fixture)) {
            world->failed = true;
            return ab_fail(error, "Invalid physics fixture definition");
        }
    }
    for (size_t index = 0; index < storage.joint_count; ++index) {
        const AbWorldSolveJoint *joint = storage.joints + index;
        if (joint->first_body >= storage.body_count ||
            joint->second_body >= storage.body_count ||
            !ab_world_joint_definition_valid(joint)) {
            world->failed = true;
            return ab_fail(error, "Invalid physics joint definition");
        }
    }
    if (!ab_broadphase_open(definition.broadphase_limits, &world->broadphase, error)) {
        world->failed = true;
        return false;
    }
    if (storage.body_capacity) {
        world->body_order = malloc(storage.body_capacity * sizeof(*world->body_order));
        world->body_alive = calloc(storage.body_capacity, sizeof(*world->body_alive));
    }
    if (storage.fixture_capacity) {
        world->fixture_order =
            malloc(storage.fixture_capacity * sizeof(*world->fixture_order));
        world->fixture_alive =
            calloc(storage.fixture_capacity, sizeof(*world->fixture_alive));
    }
    if (storage.joint_capacity) {
        world->joint_order =
            malloc(storage.joint_capacity * sizeof(*world->joint_order));
        world->joint_alive =
            calloc(storage.joint_capacity, sizeof(*world->joint_alive));
    }
    if ((storage.body_capacity && (!world->body_order || !world->body_alive)) ||
        (storage.fixture_capacity &&
         (!world->fixture_order || !world->fixture_alive)) ||
        (storage.joint_capacity && (!world->joint_order || !world->joint_alive))) {
        world->failed = true;
        return ab_fail(error, "Cannot allocate physics world slot metadata");
    }
    world->body_count = storage.body_count;
    world->body_slot_count = storage.body_count;
    for (size_t body = 0; body < storage.body_count; ++body) {
        world->body_order[body] = body;
        world->body_alive[body] = true;
    }
    world->joint_count = storage.joint_count;
    world->joint_slot_count = storage.joint_count;
    for (size_t index = 0; index < storage.joint_count; ++index) {
        world->joint_order[index] = index;
        world->joint_alive[index] = true;
        ab_world_joint_reset(storage.joints + index);
    }
    for (size_t index = 0; index < storage.fixture_count; ++index) {
        if (!ab_world_attach_fixture(world, index, error)) {
            world->failed = true;
            return ab_fail(error, "Cannot attach physics fixture");
        }
    }
    world->new_fixtures = storage.fixture_count != 0;
    return true;
}

bool ab_world_apply_force(AbPhysicsWorld *world, size_t body_index, AbVec2 force,
                          float torque) {
    if (!world || world->failed || !ab_world_has_body(world, body_index) ||
        !isfinite(force.x) || !isfinite(force.y) || !isfinite(torque)) {
        return false;
    }
    AbBodyMotion *body = &world->storage.bodies[body_index].motion;
    if (body->mass.type != AB_BODY_DYNAMIC) {
        return true;
    }
    AbVec2 accumulated = {body->force.x + force.x, body->force.y + force.y};
    float accumulated_torque = body->torque + torque;
    if (!isfinite(accumulated.x) || !isfinite(accumulated.y) ||
        !isfinite(accumulated_torque)) {
        return false;
    }
    ab_world_wake_body(body);
    body->force = accumulated;
    body->torque = accumulated_torque;
    return true;
}
