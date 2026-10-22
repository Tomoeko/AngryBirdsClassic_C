#include "ab/physics_geometry.h"

#include "internal.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

bool ab_circle_raycast(AbCircle circle, AbRigidTransform transform, AbRay ray,
                       AbRayHit *output) {
    AbVec2 center = ab_physics_point(circle.center, transform);
    AbVec2 start = {ray.first.x - center.x, ray.first.y - center.y};
    AbVec2 direction = {ray.second.x - ray.first.x, ray.second.y - ray.first.y};
    float outside = start.y * start.y + start.x * start.x;
    outside = outside - circle.radius * circle.radius;
    float length_squared = direction.y * direction.y + direction.x * direction.x;
    float projected = start.y * direction.y + start.x * direction.x;
    float discriminant = projected * projected - outside * length_squared;
    if (discriminant < 0 || length_squared < FLT_EPSILON) {
        return false;
    }
    float numerator = -(sqrtf(discriminant) + projected);
    /* The native ordered comparisons reject unordered numerators and limits. */
    if (!(numerator >= 0) || !(numerator <= length_squared * ray.max_fraction)) {
        return false;
    }
    float fraction = numerator / length_squared;
    AbVec2 normal = {start.x + fraction * direction.x,
                     start.y + fraction * direction.y};
    float length = sqrtf(normal.y * normal.y + normal.x * normal.x);
    if (length >= FLT_EPSILON) {
        float inverse = 1.0f / length;
        normal.x = normal.x * inverse;
        normal.y = normal.y * inverse;
    }
    *output = (AbRayHit){normal, fraction};
    return true;
}

static AbVec2 polygon_ray_point(AbVec2 point, AbRigidTransform transform) {
    float dy = point.y - transform.position.y;
    float dx = point.x - transform.position.x;
    /* This native path uses an added negative product for the second component. */
    return (AbVec2){dy * transform.sine + dx * transform.cosine,
                    dy * transform.cosine + -transform.sine * dx};
}

bool ab_polygon_raycast(const AbPolygon *polygon, AbRigidTransform transform, AbRay ray,
                        AbRayHit *output) {
    AbVec2 first = polygon_ray_point(ray.first, transform);
    AbVec2 second = polygon_ray_point(ray.second, transform);
    AbVec2 direction = {second.x - first.x, second.y - first.y};
    float lower = 0;
    float upper = ray.max_fraction;
    size_t selected = SIZE_MAX;
    for (size_t index = 0; index < polygon->count; ++index) {
        AbVec2 vertex = polygon->vertices[index];
        AbVec2 normal = polygon->normals[index];
        float denominator = direction.y * normal.y + direction.x * normal.x;
        float numerator = (vertex.y - first.y) * normal.y;
        numerator = numerator + (vertex.x - first.x) * normal.x;
        if (denominator == 0) {
            if (numerator < 0) {
                return false;
            }
        } else if (denominator < 0 && numerator < lower * denominator) {
            lower = numerator / denominator;
            selected = index;
        } else if (denominator > 0 && numerator < upper * denominator) {
            upper = numerator / denominator;
        }
        if (upper < lower) {
            return false;
        }
    }
    if (selected == SIZE_MAX) {
        return false;
    }
    *output =
        (AbRayHit){ab_physics_rotate(polygon->normals[selected], transform), lower};
    return true;
}
