#include "world_internal.h"

bool ab_world_joint_graph_valid(AbWorldJointGraph graph, size_t body_count,
                                AbWorldSolveWorkspace *workspace) {
    if (graph.count > graph.slot_count) {
        return false;
    }
    if (!graph.count) {
        return true;
    }
    if (!graph.joints || graph.slot_count > SIZE_MAX / 2 ||
        workspace->joint_capacity < graph.slot_count || !workspace->joints ||
        !workspace->joint_order || !workspace->joint_heads ||
        !workspace->joint_edge_next || workspace->solver.body_capacity < body_count ||
        !workspace->solver.positions || !workspace->solver.velocities) {
        return false;
    }
    for (size_t slot = 0; slot < graph.slot_count; ++slot) {
        workspace->joint_edge_next[slot * 2] = 0;
    }
    for (size_t index = 0; index < graph.count; ++index) {
        size_t slot = graph.order ? graph.order[index] : index;
        if (slot >= graph.slot_count || workspace->joint_edge_next[slot * 2]) {
            return false;
        }
        workspace->joint_edge_next[slot * 2] = 1;
        const AbWorldSolveJoint *joint = graph.joints + slot;
        if (joint->first_body >= body_count || joint->second_body >= body_count ||
            joint->first_body == joint->second_body ||
            (unsigned)joint->constraint.kind > AB_JOINT_PRISMATIC) {
            return false;
        }
    }
    return true;
}

void ab_world_joint_graph_build(AbWorldJointGraph graph, size_t body_count,
                                AbWorldSolveWorkspace *workspace) {
    if (!graph.count) {
        return;
    }
    for (size_t body = 0; body < body_count; ++body) {
        workspace->joint_heads[body] = SIZE_MAX;
    }
    for (size_t cursor = graph.count; cursor > 0;) {
        --cursor;
        size_t index = graph.order ? graph.order[cursor] : cursor;
        AbWorldSolveJoint *joint = graph.joints + index;
        joint->island = false;
        const size_t endpoints[] = {joint->first_body, joint->second_body};
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            size_t edge = index * 2 + endpoint;
            workspace->joint_edge_next[edge] =
                workspace->joint_heads[endpoints[endpoint]];
            workspace->joint_heads[endpoints[endpoint]] = edge;
        }
    }
}

void ab_world_joint_graph_select(AbWorldJointGraph graph, AbWorldSolveBody *bodies,
                                 size_t body, size_t *stack_count, size_t *joint_count,
                                 AbWorldSolveWorkspace *workspace) {
    if (!graph.count) {
        return;
    }
    for (size_t edge = workspace->joint_heads[body]; edge != SIZE_MAX;
         edge = workspace->joint_edge_next[edge]) {
        size_t index = edge / 2;
        AbWorldSolveJoint *joint = graph.joints + index;
        size_t other = edge % 2 ? joint->first_body : joint->second_body;
        if (joint->island || !bodies[other].motion.active) {
            continue;
        }
        workspace->joint_order[(*joint_count)++] = index;
        joint->island = true;
        if (!bodies[other].island) {
            workspace->stack[(*stack_count)++] = other;
            bodies[other].island = true;
        }
    }
}
