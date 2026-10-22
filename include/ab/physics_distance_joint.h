#ifndef AB_PHYSICS_DISTANCE_JOINT_H
#define AB_PHYSICS_DISTANCE_JOINT_H

#include "ab/physics_joint.h"
#include "ab/physics_solver.h"

typedef struct {
    AbVec2 local_anchors[2];
    float length;
    float frequency;
    float damping_ratio;
} AbDistanceDefinition;

typedef struct {
    AbVec2 offsets[2];
    AbVec2 local_centers[2];
    float inverse_mass[2];
    float inverse_inertia[2];
    AbVec2 axis;
    float mass;
    float gamma;
    float bias;
    float impulse;
} AbDistanceState;

/* Initialize from two resolved bodies and their world centers. Positive
 * frequency enables spring/damping; nonpositive frequency is rigid. Warm start
 * reads only the preceding impulse and scales it by time_ratio; otherwise state
 * may be uninitialized. Required inputs/results must be finite, inverse masses,
 * inertias, length and duration nonnegative. Angles use the measured rotation
 * domain. Disjoint storage; failure preserves state and velocities. No allocation
 * or world membership changes. */
bool ab_distance_velocity_initialize(const AbDistanceDefinition *definition,
                                     const AbJointSolverBody bodies[2],
                                     const AbVec2 centers[2], AbJointStep step,
                                     AbDistanceState *state, AbVelocity velocities[2],
                                     AbError *error);

/* Carry initialized state through one velocity iteration. Only retained impulse
 * and velocities change. Matching definition/cache must stay stable between
 * passes. Finite state/results and nonnegative cached inverse masses/inertias
 * required. Disjoint storage; failure preserves state and velocities. */
bool ab_distance_velocity_solve(AbDistanceState *state, AbVelocity velocities[2],
                                AbError *error);

/* Correct rigid distance with current rotated anchors and the initialized
 * effective mass. Spring mode returns resolved without changing positions.
 * Native convergence uses the magnitude of the clamped pre-correction length
 * error and a strict comparison. State stays unchanged. Required finite inputs,
 * nonnegative masses/inertias/length, disjoint storage and measured angle domain;
 * failure preserves positions. No allocation or velocity changes. */
AbPositionResult ab_distance_position_solve(const AbDistanceDefinition *definition,
                                            const AbDistanceState *state,
                                            AbPositionState positions[2],
                                            AbError *error);

#endif
