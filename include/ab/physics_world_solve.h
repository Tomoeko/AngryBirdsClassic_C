#ifndef AB_PHYSICS_WORLD_SOLVE_H
#define AB_PHYSICS_WORLD_SOLVE_H

#include "ab/physics_contact_state.h"
#include "ab/physics_island.h"

typedef struct {
    AbBodyMotion motion;
    size_t island_index;
    bool island;
} AbWorldSolveBody;

typedef struct {
    AbContactState state;
    size_t first_body;
    size_t second_body;
    float first_radius;
    float second_radius;
    bool sensor;
    float impact_fraction;
    uint32_t impact_count;
} AbWorldSolveContact;

typedef struct {
    size_t first_body;
    size_t second_body;
    AbJointConstraint constraint;
    bool island;
    bool collide_connected;
    uintptr_t user_data;
} AbWorldSolveJoint;

typedef struct {
    AbBodyMotion *bodies;
    AbIslandContact *contacts;
    size_t *body_order;
    size_t *contact_order;
    size_t *stack;
    size_t *body_heads;
    size_t *edge_next;
    size_t body_capacity;
    size_t contact_capacity;
    AbIslandWorkspace solver;
    size_t island_count;
    AbIslandJoint *joints;
    size_t *joint_order;
    size_t *joint_heads;
    size_t *joint_edge_next;
    size_t joint_capacity;
} AbWorldSolveWorkspace;

/* Discrete world-island selection and solving with an existing contact graph.
 * Bodies and contacts are supplied in world-list order (newest first). A body's
 * contact-list order is the corresponding subsequence of the world contact list.
 * Contact discovery/update, fixture synchronization,
 * force clearing and continuous collision belong to the enclosing world step.
 * Static bodies may participate in several islands. Other visited bodies retain
 * their island flag/index. Optional reports use world contact indices and run
 * with world body/manifold state synchronized, before each island sleeps.
 * Sensors and disabled/non-touching contacts are skipped.
 * Workspace arrays are borrowed and disjoint from inputs and one another.
 * edge_next holds twice contact_capacity entries; other arrays use their named
 * capacity. All storage is supplied by the caller; no allocation occurs.
 * Structural/control errors reject before mutation. Invalid numeric values or
 * intermediate arithmetic can fail after earlier islands have changed state.
 */
bool ab_world_solve_discrete(AbWorldSolveBody *bodies, size_t body_count,
                             AbWorldSolveContact *contacts, size_t contact_count,
                             const AbIslandStep *step,
                             AbWorldSolveWorkspace *workspace);

/* The same discrete solve with joints supplied in newest-first world-list order.
 * Contact edges are visited before joint edges. Inactive bodies are not reached
 * through joints, and static bodies do not extend the graph. Joint constraints
 * retain their solver state between calls. collide_connected and user_data are
 * metadata for the enclosing world; this operation uses the supplied contacts.
 * Contact edges must respect world activation: traversable contacts connect
 * active bodies. Deactivation removes these contacts in the enclosing world.
 * Joint scratch arrays use joint_capacity, except joint_heads (body_capacity)
 * and joint_edge_next (twice joint_capacity). Joint-only islands still require
 * solver positions and velocities for body_capacity bodies.
 */
bool ab_world_solve_discrete_joints(AbWorldSolveBody *bodies, size_t body_count,
                                    AbWorldSolveContact *contacts, size_t contact_count,
                                    AbWorldSolveJoint *joints, size_t joint_count,
                                    const AbIslandStep *step,
                                    AbWorldSolveWorkspace *workspace);

#endif
