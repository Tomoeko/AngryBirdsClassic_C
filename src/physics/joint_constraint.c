#include "ab/physics_joint_constraint.h"

bool ab_joint_constraint_initialize(AbJointConstraint *joint,
                                    const AbJointSolverBody bodies[2],
                                    const AbVec2 centers[2], AbJointStep step,
                                    AbVelocity velocities[2], AbError *error) {
    if (!joint || !bodies || !centers || !velocities) {
        return ab_fail(error, "Invalid joint initialization storage");
    }
    switch (joint->kind) {
        case AB_JOINT_DISTANCE:
            return ab_distance_velocity_initialize(
                &joint->definition.distance, bodies, centers, step,
                &joint->state.distance, velocities, error);
        case AB_JOINT_WELD:
            return ab_weld_velocity_initialize(&joint->definition.weld, bodies, step,
                                               &joint->state.weld, velocities, error);
        case AB_JOINT_REVOLUTE:
            return ab_revolute_velocity_initialize(&joint->definition.revolute, bodies,
                                                   step, &joint->state.revolute,
                                                   velocities, error);
        case AB_JOINT_PRISMATIC:
            return ab_prismatic_velocity_initialize(
                &joint->definition.prismatic, bodies, centers, step,
                &joint->state.prismatic, velocities, error);
        default:
            return ab_fail(error, "Unsupported joint constraint kind");
    }
}

bool ab_joint_constraint_velocity_solve(AbJointConstraint *joint, float seconds,
                                        AbVelocity velocities[2], AbError *error) {
    if (!joint || !velocities) {
        return ab_fail(error, "Invalid joint velocity storage");
    }
    switch (joint->kind) {
        case AB_JOINT_DISTANCE:
            return ab_distance_velocity_solve(&joint->state.distance, velocities,
                                              error);
        case AB_JOINT_WELD:
            return ab_weld_velocity_solve(&joint->definition.weld, &joint->state.weld,
                                          velocities, error);
        case AB_JOINT_REVOLUTE:
            return ab_revolute_velocity_solve(&joint->definition.revolute,
                                              &joint->state.revolute, seconds,
                                              velocities, error);
        case AB_JOINT_PRISMATIC:
            return ab_prismatic_velocity_solve(&joint->definition.prismatic,
                                               &joint->state.prismatic, seconds,
                                               velocities, error);
        default:
            return ab_fail(error, "Unsupported joint constraint kind");
    }
}

AbPositionResult ab_joint_constraint_position_solve(const AbJointConstraint *joint,
                                                    AbPositionState positions[2],
                                                    AbError *error) {
    if (!joint || !positions) {
        ab_fail(error, "Invalid joint position storage");
        return AB_POSITION_INVALID;
    }
    switch (joint->kind) {
        case AB_JOINT_DISTANCE:
            return ab_distance_position_solve(&joint->definition.distance,
                                              &joint->state.distance, positions, error);
        case AB_JOINT_WELD:
            return ab_weld_position_solve(&joint->definition.weld, &joint->state.weld,
                                          positions, error);
        case AB_JOINT_REVOLUTE:
            return ab_revolute_position_solve(&joint->definition.revolute,
                                              &joint->state.revolute, positions, error);
        case AB_JOINT_PRISMATIC:
            return ab_prismatic_position_solve(&joint->definition.prismatic,
                                               &joint->state.prismatic, positions,
                                               error);
        default:
            ab_fail(error, "Unsupported joint constraint kind");
            return AB_POSITION_INVALID;
    }
}
