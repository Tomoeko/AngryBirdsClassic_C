#include "island_internal.h"

bool ab_island_joints_valid(const AbIslandJoint *joints, size_t joint_count,
                            size_t body_count, const AbIslandWorkspace *workspace) {
    if (!joint_count) {
        return true;
    }
    if (!joints || !workspace || workspace->body_capacity < body_count ||
        !workspace->positions || !workspace->velocities) {
        return false;
    }
    for (size_t index = 0; index < joint_count; ++index) {
        const AbIslandJoint *joint = joints + index;
        if (!joint->constraint ||
            (unsigned)joint->constraint->kind > AB_JOINT_PRISMATIC ||
            joint->first_body >= body_count || joint->second_body >= body_count ||
            joint->first_body == joint->second_body) {
            return false;
        }
    }
    return true;
}

bool ab_island_initialize_joints(const AbBodyMotion *bodies,
                                 const AbIslandJoint *joints, size_t joint_count,
                                 const AbIslandStep *step,
                                 AbIslandWorkspace *workspace) {
    AbJointStep joint_step = {step->motion.seconds, step->time_ratio, step->warm_start};
    for (size_t index = 0; index < joint_count; ++index) {
        const AbIslandJoint *joint = joints + index;
        const size_t endpoints[] = {joint->first_body, joint->second_body};
        AbJointSolverBody frames[2];
        AbVec2 centers[2];
        AbVelocity velocities[2];
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            const AbBodyMassState *mass = &bodies[endpoints[endpoint]].mass;
            frames[endpoint] =
                (AbJointSolverBody){mass->local_center, mass->angle, mass->inverse_mass,
                                    mass->inverse_inertia};
            centers[endpoint] = mass->center;
            velocities[endpoint] = workspace->velocities[endpoints[endpoint]];
        }
        if (!ab_joint_constraint_initialize(joint->constraint, frames, centers,
                                            joint_step, velocities, NULL)) {
            return false;
        }
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            workspace->velocities[endpoints[endpoint]] = velocities[endpoint];
        }
    }
    return true;
}

bool ab_island_solve_joint_velocities(const AbIslandJoint *joints, size_t joint_count,
                                      float seconds, AbIslandWorkspace *workspace) {
    for (size_t index = 0; index < joint_count; ++index) {
        const AbIslandJoint *joint = joints + index;
        AbVelocity velocities[] = {workspace->velocities[joint->first_body],
                                   workspace->velocities[joint->second_body]};
        if (!ab_joint_constraint_velocity_solve(joint->constraint, seconds, velocities,
                                                NULL)) {
            return false;
        }
        workspace->velocities[joint->first_body] = velocities[0];
        workspace->velocities[joint->second_body] = velocities[1];
    }
    return true;
}

AbPositionResult ab_island_solve_joint_positions(const AbIslandJoint *joints,
                                                 size_t joint_count,
                                                 AbIslandWorkspace *workspace) {
    AbPositionResult result = AB_POSITION_RESOLVED;
    for (size_t index = 0; index < joint_count; ++index) {
        const AbIslandJoint *joint = joints + index;
        AbPositionState positions[] = {workspace->positions[joint->first_body],
                                       workspace->positions[joint->second_body]};
        AbPositionResult current =
            ab_joint_constraint_position_solve(joint->constraint, positions, NULL);
        if (current == AB_POSITION_INVALID) {
            return current;
        }
        workspace->positions[joint->first_body] = positions[0];
        workspace->positions[joint->second_body] = positions[1];
        if (current == AB_POSITION_UNRESOLVED) {
            result = current;
        }
    }
    return result;
}
