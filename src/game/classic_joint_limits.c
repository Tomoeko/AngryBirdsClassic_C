#include "ab/classic_joints.h"

#include "classic_joints_internal.h"

#include <math.h>

static bool check_joint(AbPhysicsWorld *world, size_t index, AbError *error) {
    if (!ab_world_has_joint(world, index)) {
        return ab_fail(error, "Classic motor record has no live engine joint");
    }
    AbWorldSolveJoint *joint = world->storage.joints + index;
    const AbJointConstraint *constraint = &joint->constraint;
    if (constraint->kind != AB_JOINT_REVOLUTE &&
        constraint->kind != AB_JOINT_PRISMATIC) {
        return true;
    }
    const size_t indices[] = {joint->first_body, joint->second_body};
    AbJointBodyFrame frames[2];
    for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
        if (!ab_world_has_body(world, indices[endpoint])) {
            return ab_fail(error, "Missing classic motor endpoint");
        }
        const AbBodyMassState *body =
            &world->storage.bodies[indices[endpoint]].motion.mass;
        frames[endpoint] = (AbJointBodyFrame){body->transform, body->angle};
    }
    float coordinate;
    if (!ab_joint_constraint_coordinate(constraint, frames, &coordinate, error)) {
        return false;
    }
    float lower, upper, speed;
    bool enabled;
    if (constraint->kind == AB_JOINT_REVOLUTE) {
        const AbRevoluteDefinition *definition = &constraint->definition.revolute;
        lower = definition->lower_angle;
        upper = definition->upper_angle;
        speed = definition->motor_speed;
        enabled = definition->enable_motor && definition->enable_limit;
    } else {
        const AbPrismaticDefinition *definition = &constraint->definition.prismatic;
        lower = definition->lower_translation;
        upper = definition->upper_translation;
        speed = definition->motor_speed;
        enabled = definition->enable_motor && definition->enable_limit;
    }
    if (!isfinite(lower) || !isfinite(upper) || !isfinite(speed)) {
        return ab_fail(error, "Nonfinite classic motor parameters");
    }
    bool reverse =
        (speed > 0 && coordinate >= upper) || (speed < 0 && coordinate <= lower);
    return !enabled || !reverse ||
           ab_world_set_joint_motor_speed(world, index, -speed, error);
}

bool ab_classic_joints_check_limits(AbClassicJoints *joints, AbSpan name,
                                    AbError *error) {
    if (!ab_classic_joints_healthy(joints) || (name.size && !name.data)) {
        return ab_fail(error, "Invalid classic motor registry/name");
    }
    for (size_t index = 0; index < joints->joint_count; ++index) {
        const AbClassicJointRecord *record = joints->storage.joints + index;
        if (ab_classic_joint_name_matches(record->definition.name, name) &&
            !check_joint(joints->world, record->world_joint, error)) {
            joints->failed = true;
            return false;
        }
    }
    return true;
}
