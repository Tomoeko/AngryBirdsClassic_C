#ifndef AB_PHYSICS_REVOLUTE_H
#define AB_PHYSICS_REVOLUTE_H

#include "ab/physics_joint.h"
#include "ab/physics_matrix.h"
#include "ab/physics_solver.h"

typedef struct {
    AbVec2 local_anchors[2];
    float reference_angle;
    float lower_angle;
    float upper_angle;
    float motor_speed;
    float maximum_motor_torque;
    bool enable_motor;
    bool enable_limit;
} AbRevoluteDefinition;

typedef struct {
    AbVec2 offsets[2];
    AbVec2 local_centers[2];
    float inverse_mass[2];
    float inverse_inertia[2];
    AbSymmetric3 matrix;
    float motor_mass;
    AbVec3 impulse;
    float motor_impulse;
    AbJointLimitState limit;
} AbRevoluteState;

/* Initialize the coupled anchor matrix, motor mass, angular limit state and
 * warm-start impulses. Bounds may be reversed; keep native width classification
 * and boundary ordering. Warm start reads preceding impulses and limit state;
 * otherwise state may be uninitialized. Required parameters/velocities and all
 * results must be finite. Inverse masses/inertias, duration and maximum torque
 * are nonnegative. Angles use the measured rotation domain. Disjoint storage;
 * failure preserves state/velocities. No allocation or world changes. */
bool ab_revolute_velocity_initialize(const AbRevoluteDefinition *definition,
                                     const AbJointSolverBody bodies[2],
                                     AbJointStep step, AbRevoluteState *state,
                                     AbVelocity velocities[2], AbError *error);

/* Run one initialized velocity iteration, solving the clamped motor before
 * coupled active angular limits or free anchor motion. Only retained impulses
 * and velocities change. Matching definition/cache must stay stable between
 * passes. Required parameters/state/results are finite, inverse masses/inertias,
 * seconds and maximum torque nonnegative. Disjoint storage; failure preserves
 * state and velocities. No allocation or position changes. */
bool ab_revolute_velocity_solve(const AbRevoluteDefinition *definition,
                                AbRevoluteState *state, float seconds,
                                AbVelocity velocities[2], AbError *error);

/* Apply the cached angular limit first, then recompute rotated anchors and
 * correct their separation. Use the initialized limit state and motor mass;
 * state/velocities remain unchanged. Return convergence from native angular
 * and pre-linear-correction errors. Required inputs/results must be finite,
 * cached inverse masses/inertias nonnegative, and angles within the measured
 * rotation domain. Disjoint storage; invalid results preserve positions. */
AbPositionResult ab_revolute_position_solve(const AbRevoluteDefinition *definition,
                                            const AbRevoluteState *state,
                                            AbPositionState positions[2],
                                            AbError *error);

#endif
