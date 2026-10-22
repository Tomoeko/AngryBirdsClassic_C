#ifndef AB_PHYSICS_PRISMATIC_H
#define AB_PHYSICS_PRISMATIC_H

#include "ab/physics_joint.h"
#include "ab/physics_matrix.h"
#include "ab/physics_solver.h"

typedef struct {
    AbVec2 axis;
    AbVec2 perpendicular;
} AbPrismaticAxes;

typedef struct {
    AbVec2 local_anchors[2];
    AbPrismaticAxes local;
    float reference_angle;
    float lower_translation;
    float upper_translation;
    float maximum_motor_force;
    float motor_speed;
    bool enable_limit;
    bool enable_motor;
} AbPrismaticDefinition;

typedef struct {
    AbVec2 local_centers[2];
    float inverse_mass[2];
    float inverse_inertia[2];
    AbVec2 axis;
    AbVec2 perpendicular;
    float perpendicular_arms[2];
    float axis_arms[2];
    AbSymmetric3 matrix;
    float motor_mass;
    AbVec3 impulse;
    float motor_impulse;
    AbJointLimitState limit;
} AbPrismaticState;

/* Prepare the constructor's local axes once from the raw definition axis.
 * Normalize at the target threshold; shorter axes retain their components.
 * Perpendicular is (-axis.y, axis.x), preserving signed zeros. Finite inputs
 * and results, disjoint storage; arithmetic failure preserves output. */
bool ab_prismatic_axes_prepare(AbVec2 local_axis, AbPrismaticAxes *output,
                               AbError *error);

/* Initialize with constructor-prepared local axes and two resolved body frames
 * and world centers. Cache world axes, lever arms, coupled matrix, motor mass,
 * limit state and scaled retained impulses. Warm start reads prior impulses
 * and limit state; otherwise state may be uninitialized. Required parameters,
 * velocities and results must be finite; inverse masses/inertias, duration and
 * maximum force nonnegative. Bounds may be reversed. Angles use the measured
 * rotation domain. Disjoint storage; failure preserves state and velocities.
 * No allocation or world membership changes. */
bool ab_prismatic_velocity_initialize(const AbPrismaticDefinition *definition,
                                      const AbJointSolverBody bodies[2],
                                      const AbVec2 centers[2], AbJointStep step,
                                      AbPrismaticState *state, AbVelocity velocities[2],
                                      AbError *error);

/* Run one initialized velocity pass: clamped motor, then perpendicular/angular
 * constraints with the active translation limit. Only retained impulses and
 * body velocities change. Required parameters/state/results must be finite;
 * inverse masses/inertias, seconds and maximum force are nonnegative. Keep the
 * matching definition/cache stable. Disjoint storage; failure preserves state
 * and velocities. No allocation, positions or world membership changes. */
bool ab_prismatic_velocity_solve(const AbPrismaticDefinition *definition,
                                 AbPrismaticState *state, float seconds,
                                 AbVelocity velocities[2], AbError *error);

/* Recompute axes, lever arms and constraints at current positions. Translation
 * limits use the current projection, independently of cached limit state. Keep
 * constructor-prepared local axes and cached centers/inverse masses/inertias.
 * State and velocities stay unchanged. Required inputs/results must be finite,
 * inverse masses/inertias nonnegative, angles in the measured rotation domain.
 * Disjoint storage; invalid results preserve positions. No allocation. */
AbPositionResult ab_prismatic_position_solve(const AbPrismaticDefinition *definition,
                                             const AbPrismaticState *state,
                                             AbPositionState positions[2],
                                             AbError *error);

#endif
