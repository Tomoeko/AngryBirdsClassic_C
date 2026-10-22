#ifndef AB_PHYSICS_ISLAND_H
#define AB_PHYSICS_ISLAND_H

#include "ab/physics_step.h"
#include "ab/physics_solver.h"
#include "ab/physics_joint_constraint.h"

typedef struct {
    AbManifold manifold;
    size_t first_body;
    size_t second_body;
    float first_radius;
    float second_radius;
    float friction;
    float restitution;
    float tangent_speed;
} AbIslandContact;

typedef struct {
    size_t first_body;
    size_t second_body;
    AbJointConstraint *constraint;
} AbIslandJoint;

typedef struct {
    float normal[2];
    float tangent[2];
    size_t count;
} AbContactImpulse;

typedef void (*AbIslandReport)(void *context, size_t contact,
                               const AbContactImpulse *impulse);

typedef struct {
    AbFreeIslandStep motion;
    size_t velocity_iterations;
    float time_ratio;
    bool warm_start;
    AbIslandReport report;
    void *report_context;
} AbIslandStep;

typedef struct {
    AbPositionState *positions;
    AbVelocity *velocities;
    AbVelocityContact *velocity_contacts;
    AbPositionConstraint *position_contacts;
    size_t body_capacity;
    size_t contact_capacity;
} AbIslandWorkspace;

/* Discrete island with supplied contact manifolds; no joints.
 * Workspace and input arrays are borrowed, valid for their stated counts, and
 * cannot overlap. No allocation occurs. Finite numeric and rotation contracts
 * from physics_step.h apply. Invalid counts/indices/controls reject before any
 * mutation. Invalid intermediate math can stop after partial state changes.
 * Active saved impulses are updated; reduced two-point velocity contacts retain
 * the original second manifold impulse. Return position convergence separately
 * from invalid input. Optional reports run in contact order after body state is
 * synchronized and before sleeping. Only count impulse entries are active;
 * reduced velocity contacts report their reduced count. The impulse is borrowed
 * during the callback. Callbacks keep arrays/workspace structurally valid.
 * World selection and continuous collision are separate.
 */
AbPositionResult ab_island_step(AbBodyMotion *bodies, size_t body_count,
                                AbIslandContact *contacts, size_t contact_count,
                                const AbIslandStep *step, AbIslandWorkspace *workspace);

/* Extend discrete stepping with supplied joints in native island-list order.
 * Entries borrow distinct mutable constraints and refer to distinct endpoints.
 * Keep definitions and all array structure stable for the call. Joint-only
 * islands require position/velocity workspace but no contact workspace.
 * Contacts initialize/warm-start before joints initialize. Velocity passes
 * solve joints then contacts; position passes solve contacts then all joints,
 * and require both to converge before stopping/sleeping. Other discrete
 * ownership, numeric, callback and partial-failure contracts above apply.
 * World graph selection, joint creation/deletion and game policy are separate.
 */
AbPositionResult ab_island_step_joints(AbBodyMotion *bodies, size_t body_count,
                                       AbIslandContact *contacts, size_t contact_count,
                                       const AbIslandJoint *joints, size_t joint_count,
                                       const AbIslandStep *step,
                                       AbIslandWorkspace *workspace);

/* Continuous island: correct positions, update the two selected previous sweep
 * endpoints, initialize/solve velocities, integrate/clamp motion, synchronize and
 * report. Force/sleep state and saved manifold impulses are preserved. Supplied
 * warm_start seeds solver impulses; continuous world substeps use false.
 * Selected indices must fit a nonempty body array. Other discrete numeric,
 * workspace, callback, and partial-failure contracts apply.
 */
AbPositionResult ab_island_step_toi(AbBodyMotion *bodies, size_t body_count,
                                    AbIslandContact *contacts, size_t contact_count,
                                    const AbIslandStep *step,
                                    AbIslandWorkspace *workspace, size_t first_toi,
                                    size_t second_toi);

#endif
