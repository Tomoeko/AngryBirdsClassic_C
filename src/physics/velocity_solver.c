#include "ab/physics_solver.h"

static AbVec2 point_velocity_difference(AbVelocity first, AbVelocity second,
                                        const AbVelocityPoint *point) {
    float y = second.linear.y + second.angular * point->second_offset.x;
    y = y - first.linear.y;
    y = y - first.angular * point->first_offset.x;
    float x = second.linear.x - second.angular * point->second_offset.y;
    x = x - first.linear.x;
    x = x + first.angular * point->first_offset.y;
    return (AbVec2){x, y};
}

static void apply_impulse(AbVelocity *first, AbVelocity *second,
                          const AbVelocityContact *contact,
                          const AbVelocityPoint *point, AbVec2 impulse) {
    float first_torque =
        point->first_offset.x * impulse.y - point->first_offset.y * impulse.x;
    float second_torque =
        point->second_offset.x * impulse.y - point->second_offset.y * impulse.x;
    second->angular = second->angular + contact->second_inverse_inertia * second_torque;
    first->angular = first->angular - contact->first_inverse_inertia * first_torque;
    first->linear.x = first->linear.x - contact->first_inverse_mass * impulse.x;
    first->linear.y = first->linear.y - contact->first_inverse_mass * impulse.y;
    second->linear.x = second->linear.x + contact->second_inverse_mass * impulse.x;
    second->linear.y = second->linear.y + contact->second_inverse_mass * impulse.y;
}

static void solve_friction(AbVelocity *first, AbVelocity *second,
                           AbVelocityContact *contact) {
    for (size_t index = 0; index < contact->point_count; ++index) {
        AbVelocityPoint *point = contact->points + index;
        AbVec2 difference = point_velocity_difference(*first, *second, point);
        float speed = (-contact->normal.x) * difference.y;
        speed = speed + contact->normal.y * difference.x;
        float error = speed - contact->tangent_speed;
        float candidate = point->tangent_impulse - error * point->tangent_mass;
        float limit = contact->friction * point->normal_impulse;
        float upper = limit > candidate ? candidate : limit;
        float next = -limit <= upper ? upper : -limit;
        float change = next - point->tangent_impulse;
        point->tangent_impulse = next;
        AbVec2 impulse = {change * contact->normal.y, change * (-contact->normal.x)};
        apply_impulse(first, second, contact, point, impulse);
    }
}

static float normal_error(AbVelocity first, AbVelocity second,
                          const AbVelocityContact *contact,
                          const AbVelocityPoint *point) {
    AbVec2 difference = point_velocity_difference(first, second, point);
    float speed = difference.y * contact->normal.y;
    speed = speed + difference.x * contact->normal.x;
    return speed - point->velocity_bias;
}

static void solve_single_normal(AbVelocity *first, AbVelocity *second,
                                AbVelocityContact *contact) {
    AbVelocityPoint *point = contact->points;
    float error = normal_error(*first, *second, contact, point);
    float candidate = point->normal_impulse - point->normal_mass * error;
    float next = candidate > 0 ? candidate : 0.0f;
    float change = next - point->normal_impulse;
    point->normal_impulse = next;
    AbVec2 impulse = {change * contact->normal.x, change * contact->normal.y};
    apply_impulse(first, second, contact, point, impulse);
}

static bool choose_normal_impulses(const AbVelocityContact *contact, AbVec2 error,
                                   AbVec2 *next) {
    const AbSymmetric2 *inverse = &contact->normal_inverse;
    next->x = -(error.x * inverse->xx + error.y * inverse->xy);
    next->y = -(error.x * inverse->xy + error.y * inverse->yy);
    if (next->x >= 0 && next->y >= 0) {
        return true;
    }
    next->x = -(contact->points[0].normal_mass * error.x);
    next->y = 0;
    if (next->x >= 0 && error.y + next->x * contact->normal_matrix.xy >= 0) {
        return true;
    }
    next->x = 0;
    next->y = -(contact->points[1].normal_mass * error.y);
    if (next->y >= 0 && error.x + next->y * contact->normal_matrix.xy >= 0) {
        return true;
    }
    next->x = 0;
    next->y = 0;
    return error.x >= 0 && error.y >= 0;
}

static void solve_block_normals(AbVelocity *first, AbVelocity *second,
                                AbVelocityContact *contact) {
    float old_first = contact->points[0].normal_impulse;
    float old_second = contact->points[1].normal_impulse;
    AbVec2 error = {normal_error(*first, *second, contact, &contact->points[0]),
                    normal_error(*first, *second, contact, &contact->points[1])};
    float first_offset = old_second * contact->normal_matrix.xy;
    first_offset = first_offset + old_first * contact->normal_matrix.xx;
    float second_offset = old_second * contact->normal_matrix.yy;
    second_offset = second_offset + old_first * contact->normal_matrix.xy;
    error.x = error.x - first_offset;
    error.y = error.y - second_offset;
    AbVec2 next;
    if (!choose_normal_impulses(contact, error, &next)) {
        return;
    }
    float change_first = next.x - old_first;
    float change_second = next.y - old_second;
    contact->points[0].normal_impulse = next.x;
    contact->points[1].normal_impulse = next.y;
    AbVec2 impulse_first = {change_first * contact->normal.x,
                            change_first * contact->normal.y};
    AbVec2 impulse_second = {change_second * contact->normal.x,
                             change_second * contact->normal.y};
    AbVec2 impulse = {impulse_first.x + impulse_second.x,
                      impulse_first.y + impulse_second.y};
    float first_torque_a = impulse_first.y * contact->points[0].first_offset.x -
                           impulse_first.x * contact->points[0].first_offset.y;
    float first_torque_b = impulse_second.y * contact->points[1].first_offset.x -
                           impulse_second.x * contact->points[1].first_offset.y;
    float second_torque_a = impulse_first.y * contact->points[0].second_offset.x -
                            impulse_first.x * contact->points[0].second_offset.y;
    float second_torque_b = impulse_second.y * contact->points[1].second_offset.x -
                            impulse_second.x * contact->points[1].second_offset.y;
    first->angular = first->angular -
                     (first_torque_a + first_torque_b) * contact->first_inverse_inertia;
    second->angular = second->angular + (second_torque_a + second_torque_b) *
                                            contact->second_inverse_inertia;
    first->linear.x = first->linear.x - contact->first_inverse_mass * impulse.x;
    first->linear.y = first->linear.y - contact->first_inverse_mass * impulse.y;
    second->linear.x = second->linear.x + contact->second_inverse_mass * impulse.x;
    second->linear.y = second->linear.y + contact->second_inverse_mass * impulse.y;
}

bool ab_contacts_solve_velocity(AbVelocity *velocities, size_t body_count,
                                AbVelocityContact *contacts, size_t contact_count) {
    if (contact_count && (!contacts || !velocities)) {
        return false;
    }
    for (size_t index = 0; index < contact_count; ++index) {
        if (contacts[index].point_count < 1 || contacts[index].point_count > 2 ||
            contacts[index].first_body >= body_count ||
            contacts[index].second_body >= body_count) {
            return false;
        }
    }
    for (size_t index = 0; index < contact_count; ++index) {
        AbVelocityContact *contact = contacts + index;
        AbVelocity first = velocities[contact->first_body];
        AbVelocity second = velocities[contact->second_body];
        solve_friction(&first, &second, contact);
        if (contact->point_count == 1) {
            solve_single_normal(&first, &second, contact);
        } else {
            solve_block_normals(&first, &second, contact);
        }
        velocities[contact->first_body] = first;
        velocities[contact->second_body] = second;
    }
    return true;
}
