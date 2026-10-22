#include "ab/classic_joints.h"

#include "classic_joints_internal.h"

static bool remove_connected(AbClassicJoints *joints, size_t body, AbError *error) {
    AbPhysicsWorld *world = joints->world;
    size_t cursor = 0;
    while (cursor < world->joint_count) {
        size_t slot = world->joint_order[cursor];
        const AbWorldSolveJoint *joint = world->storage.joints + slot;
        if (joint->first_body != body && joint->second_body != body) {
            ++cursor;
            continue;
        }
        size_t previous_count = world->joint_count;
        if (!ab_classic_joints_remove(joints, slot, error)) {
            return false;
        }
        if (world->joint_count == previous_count) {
            ++cursor;
        }
    }
    return true;
}

static bool create_glue(AbClassicJoints *joints, AbClassicGluePair pair,
                        AbError *error) {
    AbPhysicsWorld *world = joints->world;
    if (!remove_connected(joints, pair.second_body, error)) {
        return false;
    }
    if (world->locked) {
        return true;
    }
    const size_t indices[] = {pair.first_body, pair.second_body};
    AbJointBodyFrame frames[2];
    for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
        const AbBodyMassState *body =
            &world->storage.bodies[indices[endpoint]].motion.mass;
        frames[endpoint] = (AbJointBodyFrame){body->transform, body->angle};
    }
    AbWorldSolveJoint definition;
    size_t slot;
    return ab_classic_glue_definition(frames, indices, &definition, error) &&
           ab_world_create_joint(world, &definition, &slot, error);
}

bool ab_classic_joints_glue(AbClassicJoints *joints, const AbClassicGluePair *pairs,
                            size_t *count, AbError *error) {
    if (!count || !ab_classic_joints_healthy(joints) || (*count && !pairs) ||
        *count > SIZE_MAX / sizeof(*pairs)) {
        return ab_fail(error, "Invalid classic glue queue/storage");
    }
    for (size_t index = 0; index < *count; ++index) {
        if (pairs[index].first_body == pairs[index].second_body ||
            !ab_world_has_body(joints->world, pairs[index].first_body) ||
            !ab_world_has_body(joints->world, pairs[index].second_body)) {
            return ab_fail(error, "Invalid classic glue pair");
        }
    }
    for (size_t index = 0; index < *count; ++index) {
        if (!create_glue(joints, pairs[index], error)) {
            joints->failed = true;
            return false;
        }
    }
    *count = 0;
    return true;
}
