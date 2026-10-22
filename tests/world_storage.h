#ifndef AB_TEST_WORLD_STORAGE_H
#define AB_TEST_WORLD_STORAGE_H

#include "ab/physics_world.h"

typedef struct {
    AbWorldSolveBody bodies[5];
    AbWorldFixture fixtures[6];
    AbWorldSolveContact contacts[28];
    AbFixturePair pairs[28];
    AbFixtureMass mass[6];
    AbBodyMotion island_bodies[5];
    AbIslandContact island_contacts[28];
    size_t body_order[5], contact_order[28], stack[5], heads[5], edges[56];
    AbPositionState positions[5];
    AbVelocity velocities[5];
    AbVelocityContact velocity_contacts[28];
    AbPositionConstraint position_contacts[28];
    AbWorldSolveJoint joints[6];
    AbIslandJoint island_joints[6];
    size_t joint_order[6], joint_heads[5], joint_edges[12];
    AbWorldSolveWorkspace solve;
} WorldStorage;

static AbWorldStorage prepare_storage(WorldStorage *storage, size_t bodies,
                                      size_t fixtures) {
    *storage = (WorldStorage){0};
    storage->solve = (AbWorldSolveWorkspace){
        .bodies = storage->island_bodies,
        .contacts = storage->island_contacts,
        .body_order = storage->body_order,
        .contact_order = storage->contact_order,
        .stack = storage->stack,
        .body_heads = storage->heads,
        .edge_next = storage->edges,
        .body_capacity = 5,
        .contact_capacity = 28,
        .joints = storage->island_joints,
        .joint_order = storage->joint_order,
        .joint_heads = storage->joint_heads,
        .joint_edge_next = storage->joint_edges,
        .joint_capacity = 6,
        .solver = {storage->positions, storage->velocities, storage->velocity_contacts,
                   storage->position_contacts, 5, 28},
    };
    return (AbWorldStorage){.bodies = storage->bodies,
                            .body_count = bodies,
                            .fixtures = storage->fixtures,
                            .fixture_count = fixtures,
                            .contacts = storage->contacts,
                            .pairs = storage->pairs,
                            .contact_capacity = 28,
                            .mass_scratch = storage->mass,
                            .mass_capacity = 6,
                            .solve = &storage->solve,
                            .body_capacity = 5,
                            .fixture_capacity = 6,
                            .joints = storage->joints,
                            .joint_capacity = 6};
}

static AbWorldDefinition default_definition(void) {
    return (AbWorldDefinition){.broadphase_limits = {128, 1024, 1024},
                               .maximum_translation = 2,
                               .maximum_translation_squared = 4,
                               .allow_sleep = true,
                               .warm_start = true,
                               .clear_forces = true};
}

#endif
