#include "ab/physics_solver.h"

static float normal_cross(AbVec2 offset, AbVec2 normal) {
    return offset.x * normal.y - offset.y * normal.x;
}

static float tangent_cross(AbVec2 offset, AbVec2 normal) {
    return offset.x * (-normal.x) - offset.y * normal.y;
}

static float mass_denominator(const AbVelocityContact *contact, float first_cross,
                              float second_cross) {
    float result = contact->first_inverse_mass + contact->second_inverse_mass;
    float first = contact->first_inverse_inertia * first_cross;
    result = result + first * first_cross;
    float second = contact->second_inverse_inertia * second_cross;
    return result + second * second_cross;
}

static float velocity_bias(const AbVelocityContact *contact,
                           const AbVelocityPoint *point, AbVelocity first,
                           AbVelocity second, float restitution) {
    float y = second.linear.y + second.angular * point->second_offset.x;
    y = y - first.linear.y;
    y = y - first.angular * point->first_offset.x;
    float x = second.linear.x + point->second_offset.y * (-second.angular);
    x = x - first.linear.x;
    x = x - point->first_offset.y * (-first.angular);
    float normal_velocity = contact->normal.y * y;
    normal_velocity = normal_velocity + contact->normal.x * x;
    return normal_velocity < -1.0f ? -(restitution * normal_velocity) : 0.0f;
}

static void initialize_block(AbVelocityContact *contact) {
    float first_a = normal_cross(contact->points[0].first_offset, contact->normal);
    float first_b = normal_cross(contact->points[1].first_offset, contact->normal);
    float second_a = normal_cross(contact->points[0].second_offset, contact->normal);
    float second_b = normal_cross(contact->points[1].second_offset, contact->normal);
    float sum_mass = contact->first_inverse_mass + contact->second_inverse_mass;
    float first_product = contact->first_inverse_inertia * first_a;
    float second_product = contact->second_inverse_inertia * second_a;
    float xx = sum_mass + first_product * first_a;
    xx = xx + second_product * second_a;
    float xy = sum_mass + first_product * first_b;
    xy = xy + second_product * second_b;
    first_product = contact->first_inverse_inertia * first_b;
    second_product = contact->second_inverse_inertia * second_b;
    float yy = sum_mass + first_product * first_b;
    yy = yy + second_product * second_b;
    float determinant = xx * yy - xy * xy;
    if (!(xx * xx < determinant * 1000.0f)) {
        contact->point_count = 1;
        return;
    }
    contact->normal_matrix = (AbSymmetric2){xx, xy, yy};
    float inverse = determinant != 0 ? 1.0f / determinant : determinant;
    contact->normal_inverse =
        (AbSymmetric2){yy * inverse, -(inverse * xy), xx * inverse};
}

bool ab_contact_velocity_initialize(AbVelocityContact *contact,
                                    const AbManifold *manifold, AbSolverFrame first,
                                    float first_radius, AbSolverFrame second,
                                    float second_radius, float restitution) {
    if (!contact || !manifold || contact->point_count < 1 || contact->point_count > 2 ||
        contact->point_count != manifold->point_count ||
        (unsigned)manifold->kind > AB_CONTACT_FACE_SECOND) {
        return false;
    }
    AbWorldManifold world = {0};
    ab_world_manifold(&world, manifold, first.transform, first_radius, second.transform,
                      second_radius);
    contact->normal = world.normal;
    for (size_t index = 0; index < contact->point_count; ++index) {
        AbVelocityPoint *point = contact->points + index;
        point->first_offset = (AbVec2){world.points[index].x - first.center.x,
                                       world.points[index].y - first.center.y};
        point->second_offset = (AbVec2){world.points[index].x - second.center.x,
                                        world.points[index].y - second.center.y};
        float denominator = mass_denominator(
            contact, normal_cross(point->first_offset, contact->normal),
            normal_cross(point->second_offset, contact->normal));
        point->normal_mass = denominator > 0 ? 1.0f / denominator : 0.0f;
        denominator = mass_denominator(
            contact, tangent_cross(point->first_offset, contact->normal),
            tangent_cross(point->second_offset, contact->normal));
        point->tangent_mass = denominator > 0 ? 1.0f / denominator : 0.0f;
        point->velocity_bias =
            velocity_bias(contact, point, first.velocity, second.velocity, restitution);
    }
    if (contact->point_count == 2) {
        initialize_block(contact);
    }
    return true;
}
