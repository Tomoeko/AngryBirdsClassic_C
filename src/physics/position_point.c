#include "ab/physics_solver.h"

#include "internal.h"
#include "position_internal.h"

#include <float.h>
#include <math.h>

typedef struct {
    AbVec2 point;
    AbVec2 normal;
    float separation;
} PositionManifold;

static PositionManifold circle_manifold(const AbPositionContact *contact,
                                        AbRigidTransform first,
                                        AbRigidTransform second) {
    AbVec2 first_point = ab_physics_point(contact->local_point, first);
    AbVec2 second_point = ab_physics_point(contact->local_points[0], second);
    AbVec2 difference = {second_point.x - first_point.x,
                         second_point.y - first_point.y};
    AbVec2 normal = difference;
    float separation = difference.y * difference.y + difference.x * difference.x;
    float length = sqrtf(separation);
    if (length >= FLT_EPSILON) {
        float inverse = 1.0f / length;
        normal.x = difference.x * inverse;
        normal.y = difference.y * inverse;
        separation = difference.y * normal.y + difference.x * normal.x;
    }
    separation = separation - contact->first_radius;
    separation = separation - contact->second_radius;
    return (PositionManifold){{(first_point.x + second_point.x) * 0.5f,
                               (first_point.y + second_point.y) * 0.5f},
                              normal,
                              separation};
}

static PositionManifold face_manifold(const AbPositionContact *contact, size_t point,
                                      AbRigidTransform first, AbRigidTransform second) {
    bool reversed = contact->kind == AB_CONTACT_FACE_SECOND;
    AbRigidTransform face = reversed ? second : first;
    AbRigidTransform incident = reversed ? first : second;
    AbVec2 normal = ab_physics_rotate(contact->local_normal, face);
    AbVec2 plane = ab_physics_point(contact->local_point, face);
    AbVec2 clip = ab_physics_point(contact->local_points[point], incident);
    float separation = (clip.y - plane.y) * normal.y;
    separation = separation + (clip.x - plane.x) * normal.x;
    separation = separation - contact->first_radius;
    separation = separation - contact->second_radius;
    if (reversed) {
        normal.x = -normal.x;
        normal.y = -normal.y;
    }
    return (PositionManifold){clip, normal, separation};
}

bool ab_contact_position_point_rate(const AbPositionContact *contact, size_t point,
                                    AbPositionBody *first, AbPositionBody *second,
                                    float *minimum_separation, float correction_rate) {
    if (!contact || contact->point_count < 1 || contact->point_count > 2 ||
        point >= contact->point_count ||
        (unsigned)contact->kind > AB_CONTACT_FACE_SECOND) {
        return false;
    }
    AbPositionBody first_state = *first;
    AbPositionBody second_state = *second;
    PositionManifold manifold =
        contact->kind == AB_CONTACT_CIRCLES
            ? circle_manifold(contact, first->transform, second->transform)
            : face_manifold(contact, point, first->transform, second->transform);
    if (manifold.separation <= *minimum_separation) {
        *minimum_separation = manifold.separation;
    }
    float correction = (manifold.separation + 0.05f) * correction_rate;
    if (correction >= 0) {
        correction = 0;
    } else if (correction < -0.2f) {
        correction = -0.2f;
    }
    AbVec2 first_offset = {manifold.point.x - first_state.center.x,
                           manifold.point.y - first_state.center.y};
    AbVec2 second_offset = {manifold.point.x - second_state.center.x,
                            manifold.point.y - second_state.center.y};
    float first_cross =
        first_offset.x * manifold.normal.y - first_offset.y * manifold.normal.x;
    float second_cross =
        second_offset.x * manifold.normal.y - second_offset.y * manifold.normal.x;
    float denominator = first_state.inverse_mass + second_state.inverse_mass;
    float first_weight = first_state.inverse_inertia * first_cross;
    denominator = denominator + first_weight * first_cross;
    float second_weight = second_state.inverse_inertia * second_cross;
    denominator = denominator + second_weight * second_cross;
    float magnitude = denominator > 0 ? -correction / denominator : 0.0f;
    AbVec2 impulse = {magnitude * manifold.normal.x, magnitude * manifold.normal.y};
    float first_torque = first_offset.x * impulse.y - first_offset.y * impulse.x;
    float second_torque = second_offset.x * impulse.y - second_offset.y * impulse.x;
    second_state.angle =
        second_state.angle + second_state.inverse_inertia * second_torque;
    first_state.angle = first_state.angle - first_state.inverse_inertia * first_torque;
    first_state.center.x = first_state.center.x - first_state.inverse_mass * impulse.x;
    first_state.center.y = first_state.center.y - first_state.inverse_mass * impulse.y;
    second_state.center.x =
        second_state.center.x + second_state.inverse_mass * impulse.x;
    second_state.center.y =
        second_state.center.y + second_state.inverse_mass * impulse.y;
    *first = first_state;
    *second = second_state;
    return true;
}

bool ab_contact_position_point(const AbPositionContact *contact, size_t point,
                               AbPositionBody *first, AbPositionBody *second,
                               float *minimum_separation) {
    return ab_contact_position_point_rate(contact, point, first, second,
                                          minimum_separation, 0.2f);
}
