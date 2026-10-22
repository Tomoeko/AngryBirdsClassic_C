#ifndef AB_PHYSICS_WORLD_INTERNAL_H
#define AB_PHYSICS_WORLD_INTERNAL_H

#include "ab/physics_world.h"
#include "ab/physics_distance.h"

typedef struct {
    size_t body_count;
    size_t contact_count;
    bool continuous;
    size_t first_impact_body;
    size_t second_impact_body;
    size_t joint_count;
} AbWorldIslandSelection;

typedef struct {
    AbWorldSolveJoint *joints;
    size_t slot_count;
    const size_t *order;
    size_t count;
} AbWorldJointGraph;

bool ab_world_find_contacts(AbPhysicsWorld *world, AbError *error);
bool ab_world_refresh_contacts(AbPhysicsWorld *world, AbError *error);
bool ab_world_update_contact(AbPhysicsWorld *world, size_t index, AbError *error);
bool ab_world_reset_body_mass(AbPhysicsWorld *world, size_t body, size_t fixture_limit);
bool ab_world_fixture_valid(const AbPhysicsWorld *world, const AbWorldFixture *fixture);
bool ab_world_mutable(const AbPhysicsWorld *world);
size_t ab_world_unused_slot(const bool *alive, size_t count, size_t capacity);
void ab_world_unlink_slot(size_t *order, size_t *count, size_t slot);
bool ab_world_joint_definition_valid(const AbWorldSolveJoint *joint);
void ab_world_joint_reset(AbWorldSolveJoint *joint);
bool ab_world_remove_body_joints(AbPhysicsWorld *world, size_t body, AbError *error);
bool ab_world_attach_fixture(AbPhysicsWorld *world, size_t fixture, AbError *error);
void ab_world_remove_body_contacts(AbPhysicsWorld *world, size_t body);
bool ab_world_solve_ordered(AbWorldSolveBody *bodies, size_t slot_count,
                            const size_t *body_order, size_t body_count,
                            AbWorldSolveContact *contacts, size_t contact_count,
                            const AbIslandStep *step, AbWorldSolveWorkspace *workspace);
bool ab_world_solve_ordered_joints(AbWorldSolveBody *bodies, size_t slot_count,
                                   const size_t *body_order, size_t body_count,
                                   AbWorldSolveContact *contacts, size_t contact_count,
                                   AbWorldJointGraph graph, const AbIslandStep *step,
                                   AbWorldSolveWorkspace *workspace);
bool ab_world_joint_graph_valid(AbWorldJointGraph graph, size_t body_count,
                                AbWorldSolveWorkspace *workspace);
void ab_world_joint_graph_build(AbWorldJointGraph graph, size_t body_count,
                                AbWorldSolveWorkspace *workspace);
void ab_world_joint_graph_select(AbWorldJointGraph graph, AbWorldSolveBody *bodies,
                                 size_t body, size_t *stack_count, size_t *joint_count,
                                 AbWorldSolveWorkspace *workspace);
AbDistanceProxy ab_world_shape_proxy(const AbPhysicsShape *shape);
bool ab_world_synchronize_body(AbPhysicsWorld *world, size_t index, AbError *error);
bool ab_world_solve_continuous(AbPhysicsWorld *world, const AbIslandStep *step,
                               AbError *error);
bool ab_world_solve_selected(AbWorldSolveBody *bodies, AbWorldSolveContact *contacts,
                             AbWorldSolveJoint *joints,
                             AbWorldIslandSelection selection, const AbIslandStep *step,
                             AbWorldSolveWorkspace *workspace);
bool ab_world_pair_allowed(const AbPhysicsWorld *world, size_t first, size_t second);
void ab_world_remove_contact(AbPhysicsWorld *world, size_t index);
void ab_world_wake_body(AbBodyMotion *body);

#endif
