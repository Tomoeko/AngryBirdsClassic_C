#include "world_internal.h"

#include "ab/rotation.h"
#include "internal.h"

#include <math.h>

static void report_contact(void *context, size_t contact,
                           const AbContactImpulse *impulse) {
    AbPhysicsWorld *world = context;
    world->listener.post_solve(world->listener.context, world, contact, impulse);
}

bool ab_world_clear_forces(AbPhysicsWorld *world, AbError *error) {
    if (!world || world->failed || !world->broadphase.tree.nodes ||
        world->body_count > world->body_slot_count ||
        world->body_slot_count > world->storage.body_capacity ||
        (world->body_count &&
         (!world->storage.bodies || !world->body_order || !world->body_alive))) {
        return ab_fail(error, "Invalid physics force-clear storage");
    }
    for (size_t cursor = 0; cursor < world->body_count; ++cursor) {
        if (!ab_world_has_body(world, world->body_order[cursor])) {
            return ab_fail(error, "Invalid force-clear body at index %zu", cursor);
        }
    }
    for (size_t cursor = 0; cursor < world->body_count; ++cursor) {
        AbBodyMotion *body = &world->storage.bodies[world->body_order[cursor]].motion;
        body->force = (AbVec2){0, 0};
        body->torque = 0;
    }
    return true;
}

bool ab_world_synchronize_body(AbPhysicsWorld *world, size_t index, AbError *error) {
    const AbWorldSolveBody *body = world->storage.bodies + index;
    const AbBodyMassState *mass = &body->motion.mass;
    AbRotation rotation;
    if (!ab_rotation_from_angle(mass->previous_angle, &rotation)) {
        return ab_fail(error, "Unsupported previous world body angle");
    }
    AbRigidTransform previous = {.sine = rotation.sine, .cosine = rotation.cosine};
    AbVec2 offset = ab_physics_rotate(mass->local_center, previous);
    previous.position = (AbVec2){mass->previous_center.x - offset.x,
                                 mass->previous_center.y - offset.y};
    for (size_t cursor = world->fixture_count; cursor > 0;) {
        AbWorldFixture *fixture =
            world->storage.fixtures + world->fixture_order[--cursor];
        if (fixture->body == index &&
            !ab_fixture_proxy_sync(&fixture->proxy, &fixture->shape, previous,
                                   mass->transform, &world->broadphase, error)) {
            return false;
        }
    }
    return true;
}

static bool synchronize_fixtures(AbPhysicsWorld *world, AbError *error) {
    for (size_t cursor = 0; cursor < world->body_count; ++cursor) {
        size_t index = world->body_order[cursor];
        const AbWorldSolveBody *body = world->storage.bodies + index;
        if (body->island && body->motion.mass.type != AB_BODY_STATIC &&
            !ab_world_synchronize_body(world, index, error)) {
            return false;
        }
    }
    return true;
}

bool ab_world_step(AbPhysicsWorld *world, float seconds, size_t velocity_iterations,
                   size_t position_iterations, AbError *error) {
    if (!world || world->failed || world->locked || !world->broadphase.tree.nodes ||
        !isfinite(seconds) || seconds < 0) {
        return ab_fail(error, "Invalid physics world step");
    }
    float inverse_seconds = seconds > 0 ? 1.0f / seconds : 0;
    float time_ratio = world->inverse_previous_step * seconds;
    if (!isfinite(inverse_seconds) || !isfinite(time_ratio)) {
        return ab_fail(error, "Unsupported physics step range");
    }
    if (world->new_fixtures) {
        if (!ab_world_find_contacts(world, error)) {
            return false;
        }
        world->new_fixtures = false;
    }
    world->locked = true;
    bool okay = ab_world_refresh_contacts(world, error);
    if (okay && seconds > 0) {
        AbIslandStep step = {
            .motion = {.seconds = seconds,
                       .gravity = world->definition.gravity,
                       .maximum_translation = world->definition.maximum_translation,
                       .maximum_translation_squared =
                           world->definition.maximum_translation_squared,
                       .position_iterations = position_iterations,
                       .allow_sleep = world->definition.allow_sleep},
            .velocity_iterations = velocity_iterations,
            .time_ratio = time_ratio,
            .warm_start = world->definition.warm_start,
            .report = world->listener.post_solve ? report_contact : NULL,
            .report_context = world,
        };
        AbWorldJointGraph graph = {world->storage.joints, world->joint_slot_count,
                                   world->joint_order, world->joint_count};
        okay = ab_world_solve_ordered_joints(
            world->storage.bodies, world->body_slot_count, world->body_order,
            world->body_count, world->storage.contacts, world->contact_count, graph,
            &step, world->storage.solve);
        if (!okay) {
            ab_fail(error, "Discrete world island solve failed");
        } else {
            okay = synchronize_fixtures(world, error) &&
                   ab_world_find_contacts(world, error);
            if (okay && world->definition.continuous) {
                okay = ab_world_solve_continuous(world, &step, error);
            }
        }
    }
    if (okay) {
        if (seconds > 0) {
            world->inverse_previous_step = inverse_seconds;
        }
        if (world->definition.clear_forces) {
            okay = ab_world_clear_forces(world, error);
        }
    }
    if (!okay) {
        world->failed = true;
    }
    world->locked = false;
    return okay;
}
