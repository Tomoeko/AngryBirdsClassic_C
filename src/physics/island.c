#include "ab/physics_island.h"

#include "ab/rotation.h"
#include "internal.h"
#include "island_internal.h"
#include "step_internal.h"

#include <math.h>

bool ab_island_contacts_valid(const AbIslandContact *contacts, size_t contact_count,
                              size_t body_count, const AbIslandWorkspace *workspace) {
    if (!contacts || !workspace || workspace->body_capacity < body_count ||
        workspace->contact_capacity < contact_count || !workspace->positions ||
        !workspace->velocities || !workspace->velocity_contacts ||
        !workspace->position_contacts) {
        return false;
    }
    for (size_t index = 0; index < contact_count; ++index) {
        const AbIslandContact *contact = contacts + index;
        if (contact->first_body >= body_count || contact->second_body >= body_count ||
            contact->manifold.point_count < 1 || contact->manifold.point_count > 2 ||
            (unsigned)contact->manifold.kind > AB_CONTACT_FACE_SECOND ||
            (contact->manifold.kind == AB_CONTACT_CIRCLES &&
             contact->manifold.point_count != 1)) {
            return false;
        }
    }
    return true;
}

static bool make_frame(const AbBodyMassState *body, AbVelocity velocity,
                       AbSolverFrame *frame) {
    AbRotation rotation;
    if (!ab_rotation_from_angle(body->angle, &rotation)) {
        return false;
    }
    AbRigidTransform transform = {.sine = rotation.sine, .cosine = rotation.cosine};
    AbVec2 offset = ab_physics_rotate(body->local_center, transform);
    transform.position = (AbVec2){body->center.x - offset.x, body->center.y - offset.y};
    *frame = (AbSolverFrame){body->center, transform, velocity};
    return true;
}

bool ab_island_initialize_contact(const AbBodyMotion *bodies,
                                  const AbIslandContact *input,
                                  const AbIslandStep *step,
                                  AbIslandWorkspace *workspace, size_t index) {
    const AbBodyMassState *first = &bodies[input->first_body].mass;
    const AbBodyMassState *second = &bodies[input->second_body].mass;
    AbVelocityContact *velocity = workspace->velocity_contacts + index;
    *velocity = (AbVelocityContact){
        .first_inverse_mass = first->inverse_mass,
        .second_inverse_mass = second->inverse_mass,
        .first_inverse_inertia = first->inverse_inertia,
        .second_inverse_inertia = second->inverse_inertia,
        .friction = input->friction,
        .tangent_speed = input->tangent_speed,
        .first_body = input->first_body,
        .second_body = input->second_body,
        .point_count = input->manifold.point_count,
    };
    for (size_t point = 0; point < velocity->point_count; ++point) {
        if (step->warm_start) {
            velocity->points[point].normal_impulse =
                step->time_ratio * input->manifold.points[point].normal_impulse;
            velocity->points[point].tangent_impulse =
                step->time_ratio * input->manifold.points[point].tangent_impulse;
        }
    }
    workspace->position_contacts[index] = ab_island_position_constraint(bodies, input);
    AbSolverFrame first_frame, second_frame;
    return make_frame(first, workspace->velocities[input->first_body], &first_frame) &&
           make_frame(second, workspace->velocities[input->second_body],
                      &second_frame) &&
           ab_contact_velocity_initialize(velocity, &input->manifold, first_frame,
                                          input->first_radius, second_frame,
                                          input->second_radius, input->restitution);
}

AbPositionConstraint ab_island_position_constraint(const AbBodyMotion *bodies,
                                                   const AbIslandContact *input) {
    const AbBodyMassState *first = &bodies[input->first_body].mass;
    const AbBodyMassState *second = &bodies[input->second_body].mass;
    return (AbPositionConstraint){
        .geometry =
            {
                .local_points = {input->manifold.points[0].local_point,
                                 input->manifold.points[1].local_point},
                .local_normal = input->manifold.local_normal,
                .local_point = input->manifold.local_point,
                .first_radius = input->first_radius,
                .second_radius = input->second_radius,
                .kind = input->manifold.kind,
                .point_count = input->manifold.point_count,
            },
        .first_body = input->first_body,
        .second_body = input->second_body,
        .first_local_center = first->local_center,
        .second_local_center = second->local_center,
        .first_inverse_mass = first->inverse_mass,
        .second_inverse_mass = second->inverse_mass,
        .first_inverse_inertia = first->inverse_inertia,
        .second_inverse_inertia = second->inverse_inertia,
    };
}

static void store_impulses(AbIslandContact *contacts, size_t count,
                           const AbVelocityContact *velocity) {
    for (size_t index = 0; index < count; ++index) {
        for (size_t point = 0; point < velocity[index].point_count; ++point) {
            contacts[index].manifold.points[point].normal_impulse =
                velocity[index].points[point].normal_impulse;
            contacts[index].manifold.points[point].tangent_impulse =
                velocity[index].points[point].tangent_impulse;
        }
    }
}

void ab_island_report_contacts(const AbIslandStep *step,
                               const AbVelocityContact *contacts, size_t count) {
    if (!step->report) {
        return;
    }
    for (size_t contact = 0; contact < count; ++contact) {
        AbContactImpulse impulse = {.count = contacts[contact].point_count};
        for (size_t point = 0; point < impulse.count; ++point) {
            impulse.normal[point] = contacts[contact].points[point].normal_impulse;
            impulse.tangent[point] = contacts[contact].points[point].tangent_impulse;
        }
        step->report(step->report_context, contact, &impulse);
    }
}

AbPositionResult ab_island_step_joints(AbBodyMotion *bodies, size_t body_count,
                                       AbIslandContact *contacts, size_t contact_count,
                                       const AbIslandJoint *joints, size_t joint_count,
                                       const AbIslandStep *step,
                                       AbIslandWorkspace *workspace) {
    if (!step || !isfinite(step->time_ratio) ||
        !ab_step_controls_valid(bodies, body_count, &step->motion) ||
        !ab_island_joints_valid(joints, joint_count, body_count, workspace)) {
        return AB_POSITION_INVALID;
    }
    if (!contact_count && !joint_count) {
        if (!ab_island_step_free(bodies, body_count, &step->motion)) {
            return AB_POSITION_INVALID;
        }
        return step->motion.position_iterations ? AB_POSITION_RESOLVED
                                                : AB_POSITION_UNRESOLVED;
    }
    if (contact_count &&
        !ab_island_contacts_valid(contacts, contact_count, body_count, workspace)) {
        return AB_POSITION_INVALID;
    }
    for (size_t index = 0; index < body_count; ++index) {
        ab_step_apply_forces(bodies + index, &step->motion);
        workspace->velocities[index] = (AbVelocity){
            bodies[index].mass.linear_velocity, bodies[index].mass.angular_velocity};
    }
    for (size_t index = 0; index < contact_count; ++index) {
        if (!ab_island_initialize_contact(bodies, contacts + index, step, workspace,
                                          index)) {
            return AB_POSITION_INVALID;
        }
    }
    if (step->warm_start &&
        !ab_contacts_warm_start(workspace->velocities, body_count,
                                workspace->velocity_contacts, contact_count)) {
        return AB_POSITION_INVALID;
    }
    if (!ab_island_initialize_joints(bodies, joints, joint_count, step, workspace)) {
        return AB_POSITION_INVALID;
    }
    for (size_t iteration = 0; iteration < step->velocity_iterations; ++iteration) {
        if (!ab_island_solve_joint_velocities(joints, joint_count, step->motion.seconds,
                                              workspace) ||
            !ab_contacts_solve_velocity(workspace->velocities, body_count,
                                        workspace->velocity_contacts, contact_count)) {
            return AB_POSITION_INVALID;
        }
    }
    store_impulses(contacts, contact_count, workspace->velocity_contacts);
    for (size_t index = 0; index < body_count; ++index) {
        AbBodyMassState *body = &bodies[index].mass;
        body->linear_velocity = workspace->velocities[index].linear;
        body->angular_velocity = workspace->velocities[index].angular;
        ab_step_move_body(bodies + index, &step->motion);
        workspace->positions[index] = (AbPositionState){body->center, body->angle};
    }
    AbPositionResult position = AB_POSITION_UNRESOLVED;
    for (size_t iteration = 0; iteration < step->motion.position_iterations;
         ++iteration) {
        position =
            ab_contacts_solve_position(workspace->positions, body_count,
                                       workspace->position_contacts, contact_count);
        if (position == AB_POSITION_INVALID) {
            return position;
        }
        AbPositionResult joint_position =
            ab_island_solve_joint_positions(joints, joint_count, workspace);
        if (joint_position == AB_POSITION_INVALID) {
            return joint_position;
        }
        if (joint_position == AB_POSITION_UNRESOLVED) {
            position = AB_POSITION_UNRESOLVED;
        }
        if (position == AB_POSITION_RESOLVED) {
            break;
        }
    }
    for (size_t index = 0; index < body_count; ++index) {
        bodies[index].mass.center = workspace->positions[index].center;
        bodies[index].mass.angle = workspace->positions[index].angle;
        if (!ab_step_sync_body(bodies + index)) {
            return AB_POSITION_INVALID;
        }
    }
    ab_island_report_contacts(step, workspace->velocity_contacts, contact_count);
    if (step->motion.allow_sleep) {
        ab_step_sleep(bodies, body_count, &step->motion,
                      position == AB_POSITION_RESOLVED);
    }
    return position;
}

AbPositionResult ab_island_step(AbBodyMotion *bodies, size_t body_count,
                                AbIslandContact *contacts, size_t contact_count,
                                const AbIslandStep *step,
                                AbIslandWorkspace *workspace) {
    return ab_island_step_joints(bodies, body_count, contacts, contact_count, NULL, 0,
                                 step, workspace);
}
