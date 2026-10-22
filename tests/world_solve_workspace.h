#ifndef AB_TEST_WORLD_SOLVE_WORKSPACE_H
#define AB_TEST_WORLD_SOLVE_WORKSPACE_H

#include "ab/physics_world_solve.h"

typedef struct {
    AbBodyMotion motion[7];
    AbIslandContact contacts[8];
    size_t body_order[7];
    size_t contact_order[8];
    size_t stack[7];
    size_t body_heads[7];
    size_t edge_next[16];
    AbIslandJoint joints[6];
    size_t joint_order[6];
    size_t joint_heads[7];
    size_t joint_edge_next[12];
    AbPositionState positions[7];
    AbVelocity velocities[7];
    AbVelocityContact velocity_contacts[8];
    AbPositionConstraint position_contacts[8];
} WorldTestStorage;

static AbWorldSolveWorkspace world_workspace(WorldTestStorage *storage) {
    return (AbWorldSolveWorkspace){
        .bodies = storage->motion,
        .contacts = storage->contacts,
        .body_order = storage->body_order,
        .contact_order = storage->contact_order,
        .stack = storage->stack,
        .body_heads = storage->body_heads,
        .edge_next = storage->edge_next,
        .body_capacity = 7,
        .contact_capacity = 8,
        .joints = storage->joints,
        .joint_order = storage->joint_order,
        .joint_heads = storage->joint_heads,
        .joint_edge_next = storage->joint_edge_next,
        .joint_capacity = 6,
        .solver = {storage->positions, storage->velocities, storage->velocity_contacts,
                   storage->position_contacts, 7, 8},
    };
}

#endif
