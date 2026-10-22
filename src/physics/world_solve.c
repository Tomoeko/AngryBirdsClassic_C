#include "ab/physics_world_solve.h"

#include "step_internal.h"
#include "world_internal.h"

#include <math.h>
#include <stdint.h>

static bool valid_graph(const AbWorldSolveBody *bodies, size_t body_count,
                        const AbWorldSolveContact *contacts, size_t contact_count,
                        const AbIslandStep *step,
                        const AbWorldSolveWorkspace *workspace) {
    if (!step || !workspace || (body_count && !bodies) ||
        (contact_count && !contacts) || contact_count > SIZE_MAX / 2 ||
        workspace->body_capacity < body_count ||
        workspace->contact_capacity < contact_count || !isfinite(step->time_ratio) ||
        !ab_step_controls_valid(NULL, 0, &step->motion)) {
        return false;
    }
    if (body_count && (!workspace->bodies || !workspace->body_order ||
                       !workspace->stack || !workspace->body_heads)) {
        return false;
    }
    if (contact_count &&
        (!workspace->contacts || !workspace->contact_order || !workspace->edge_next ||
         workspace->solver.body_capacity < body_count ||
         workspace->solver.contact_capacity < contact_count ||
         !workspace->solver.positions || !workspace->solver.velocities ||
         !workspace->solver.velocity_contacts ||
         !workspace->solver.position_contacts)) {
        return false;
    }
    for (size_t body = 0; body < body_count; ++body) {
        if ((unsigned)bodies[body].motion.mass.type > AB_BODY_DYNAMIC) {
            return false;
        }
    }
    for (size_t index = 0; index < contact_count; ++index) {
        const AbWorldSolveContact *contact = contacts + index;
        const AbManifold *manifold = &contact->state.manifold;
        if (contact->first_body >= body_count || contact->second_body >= body_count ||
            contact->first_body == contact->second_body) {
            return false;
        }
        if (!contact->sensor && (contact->state.flags & 6) == 6 &&
            (manifold->point_count < 1 || manifold->point_count > 2 ||
             (unsigned)manifold->kind > AB_CONTACT_FACE_SECOND ||
             (manifold->kind == AB_CONTACT_CIRCLES && manifold->point_count != 1))) {
            return false;
        }
    }
    return true;
}

static void build_graph(AbWorldSolveBody *bodies, size_t body_count,
                        AbWorldSolveContact *contacts, size_t contact_count,
                        AbWorldSolveWorkspace *workspace) {
    for (size_t body = 0; body < body_count; ++body) {
        bodies[body].island = false;
        workspace->body_heads[body] = SIZE_MAX;
    }
    for (size_t index = contact_count; index > 0;) {
        --index;
        AbWorldSolveContact *contact = contacts + index;
        contact->state.flags &= ~UINT32_C(1);
        const size_t endpoints[] = {contact->first_body, contact->second_body};
        for (size_t endpoint = 0; endpoint < 2; ++endpoint) {
            size_t edge = index * 2 + endpoint;
            workspace->edge_next[edge] = workspace->body_heads[endpoints[endpoint]];
            workspace->body_heads[endpoints[endpoint]] = edge;
        }
    }
}

static void select_island(AbWorldSolveBody *bodies, AbWorldSolveContact *contacts,
                          AbWorldJointGraph graph, size_t seed,
                          AbWorldSolveWorkspace *workspace,
                          AbWorldIslandSelection *selection) {
    size_t stack_count = 1;
    workspace->stack[0] = seed;
    bodies[seed].island = true;
    *selection = (AbWorldIslandSelection){0};
    while (stack_count) {
        size_t index = workspace->stack[--stack_count];
        AbWorldSolveBody *body = bodies + index;
        body->island_index = selection->body_count;
        workspace->body_order[selection->body_count++] = index;
        if (!body->motion.awake) {
            body->motion.awake = true;
            body->motion.sleep_time = 0;
        }
        if (body->motion.mass.type == AB_BODY_STATIC) {
            continue;
        }
        for (size_t edge = workspace->body_heads[index]; edge != SIZE_MAX;
             edge = workspace->edge_next[edge]) {
            size_t contact_index = edge / 2;
            AbWorldSolveContact *contact = contacts + contact_index;
            if ((contact->state.flags & 1) || (contact->state.flags & 6) != 6 ||
                contact->sensor) {
                continue;
            }
            workspace->contact_order[selection->contact_count++] = contact_index;
            contact->state.flags |= 1;
            size_t other = edge % 2 ? contact->first_body : contact->second_body;
            if (!bodies[other].island) {
                workspace->stack[stack_count++] = other;
                bodies[other].island = true;
            }
        }
        ab_world_joint_graph_select(graph, bodies, index, &stack_count,
                                    &selection->joint_count, workspace);
    }
}

typedef struct {
    AbWorldSolveBody *bodies;
    AbWorldSolveContact *contacts;
    size_t body_count;
    size_t contact_count;
    const AbIslandStep *step;
    AbWorldSolveWorkspace *workspace;
} WorldReportContext;

static void report_world_island(void *context, size_t contact,
                                const AbContactImpulse *impulse) {
    WorldReportContext *report = context;
    AbWorldSolveWorkspace *workspace = report->workspace;
    for (size_t body = 0; body < report->body_count; ++body) {
        report->bodies[workspace->body_order[body]].motion = workspace->bodies[body];
    }
    for (size_t index = 0; index < report->contact_count; ++index) {
        report->contacts[workspace->contact_order[index]].state.manifold =
            workspace->contacts[index].manifold;
    }
    report->step->report(report->step->report_context,
                         workspace->contact_order[contact], impulse);
    for (size_t body = 0; body < report->body_count; ++body) {
        workspace->bodies[body] = report->bodies[workspace->body_order[body]].motion;
    }
    for (size_t index = 0; index < report->contact_count; ++index) {
        workspace->contacts[index].manifold =
            report->contacts[workspace->contact_order[index]].state.manifold;
    }
}

bool ab_world_solve_selected(AbWorldSolveBody *bodies, AbWorldSolveContact *contacts,
                             AbWorldSolveJoint *joints,
                             AbWorldIslandSelection selection, const AbIslandStep *step,
                             AbWorldSolveWorkspace *workspace) {
    size_t body_count = selection.body_count;
    size_t contact_count = selection.contact_count;
    for (size_t body = 0; body < body_count; ++body) {
        workspace->bodies[body] = bodies[workspace->body_order[body]].motion;
    }
    for (size_t index = 0; index < contact_count; ++index) {
        const AbWorldSolveContact *contact = contacts + workspace->contact_order[index];
        workspace->contacts[index] = (AbIslandContact){
            .manifold = contact->state.manifold,
            .first_body = bodies[contact->first_body].island_index,
            .second_body = bodies[contact->second_body].island_index,
            .first_radius = contact->first_radius,
            .second_radius = contact->second_radius,
            .friction = contact->state.friction,
            .restitution = contact->state.restitution,
            .tangent_speed = contact->state.tangent_speed,
        };
    }
    for (size_t index = 0; index < selection.joint_count; ++index) {
        AbWorldSolveJoint *joint = joints + workspace->joint_order[index];
        workspace->joints[index] = (AbIslandJoint){
            bodies[joint->first_body].island_index,
            bodies[joint->second_body].island_index, &joint->constraint};
    }
    WorldReportContext report = {bodies,        contacts, body_count,
                                 contact_count, step,     workspace};
    AbIslandStep island_step = *step;
    if (step->report) {
        island_step.report = report_world_island;
        island_step.report_context = &report;
    }
    AbPositionResult result =
        selection.continuous
            ? ab_island_step_toi(workspace->bodies, body_count, workspace->contacts,
                                 contact_count, &island_step, &workspace->solver,
                                 selection.first_impact_body,
                                 selection.second_impact_body)
            : ab_island_step_joints(workspace->bodies, body_count, workspace->contacts,
                                    contact_count, workspace->joints,
                                    selection.joint_count, &island_step,
                                    &workspace->solver);
    for (size_t body = 0; body < body_count; ++body) {
        AbWorldSolveBody *output = bodies + workspace->body_order[body];
        output->motion = workspace->bodies[body];
        if (output->motion.mass.type == AB_BODY_STATIC) {
            output->island = false;
        }
    }
    for (size_t index = 0; index < contact_count; ++index) {
        contacts[workspace->contact_order[index]].state.manifold =
            workspace->contacts[index].manifold;
    }
    return result != AB_POSITION_INVALID;
}

bool ab_world_solve_ordered_joints(AbWorldSolveBody *bodies, size_t slot_count,
                                   const size_t *body_order, size_t body_count,
                                   AbWorldSolveContact *contacts, size_t contact_count,
                                   AbWorldJointGraph graph, const AbIslandStep *step,
                                   AbWorldSolveWorkspace *workspace) {
    if (body_count > slot_count ||
        !valid_graph(bodies, slot_count, contacts, contact_count, step, workspace) ||
        !ab_world_joint_graph_valid(graph, slot_count, workspace)) {
        return false;
    }
    for (size_t cursor = 0; cursor < body_count; ++cursor) {
        if (body_order && body_order[cursor] >= slot_count) {
            return false;
        }
    }
    build_graph(bodies, slot_count, contacts, contact_count, workspace);
    ab_world_joint_graph_build(graph, slot_count, workspace);
    workspace->island_count = 0;
    for (size_t cursor = 0; cursor < body_count; ++cursor) {
        size_t seed = body_order ? body_order[cursor] : cursor;
        const AbWorldSolveBody *body = bodies + seed;
        if (body->island || !body->motion.awake || !body->motion.active ||
            body->motion.mass.type == AB_BODY_STATIC) {
            continue;
        }
        AbWorldIslandSelection selection;
        select_island(bodies, contacts, graph, seed, workspace, &selection);
        if (!ab_world_solve_selected(bodies, contacts, graph.joints, selection, step,
                                     workspace)) {
            return false;
        }
        ++workspace->island_count;
    }
    return true;
}

bool ab_world_solve_ordered(AbWorldSolveBody *bodies, size_t slot_count,
                            const size_t *body_order, size_t body_count,
                            AbWorldSolveContact *contacts, size_t contact_count,
                            const AbIslandStep *step,
                            AbWorldSolveWorkspace *workspace) {
    return ab_world_solve_ordered_joints(bodies, slot_count, body_order, body_count,
                                         contacts, contact_count,
                                         (AbWorldJointGraph){0}, step, workspace);
}

bool ab_world_solve_discrete(AbWorldSolveBody *bodies, size_t body_count,
                             AbWorldSolveContact *contacts, size_t contact_count,
                             const AbIslandStep *step,
                             AbWorldSolveWorkspace *workspace) {
    return ab_world_solve_ordered(bodies, body_count, NULL, body_count, contacts,
                                  contact_count, step, workspace);
}

bool ab_world_solve_discrete_joints(AbWorldSolveBody *bodies, size_t body_count,
                                    AbWorldSolveContact *contacts, size_t contact_count,
                                    AbWorldSolveJoint *joints, size_t joint_count,
                                    const AbIslandStep *step,
                                    AbWorldSolveWorkspace *workspace) {
    AbWorldJointGraph graph = {joints, joint_count, NULL, joint_count};
    return ab_world_solve_ordered_joints(bodies, body_count, NULL, body_count, contacts,
                                         contact_count, graph, step, workspace);
}
