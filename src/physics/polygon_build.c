#include "ab/physics_geometry.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

static size_t distinct_points(const AbVec2 *input, size_t count, AbVec2 output[8]) {
    size_t distinct = 0;
    for (size_t index = 0; index < count; ++index) {
        bool duplicate = false;
        for (size_t previous = 0; previous < distinct; ++previous) {
            float dy = input[index].y - output[previous].y;
            float dx = input[index].x - output[previous].x;
            /* Native squared-distance threshold bits are 0x3ccccccd. */
            if (dy * dy + dx * dx < 0x1.99999ap-6f) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            output[distinct++] = input[index];
        }
    }
    return distinct;
}

static size_t next_hull_vertex(const AbVec2 *points, size_t count, size_t current) {
    size_t selected = 0;
    for (size_t index = 1; index < count; ++index) {
        if (selected == current) {
            selected = index;
            continue;
        }
        AbVec2 first = {points[selected].x - points[current].x,
                        points[selected].y - points[current].y};
        AbVec2 second = {points[index].x - points[current].x,
                         points[index].y - points[current].y};
        float cross = first.x * second.y - first.y * second.x;
        if (cross < 0 || (cross == 0 && second.y * second.y + second.x * second.x >
                                            first.y * first.y + first.x * first.x)) {
            selected = index;
        }
    }
    return selected;
}

static bool finish_polygon(AbPolygon *polygon) {
    for (size_t index = 0; index < polygon->count; ++index) {
        size_t next = index + 1 == polygon->count ? 0 : index + 1;
        AbVec2 normal = {polygon->vertices[next].y - polygon->vertices[index].y,
                         -(polygon->vertices[next].x - polygon->vertices[index].x)};
        float length = sqrtf(normal.y * normal.y + normal.x * normal.x);
        if (!isfinite(length)) {
            return false;
        }
        if (length >= FLT_EPSILON) {
            float inverse = 1.0f / length;
            normal.x = normal.x * inverse;
            normal.y = normal.y * inverse;
        }
        polygon->normals[index] = normal;
    }
    float area = 0;
    AbVec2 centroid = {0};
    for (size_t index = 0; index < polygon->count; ++index) {
        size_t next = index + 1 == polygon->count ? 0 : index + 1;
        AbVec2 first = polygon->vertices[index];
        AbVec2 second = polygon->vertices[next];
        float cross = first.x * second.y - first.y * second.x;
        float triangle_area = cross * 0.5f;
        float weight = triangle_area * 0x1.555556p-2f;
        area = area + triangle_area;
        centroid.x = centroid.x + weight * (second.x + (first.x + 0.0f));
        centroid.y = centroid.y + weight * (second.y + (first.y + 0.0f));
    }
    float inverse_area = 1.0f / area;
    polygon->centroid = (AbVec2){centroid.x * inverse_area, centroid.y * inverse_area};
    return isfinite(polygon->centroid.x) && isfinite(polygon->centroid.y);
}

bool ab_polygon_build(const AbVec2 *vertices, size_t count, float radius,
                      AbPolygon *output) {
    if (!output) {
        return false;
    }
    if (count > INT32_MAX || !isfinite(radius) || radius < 0 ||
        (count > 2 && !vertices)) {
        *output = (AbPolygon){0};
        return false;
    }
    AbVec2 input[8];
    size_t used = count > AB_POLYGON_VERTICES ? AB_POLYGON_VERTICES : count;
    for (size_t index = 0; index < used && count > 2; ++index) {
        if (!isfinite(vertices[index].x) || !isfinite(vertices[index].y)) {
            *output = (AbPolygon){0};
            return false;
        }
        input[index] = vertices[index];
    }
    *output = (AbPolygon){0};
    AbPolygon polygon = ab_polygon_box(1, 1, radius);
    if (count <= 2) {
        *output = polygon;
        return true;
    }
    AbVec2 points[8];
    size_t distinct = distinct_points(input, used, points);
    if (distinct < 3) {
        *output = polygon;
        return true;
    }
    size_t start = 0;
    for (size_t index = 1; index < distinct; ++index) {
        if (points[index].x > points[start].x ||
            (points[index].x == points[start].x && points[index].y < points[start].y)) {
            start = index;
        }
    }
    size_t current = start;
    polygon.count = 0;
    do {
        if (polygon.count == distinct) {
            return false;
        }
        polygon.vertices[polygon.count++] = points[current];
        current = next_hull_vertex(points, distinct, current);
    } while (current != start);
    if (polygon.count < 3) {
        *output = ab_polygon_box(1, 1, radius);
        return true;
    }
    if (!finish_polygon(&polygon)) {
        return false;
    }
    *output = polygon;
    return true;
}
