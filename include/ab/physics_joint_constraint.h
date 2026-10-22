#ifndef AB_PHYSICS_JOINT_CONSTRAINT_H
#define AB_PHYSICS_JOINT_CONSTRAINT_H

#include "ab/physics_distance_joint.h"
#include "ab/physics_prismatic.h"
#include "ab/physics_revolute.h"
#include "ab/physics_weld.h"

typedef struct {
    AbJointKind kind;
    union {
        AbDistanceDefinition distance;
        AbWeldDefinition weld;
        AbRevoluteDefinition revolute;
        AbPrismaticDefinition prismatic;
    } definition;
    union {
        AbDistanceState distance;
        AbWeldState weld;
        AbRevoluteState revolute;
        AbPrismaticState prismatic;
    } state;
} AbJointConstraint;

/* Dispatch a resolved constraint without allocation or world membership.
 * Keep kind/definition stable and constructor-prepare prismatic axes. Initialize
 * from explicit body frames/world centers before iteration; warm_start reads
 * the active state's retained impulses/limit according to its joint contract.
 * With warm_start false, state may be uninitialized. Unused union members are
 * never read. Required input, numeric, rotation and disjoint-storage contracts
 * of the selected joint apply. Failure preserves active state and velocities. */
bool ab_joint_constraint_initialize(AbJointConstraint *joint,
                                    const AbJointSolverBody bodies[2],
                                    const AbVec2 centers[2], AbJointStep step,
                                    AbVelocity velocities[2], AbError *error);
bool ab_joint_constraint_velocity_solve(AbJointConstraint *joint, float seconds,
                                        AbVelocity velocities[2], AbError *error);
AbPositionResult ab_joint_constraint_position_solve(const AbJointConstraint *joint,
                                                    AbPositionState positions[2],
                                                    AbError *error);

/* Current revolute angle or prismatic translation, using body angles or
 * transforms and constructor geometry. Cached solver state is not read.
 * Other joint kinds are unsupported. Required inputs/results must be finite;
 * invalid inputs preserve output and all source state.
 */
bool ab_joint_constraint_coordinate(const AbJointConstraint *joint,
                                    const AbJointBodyFrame bodies[2], float *output,
                                    AbError *error);

#endif
