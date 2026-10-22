#ifndef AB_PHYSICS_WELD_H
#define AB_PHYSICS_WELD_H

#include "ab/physics_joint.h"
#include "ab/physics_matrix.h"
#include "ab/physics_solver.h"

typedef struct {
    AbVec2 local_anchors[2];
    float reference_angle;
    float frequency;
    float damping_ratio;
} AbWeldDefinition;

typedef struct {
    AbVec2 offsets[2];
    AbVec2 local_centers[2];
    float inverse_mass[2];
    float inverse_inertia[2];
    AbSymmetric3 inverse;
    AbVec3 impulse;
    float gamma;
    float bias;
} AbWeldState;

/* Initialize one weld velocity constraint from two resolved island bodies.
 * Nonpositive frequency selects the rigid route; positive frequency adds the
 * native angular spring/damping terms. Angles use the rotation module's measured
 * domain. Require finite parameters/velocities and nonnegative inverse mass,
 * inverse inertia and duration. time_ratio scales old impulse when warm_start
 * is true; only that preceding state field is read. Without warm-start, state
 * may be uninitialized and its impulse becomes positive zero. All cached fields
 * are replaced. Storage is disjoint; failure preserves state and velocities.
 * No allocation, joint membership, solver iteration or position changes. */
bool ab_weld_velocity_initialize(const AbWeldDefinition *definition,
                                 const AbJointSolverBody bodies[2], AbJointStep step,
                                 AbWeldState *state, AbVelocity velocities[2],
                                 AbError *error);

/* Run one velocity iteration over the initialized constraint and two distinct
 * body velocities. Keep the matching definition and cache stable between
 * iterations. Only impulse and velocities change; frequency selects the native
 * rigid or angular-spring route. Finite state/results and nonnegative cached
 * inverse masses/inertias are required. No allocation or position changes.
 * Disjoint storage; failure preserves both state and velocities. */
bool ab_weld_velocity_solve(const AbWeldDefinition *definition, AbWeldState *state,
                            AbVelocity velocities[2], AbError *error);

/* Run one position correction with the initialized body's cached centers and
 * inverse masses/inertias. Recompute rotated offsets and the constraint matrix
 * at current angles. Spring joints correct linear error; rigid joints also
 * correct angular error when angular mass exists. Return convergence from the
 * pre-correction errors, using the target's exact thresholds. State is borrowed
 * unchanged; disjoint positions commit only on a valid finite result. Angles
 * use the measured rotation domain. No allocation or velocity changes. */
AbPositionResult ab_weld_position_solve(const AbWeldDefinition *definition,
                                        const AbWeldState *state,
                                        AbPositionState positions[2], AbError *error);

#endif
