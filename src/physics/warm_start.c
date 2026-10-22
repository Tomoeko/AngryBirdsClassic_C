#include "ab/physics_solver.h"

static void apply_contact(AbVelocity *velocities, const AbVelocityContact *contact) {
    AbVelocity first = velocities[contact->first_body];
    AbVelocity second = velocities[contact->second_body];
    for (size_t index = 0; index < contact->point_count; ++index) {
        const AbVelocityPoint *point = &contact->points[index];
        float impulse_x = point->tangent_impulse * contact->normal.y;
        impulse_x = impulse_x + point->normal_impulse * contact->normal.x;
        float impulse_y = point->tangent_impulse * (-contact->normal.x);
        impulse_y = impulse_y + point->normal_impulse * contact->normal.y;
        float first_torque =
            impulse_y * point->first_offset.x - impulse_x * point->first_offset.y;
        float second_torque =
            impulse_y * point->second_offset.x - impulse_x * point->second_offset.y;
        first.linear.x = first.linear.x - contact->first_inverse_mass * impulse_x;
        first.linear.y = first.linear.y - contact->first_inverse_mass * impulse_y;
        second.linear.x = second.linear.x + contact->second_inverse_mass * impulse_x;
        second.linear.y = second.linear.y + contact->second_inverse_mass * impulse_y;
        first.angular = first.angular - contact->first_inverse_inertia * first_torque;
        second.angular =
            second.angular + contact->second_inverse_inertia * second_torque;
    }
    velocities[contact->first_body] = first;
    velocities[contact->second_body] = second;
}

bool ab_contacts_warm_start(AbVelocity *velocities, size_t body_count,
                            const AbVelocityContact *contacts, size_t contact_count) {
    if (contact_count && (!contacts || !velocities)) {
        return false;
    }
    for (size_t index = 0; index < contact_count; ++index) {
        if (contacts[index].point_count > 2 ||
            contacts[index].first_body >= body_count ||
            contacts[index].second_body >= body_count) {
            return false;
        }
    }
    for (size_t index = 0; index < contact_count; ++index) {
        apply_contact(velocities, &contacts[index]);
    }
    return true;
}
