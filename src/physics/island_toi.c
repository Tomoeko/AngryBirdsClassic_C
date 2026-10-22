#include "island_internal.h"

#include "step_internal.h"

#include <math.h>

AbPositionResult ab_island_step_toi(AbBodyMotion *bodies, size_t body_count,
                                    AbIslandContact *contacts, size_t contact_count,
                                    const AbIslandStep *step,
                                    AbIslandWorkspace *workspace, size_t first_toi,
                                    size_t second_toi) {
    if (!step || !workspace || !body_count || first_toi >= body_count ||
        second_toi >= body_count || !isfinite(step->time_ratio) ||
        !ab_step_controls_valid(bodies, body_count, &step->motion) ||
        workspace->body_capacity < body_count || !workspace->positions ||
        !workspace->velocities ||
        (contact_count &&
         !ab_island_contacts_valid(contacts, contact_count, body_count, workspace))) {
        return AB_POSITION_INVALID;
    }
    for (size_t body = 0; body < body_count; ++body) {
        const AbBodyMassState *mass = &bodies[body].mass;
        workspace->positions[body] = (AbPositionState){mass->center, mass->angle};
        workspace->velocities[body] =
            (AbVelocity){mass->linear_velocity, mass->angular_velocity};
    }
    for (size_t contact = 0; contact < contact_count; ++contact) {
        workspace->position_contacts[contact] =
            ab_island_position_constraint(bodies, contacts + contact);
    }
    AbPositionResult position = AB_POSITION_UNRESOLVED;
    for (size_t iteration = 0; iteration < step->motion.position_iterations;
         ++iteration) {
        position = ab_contacts_solve_toi_position(workspace->positions, body_count,
                                                  workspace->position_contacts,
                                                  contact_count, first_toi, second_toi);
        if (position == AB_POSITION_INVALID) {
            return position;
        }
        if (position == AB_POSITION_RESOLVED) {
            break;
        }
    }
    for (size_t body = 0; body < body_count; ++body) {
        bodies[body].mass.center = workspace->positions[body].center;
        bodies[body].mass.angle = workspace->positions[body].angle;
    }
    bodies[first_toi].mass.previous_center = workspace->positions[first_toi].center;
    bodies[first_toi].mass.previous_angle = workspace->positions[first_toi].angle;
    bodies[second_toi].mass.previous_center = workspace->positions[second_toi].center;
    bodies[second_toi].mass.previous_angle = workspace->positions[second_toi].angle;
    for (size_t contact = 0; contact < contact_count; ++contact) {
        if (!ab_island_initialize_contact(bodies, contacts + contact, step, workspace,
                                          contact)) {
            return AB_POSITION_INVALID;
        }
    }
    for (size_t iteration = 0; iteration < step->velocity_iterations; ++iteration) {
        if (!ab_contacts_solve_velocity(workspace->velocities, body_count,
                                        workspace->velocity_contacts, contact_count)) {
            return AB_POSITION_INVALID;
        }
    }
    for (size_t body = 0; body < body_count; ++body) {
        AbBodyMassState *mass = &bodies[body].mass;
        mass->linear_velocity = workspace->velocities[body].linear;
        mass->angular_velocity = workspace->velocities[body].angular;
        ab_step_move_body(bodies + body, &step->motion);
        workspace->positions[body] = (AbPositionState){mass->center, mass->angle};
        workspace->velocities[body] =
            (AbVelocity){mass->linear_velocity, mass->angular_velocity};
        if (!ab_step_sync_body(bodies + body)) {
            return AB_POSITION_INVALID;
        }
    }
    ab_island_report_contacts(step, workspace->velocity_contacts, contact_count);
    return position;
}
