#include "ab/physics_contact.h"

#include "internal.h"

#include <float.h>
#include <math.h>

static void surface_point(AbWorldManifold *output, size_t index, AbVec2 first,
                          AbVec2 second, AbVec2 normal) {
    output->points[index] =
        (AbVec2){(first.x + second.x) * 0.5f, (first.y + second.y) * 0.5f};
    float separation = (second.y - first.y) * normal.y;
    output->separations[index] = separation + (second.x - first.x) * normal.x;
}

static void circle_surfaces(AbWorldManifold *output, const AbManifold *manifold,
                            AbRigidTransform first_transform, float first_radius,
                            AbRigidTransform second_transform, float second_radius) {
    AbVec2 first = ab_physics_point(manifold->local_point, first_transform);
    AbVec2 second = ab_physics_point(manifold->points[0].local_point, second_transform);
    float dy = first.y - second.y;
    float dx = first.x - second.x;
    AbVec2 normal = {1, 0};
    if (dy * dy + dx * dx > FLT_EPSILON * FLT_EPSILON) {
        normal = (AbVec2){second.x - first.x, second.y - first.y};
        float length = sqrtf(normal.y * normal.y + normal.x * normal.x);
        if (length >= FLT_EPSILON) {
            float inverse = 1.0f / length;
            normal.x = inverse * normal.x;
            normal.y = inverse * normal.y;
        }
    }
    output->normal = normal;
    first.x = first.x + first_radius * normal.x;
    first.y = first.y + first_radius * normal.y;
    second.x = second.x - second_radius * normal.x;
    second.y = second.y - second_radius * normal.y;
    surface_point(output, 0, first, second, normal);
}

void ab_world_manifold(AbWorldManifold *output, const AbManifold *manifold,
                       AbRigidTransform first_transform, float first_radius,
                       AbRigidTransform second_transform, float second_radius) {
    if (!manifold->point_count) {
        return;
    }
    if (manifold->kind == AB_CONTACT_CIRCLES) {
        circle_surfaces(output, manifold, first_transform, first_radius,
                        second_transform, second_radius);
        return;
    }
    bool reverse = manifold->kind == AB_CONTACT_FACE_SECOND;
    AbRigidTransform reference_transform = reverse ? second_transform : first_transform;
    AbRigidTransform incident_transform = reverse ? first_transform : second_transform;
    float reference_radius = reverse ? second_radius : first_radius;
    float incident_radius = reverse ? first_radius : second_radius;
    AbVec2 normal = ab_physics_rotate(manifold->local_normal, reference_transform);
    AbVec2 plane = ab_physics_point(manifold->local_point, reference_transform);
    output->normal = normal;
    for (size_t index = 0; index < manifold->point_count; ++index) {
        AbVec2 clip =
            ab_physics_point(manifold->points[index].local_point, incident_transform);
        float distance = (clip.y - plane.y) * normal.y;
        distance = distance + (clip.x - plane.x) * normal.x;
        float correction = reference_radius - distance;
        AbVec2 reference = {clip.x + correction * normal.x,
                            clip.y + correction * normal.y};
        AbVec2 incident = {clip.x - incident_radius * normal.x,
                           clip.y - incident_radius * normal.y};
        surface_point(output, index, reference, incident, normal);
    }
    if (reverse) {
        output->normal = (AbVec2){-normal.x, -normal.y};
    }
}
