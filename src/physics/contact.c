#include "ab/physics_contact.h"

#include "internal.h"

#include <float.h>
#include <math.h>

void ab_collide_circles(AbManifold *output, AbCircle first,
                        AbRigidTransform first_transform, AbCircle second,
                        AbRigidTransform second_transform) {
    output->point_count = 0;
    AbVec2 first_center = ab_physics_point(first.center, first_transform);
    AbVec2 second_center = ab_physics_point(second.center, second_transform);
    float dy = second_center.y - first_center.y;
    float dx = second_center.x - first_center.x;
    float distance_squared = dy * dy + dx * dx;
    float radius = first.radius + second.radius;
    if (distance_squared > radius * radius) {
        return;
    }
    output->kind = AB_CONTACT_CIRCLES;
    output->local_point = first.center;
    output->local_normal = (AbVec2){0, 0};
    output->points[0].local_point = second.center;
    output->points[0].feature_id = 0;
    output->point_count = 1;
}

static void polygon_circle_contact(AbManifold *output, AbCircle circle, AbVec2 normal,
                                   AbVec2 local_point) {
    output->kind = AB_CONTACT_FACE_FIRST;
    output->local_normal = normal;
    output->local_point = local_point;
    output->points[0].local_point = circle.center;
    output->points[0].feature_id = 0;
    output->point_count = 1;
}

static void polygon_vertex_contact(AbManifold *output, AbCircle circle, AbVec2 center,
                                   AbVec2 vertex, float radius) {
    AbVec2 normal = {center.x - vertex.x, center.y - vertex.y};
    float distance_squared = normal.y * normal.y + normal.x * normal.x;
    if (distance_squared > radius * radius) {
        return;
    }
    float length = sqrtf(distance_squared);
    if (length >= FLT_EPSILON) {
        float inverse_length = 1.0f / length;
        normal.x = normal.x * inverse_length;
        normal.y = normal.y * inverse_length;
    }
    polygon_circle_contact(output, circle, normal, vertex);
}

void ab_collide_polygon_circle(AbManifold *output, const AbPolygon *polygon,
                               AbRigidTransform polygon_transform, AbCircle circle,
                               AbRigidTransform circle_transform) {
    output->point_count = 0;
    AbVec2 center = ab_physics_inverse(
        ab_physics_point(circle.center, circle_transform), polygon_transform);
    float radius = polygon->radius + circle.radius;
    float separation = -FLT_MAX;
    size_t edge = 0;
    for (size_t index = 0; index < polygon->count; ++index) {
        AbVec2 vertex = polygon->vertices[index];
        AbVec2 normal = polygon->normals[index];
        float distance = (center.y - vertex.y) * normal.y;
        distance = distance + (center.x - vertex.x) * normal.x;
        if (distance > radius) {
            return;
        }
        if (distance > separation) {
            separation = distance;
            edge = index;
        }
    }
    size_t next = edge + 1 == polygon->count ? 0 : edge + 1;
    AbVec2 first = polygon->vertices[edge];
    AbVec2 second = polygon->vertices[next];
    AbVec2 midpoint = {(first.x + second.x) * 0.5f, (first.y + second.y) * 0.5f};
    if (separation < FLT_EPSILON) {
        polygon_circle_contact(output, circle, polygon->normals[edge], midpoint);
        return;
    }
    float first_region = (center.y - first.y) * (second.y - first.y);
    first_region = first_region + (center.x - first.x) * (second.x - first.x);
    if (first_region <= 0) {
        polygon_vertex_contact(output, circle, center, first, radius);
        return;
    }
    float second_region = (center.y - second.y) * (first.y - second.y);
    second_region = second_region + (center.x - second.x) * (first.x - second.x);
    if (second_region <= 0) {
        polygon_vertex_contact(output, circle, center, second, radius);
        return;
    }
    AbVec2 normal = polygon->normals[edge];
    float distance = (center.y - midpoint.y) * normal.y;
    distance = distance + (center.x - midpoint.x) * normal.x;
    if (distance <= radius) {
        polygon_circle_contact(output, circle, normal, midpoint);
    }
}
