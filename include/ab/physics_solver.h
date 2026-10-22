#ifndef AB_PHYSICS_SOLVER_H
#define AB_PHYSICS_SOLVER_H

#include "ab/physics_geometry.h"
#include "ab/physics_contact.h"

typedef struct {
    AbVec2 linear;
    float angular;
} AbVelocity;

typedef struct {
    AbVec2 first_offset;
    AbVec2 second_offset;
    float normal_impulse;
    float tangent_impulse;
    float normal_mass;
    float tangent_mass;
    float velocity_bias;
} AbVelocityPoint;

typedef struct {
    float xx;
    float xy;
    float yy;
} AbSymmetric2;

typedef struct {
    AbVelocityPoint points[2];
    AbVec2 normal;
    float first_inverse_mass;
    float second_inverse_mass;
    float first_inverse_inertia;
    float second_inverse_inertia;
    float friction;
    float tangent_speed;
    AbSymmetric2 normal_matrix;
    AbSymmetric2 normal_inverse;
    size_t first_body;
    size_t second_body;
    size_t point_count;
} AbVelocityContact;

typedef struct {
    AbVec2 center;
    AbRigidTransform transform;
    AbVelocity velocity;
} AbSolverFrame;

typedef struct {
    AbVec2 center;
    float angle;
    AbRigidTransform transform;
    float inverse_mass;
    float inverse_inertia;
} AbPositionBody;

typedef struct {
    AbVec2 local_points[2];
    AbVec2 local_normal;
    AbVec2 local_point;
    float first_radius;
    float second_radius;
    AbContactKind kind;
    size_t point_count;
} AbPositionContact;

typedef struct {
    AbVec2 center;
    float angle;
} AbPositionState;

typedef struct {
    AbPositionContact geometry;
    size_t first_body;
    size_t second_body;
    AbVec2 first_local_center;
    AbVec2 second_local_center;
    float first_inverse_mass;
    float second_inverse_mass;
    float first_inverse_inertia;
    float second_inverse_inertia;
} AbPositionConstraint;

typedef enum {
    AB_POSITION_INVALID,
    AB_POSITION_UNRESOLVED,
    AB_POSITION_RESOLVED,
} AbPositionResult;

/* Apply saved impulses in supplied contact/point order. Finite numeric inputs
 * are required. Arrays are borrowed and must not overlap. Invalid counts or
 * indices fail before modifying velocities; zero contacts require no arrays.
 */
bool ab_contacts_warm_start(AbVelocity *velocities, size_t body_count,
                            const AbVelocityContact *contacts, size_t contact_count);

/* Solve friction, then unilateral normal impulses. Two-point contacts use the
 * target's coupled block solve. Constraints must be initialized; each active
 * contact requires one or two points. Validation precedes all mutation.
 */
bool ab_contacts_solve_velocity(AbVelocity *velocities, size_t body_count,
                                AbVelocityContact *contacts, size_t contact_count);

/* Frames supply the shape-origin transform and the current mass-center state.
 * Preserve saved impulses and contact controls; initialize active offsets,
 * effective masses, bias and coupled matrices. The manifold and contact must
 * have equal nonzero point counts, at most two. Invalid counts/kind fail without
 * mutation. Geometry, velocities, material values, and inverse masses are finite.
 */
bool ab_contact_velocity_initialize(AbVelocityContact *contact,
                                    const AbManifold *manifold, AbSolverFrame first,
                                    float first_radius, AbSolverFrame second,
                                    float second_radius, float restitution);

/* Correct one contact point using supplied current transforms. Update centers,
 * angles, and the running minimum separation; transforms remain unchanged and
 * must be recomputed before the next point. Initialize the minimum to zero for
 * a complete contact pass. Counts/kind are validated before mutation. Numeric
 * inputs are finite, pointers are valid, and minimum_separation cannot overlap
 * either body. Aliased body pointers retain the target's second-endpoint store.
 */
bool ab_contact_position_point(const AbPositionContact *contact, size_t point,
                               AbPositionBody *first, AbPositionBody *second,
                               float *minimum_separation);

/* One ordered position pass; recompute transforms before each point using the
 * first-party rotation module. Counts/kinds/indices reject before mutation.
 * Unsupported intermediate angles stop the pass; earlier contacts may already
 * be written. Inputs satisfy the finite geometry and rotation contracts, and
 * the position array does not overlap the constraints. No allocation occurs.
 */
AbPositionResult ab_contacts_solve_position(AbPositionState *positions,
                                            size_t body_count,
                                            const AbPositionConstraint *constraints,
                                            size_t constraint_count);

/* Continuous position pass: only the two selected body indices have effective
 * mass/inertia. Other bodies still define geometry. Uses the target's continuous
 * correction/convergence controls. Zero constraints resolve without arrays or
 * indices; otherwise structural errors reject before mutation. Discrete position
 * contracts apply to numeric inputs, ownership, and partial arithmetic failures.
 */
AbPositionResult ab_contacts_solve_toi_position(AbPositionState *positions,
                                                size_t body_count,
                                                const AbPositionConstraint *constraints,
                                                size_t constraint_count,
                                                size_t first_toi, size_t second_toi);

#endif
