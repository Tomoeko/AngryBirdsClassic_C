#include "ab/physics_geometry.h"

#include "internal.h"

#include <float.h>

_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   FLT_MAX_EXP == 128,
               "Target physics geometry requires binary32 float");

AbMass ab_circle_mass(AbCircle circle, float density) {
    float mass = density * 0x1.921fb6p+1f;
    mass = mass * circle.radius;
    mass = circle.radius * mass;
    float distance_squared = circle.center.y * circle.center.y;
    distance_squared = distance_squared + circle.center.x * circle.center.x;
    float half_radius = circle.radius * 0.5f;
    float inertia = distance_squared + circle.radius * half_radius;
    return (AbMass){mass, circle.center, mass * inertia};
}

AbAabb ab_circle_bounds(AbCircle circle, AbRigidTransform transform) {
    AbVec2 center = ab_physics_point(circle.center, transform);
    return (AbAabb){{center.x - circle.radius, center.y - circle.radius},
                    {circle.radius + center.x, circle.radius + center.y}};
}

bool ab_circle_contains(AbCircle circle, AbRigidTransform transform, AbVec2 point) {
    AbVec2 center = ab_physics_point(circle.center, transform);
    float dy = point.y - center.y;
    float dx = point.x - center.x;
    float distance_squared = dy * dy;
    distance_squared = distance_squared + dx * dx;
    return circle.radius * circle.radius >= distance_squared;
}

AbPolygon ab_polygon_box(float half_width, float half_height, float radius) {
    return (AbPolygon){.vertices = {{-half_width, -half_height},
                                    {half_width, -half_height},
                                    {half_width, half_height},
                                    {-half_width, half_height}},
                       .normals = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}},
                       .count = 4,
                       .radius = radius};
}

AbMass ab_polygon_mass(const AbPolygon *polygon, float density) {
    AbVec2 mean = {0};
    for (size_t index = 0; index < polygon->count; ++index) {
        mean.x = mean.x + polygon->vertices[index].x;
        mean.y = mean.y + polygon->vertices[index].y;
    }
    float inverse_count = 1.0f / (float)polygon->count;
    mean.x = inverse_count * mean.x;
    mean.y = inverse_count * mean.y;

    float area = 0;
    float area_moment = 0;
    AbVec2 weighted_center = {0};
    for (size_t index = 0; index < polygon->count; ++index) {
        size_t next = index + 1 == polygon->count ? 0 : index + 1;
        AbVec2 first = {polygon->vertices[index].x - mean.x,
                        polygon->vertices[index].y - mean.y};
        AbVec2 second = {polygon->vertices[next].x - mean.x,
                         polygon->vertices[next].y - mean.y};
        float cross = first.x * second.y - first.y * second.x;
        float triangle_area = cross * 0.5f;
        float weight = triangle_area * 0x1.555556p-2f;
        area = area + triangle_area;
        weighted_center.x = weighted_center.x + weight * (first.x + second.x);
        weighted_center.y = weighted_center.y + weight * (first.y + second.y);

        float x_moment = first.x * second.x + first.x * first.x;
        x_moment = x_moment + second.x * second.x;
        float y_moment = first.y * second.y + first.y * first.y;
        y_moment = y_moment + second.y * second.y;
        float moment_weight = cross * 0x1.555556p-4f;
        area_moment = area_moment + moment_weight * (x_moment + y_moment);
    }
    float inverse_area = 1.0f / area;
    AbVec2 relative = {inverse_area * weighted_center.x,
                       inverse_area * weighted_center.y};
    AbVec2 center = {mean.x + relative.x, mean.y + relative.y};
    float center_squared = center.y * center.y + center.x * center.x;
    float relative_squared = relative.y * relative.y + relative.x * relative.x;
    float mass = density * area;
    float inertia = mass * (center_squared - relative_squared);
    inertia = inertia + density * area_moment;
    return (AbMass){mass, center, inertia};
}

AbAabb ab_polygon_bounds(const AbPolygon *polygon, AbRigidTransform transform) {
    AbVec2 first = ab_physics_point(polygon->vertices[0], transform);
    AbAabb bounds = {first, first};
    for (size_t index = 1; index < polygon->count; ++index) {
        AbVec2 point = ab_physics_point(polygon->vertices[index], transform);
        bounds.lower.x = point.x <= bounds.lower.x ? point.x : bounds.lower.x;
        bounds.lower.y = point.y <= bounds.lower.y ? point.y : bounds.lower.y;
        bounds.upper.x = point.x >= bounds.upper.x ? point.x : bounds.upper.x;
        bounds.upper.y = point.y >= bounds.upper.y ? point.y : bounds.upper.y;
    }
    bounds.lower.x = bounds.lower.x - polygon->radius;
    bounds.lower.y = bounds.lower.y - polygon->radius;
    bounds.upper.x = polygon->radius + bounds.upper.x;
    bounds.upper.y = polygon->radius + bounds.upper.y;
    return bounds;
}

bool ab_polygon_contains(const AbPolygon *polygon, AbRigidTransform transform,
                         AbVec2 point) {
    AbVec2 local = ab_physics_inverse(point, transform);
    for (size_t index = 0; index < polygon->count; ++index) {
        AbVec2 vertex = polygon->vertices[index];
        AbVec2 normal = polygon->normals[index];
        float distance = (local.y - vertex.y) * normal.y;
        distance = distance + (local.x - vertex.x) * normal.x;
        if (distance > 0) {
            return false;
        }
    }
    return true;
}
