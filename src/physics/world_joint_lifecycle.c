#include "world_internal.h"

#include <string.h>

bool ab_world_has_joint(const AbPhysicsWorld *world, size_t joint) {
    return world && joint < world->joint_slot_count && world->joint_alive &&
           world->joint_alive[joint];
}

static void flag_connected_contacts(AbPhysicsWorld *world,
                                    const AbWorldSolveJoint *joint) {
    if (joint->collide_connected) {
        return;
    }
    for (size_t index = 0; index < world->contact_count; ++index) {
        AbWorldSolveContact *contact = world->storage.contacts + index;
        if ((contact->first_body == joint->first_body &&
             contact->second_body == joint->second_body) ||
            (contact->first_body == joint->second_body &&
             contact->second_body == joint->first_body)) {
            contact->state.flags |= AB_CONTACT_FILTER;
        }
    }
}

bool ab_world_create_joint(AbPhysicsWorld *world, const AbWorldSolveJoint *definition,
                           size_t *output, AbError *error) {
    if (!output || !ab_world_mutable(world) ||
        !ab_world_joint_definition_valid(definition) ||
        !ab_world_has_body(world, definition->first_body) ||
        !ab_world_has_body(world, definition->second_body)) {
        return ab_fail(error, "Invalid physics joint creation");
    }
    size_t slot = ab_world_unused_slot(world->joint_alive, world->joint_slot_count,
                                       world->storage.joint_capacity);
    if (slot == SIZE_MAX) {
        return ab_fail(error, "Physics joint capacity exhausted");
    }
    world->storage.joints[slot] = *definition;
    ab_world_joint_reset(world->storage.joints + slot);
    memmove(world->joint_order + 1, world->joint_order,
            world->joint_count * sizeof(*world->joint_order));
    world->joint_order[0] = slot;
    world->joint_alive[slot] = true;
    ++world->joint_count;
    if (slot >= world->joint_slot_count) {
        world->joint_slot_count = slot + 1;
    }
    flag_connected_contacts(world, world->storage.joints + slot);
    *output = slot;
    return true;
}

bool ab_world_destroy_joint(AbPhysicsWorld *world, size_t index, AbError *error) {
    if (!ab_world_mutable(world) || !ab_world_has_joint(world, index)) {
        return ab_fail(error, "Invalid physics joint destruction");
    }
    AbWorldSolveJoint *joint = world->storage.joints + index;
    ab_world_unlink_slot(world->joint_order, &world->joint_count, index);
    ab_world_wake_body(&world->storage.bodies[joint->first_body].motion);
    ab_world_wake_body(&world->storage.bodies[joint->second_body].motion);
    flag_connected_contacts(world, joint);
    world->joint_alive[index] = false;
    *joint = (AbWorldSolveJoint){0};
    return true;
}

bool ab_world_remove_body_joints(AbPhysicsWorld *world, size_t body, AbError *error) {
    size_t cursor = 0;
    while (cursor < world->joint_count) {
        size_t index = world->joint_order[cursor];
        const AbWorldSolveJoint *joint = world->storage.joints + index;
        if (joint->first_body != body && joint->second_body != body) {
            ++cursor;
            continue;
        }
        if (world->listener.destroy_joint) {
            world->listener.destroy_joint(world->listener.context, world, index);
        }
        if (!ab_world_destroy_joint(world, index, error)) {
            return false;
        }
    }
    return true;
}
