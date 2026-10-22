#include "distance_internal.h"

#include "internal.h"

#include <float.h>
#include <math.h>

bool ab_distance_proxy_valid(AbDistanceProxy proxy, AbRigidTransform transform) {
    if (!proxy.vertices || proxy.count < 1 || proxy.count > AB_POLYGON_VERTICES ||
        !isfinite(proxy.radius) || proxy.radius < 0 ||
        !isfinite(transform.position.x) || !isfinite(transform.position.y) ||
        !isfinite(transform.sine) || !isfinite(transform.cosine)) {
        return false;
    }
    for (size_t vertex = 0; vertex < proxy.count; ++vertex) {
        if (!isfinite(proxy.vertices[vertex].x) ||
            !isfinite(proxy.vertices[vertex].y)) {
            return false;
        }
    }
    return true;
}

static AbDistanceVertex make_vertex(AbDistanceProxy first, AbRigidTransform first_tf,
                                    AbDistanceProxy second, AbRigidTransform second_tf,
                                    uint8_t first_index, uint8_t second_index) {
    AbDistanceVertex vertex = {
        .first = ab_physics_point(first.vertices[first_index], first_tf),
        .second = ab_physics_point(second.vertices[second_index], second_tf),
        .first_index = first_index,
        .second_index = second_index,
    };
    vertex.difference = ab_distance_subtract(vertex.second, vertex.first);
    return vertex;
}

static float simplex_metric(const AbDistanceSimplex *simplex) {
    if (simplex->count < 2) {
        return 0;
    }
    AbVec2 edge = ab_distance_subtract(simplex->vertices[1].difference,
                                       simplex->vertices[0].difference);
    if (simplex->count == 2) {
        return sqrtf(ab_distance_dot(edge, edge));
    }
    AbVec2 other = ab_distance_subtract(simplex->vertices[2].difference,
                                        simplex->vertices[0].difference);
    return ab_distance_cross(edge, other);
}

static AbVec2 search_direction(const AbDistanceSimplex *simplex) {
    AbVec2 first = simplex->vertices[0].difference;
    if (simplex->count == 1) {
        return (AbVec2){-first.x, -first.y};
    }
    AbVec2 edge = ab_distance_subtract(simplex->vertices[1].difference, first);
    if (ab_distance_cross(edge, (AbVec2){-first.x, -first.y}) > 0) {
        return (AbVec2){-edge.y, edge.x};
    }
    return (AbVec2){edge.y, -edge.x};
}

uint8_t ab_distance_support_index(AbDistanceProxy proxy, AbRigidTransform transform,
                                  AbVec2 direction) {
    AbVec2 local = {direction.y * transform.sine + direction.x * transform.cosine,
                    direction.y * transform.cosine - direction.x * transform.sine};
    uint8_t best = 0;
    float value = ab_distance_dot(proxy.vertices[0], local);
    for (size_t vertex = 1; vertex < proxy.count; ++vertex) {
        float candidate = ab_distance_dot(proxy.vertices[vertex], local);
        if (candidate > value) {
            value = candidate;
            best = (uint8_t)vertex;
        }
    }
    return best;
}

static uint32_t extend_simplex(AbDistanceSimplex *simplex, AbDistanceProxy first,
                               AbRigidTransform first_tf, AbDistanceProxy second,
                               AbRigidTransform second_tf) {
    uint32_t iterations = 0;
    while (iterations < 20) {
        uint8_t previous_first[3], previous_second[3];
        size_t previous_count = simplex->count;
        for (size_t vertex = 0; vertex < previous_count; ++vertex) {
            previous_first[vertex] = simplex->vertices[vertex].first_index;
            previous_second[vertex] = simplex->vertices[vertex].second_index;
        }
        if (simplex->count == 2) {
            ab_distance_reduce_segment(simplex);
        } else if (simplex->count == 3) {
            ab_distance_reduce_triangle(simplex);
        }
        if (simplex->count == 3) {
            break;
        }
        AbVec2 direction = search_direction(simplex);
        if (ab_distance_dot(direction, direction) < FLT_EPSILON * FLT_EPSILON) {
            break;
        }
        uint8_t first_index = ab_distance_support_index(
            first, first_tf, (AbVec2){-direction.x, -direction.y});
        uint8_t second_index = ab_distance_support_index(second, second_tf, direction);
        simplex->vertices[simplex->count] =
            make_vertex(first, first_tf, second, second_tf, first_index, second_index);
        ++iterations;
        bool duplicate = false;
        for (size_t vertex = 0; vertex < previous_count; ++vertex) {
            if (previous_first[vertex] == first_index &&
                previous_second[vertex] == second_index) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            break;
        }
        ++simplex->count;
    }
    return iterations;
}

static AbDistanceResult witness_points(const AbDistanceSimplex *simplex) {
    const AbDistanceVertex *vertices = simplex->vertices;
    AbDistanceResult result;
    if (simplex->count == 1) {
        result = (AbDistanceResult){.first = vertices[0].first,
                                    .second = vertices[0].second};
    } else {
        result = (AbDistanceResult){
            .first = {vertices[1].weight * vertices[1].first.x +
                          vertices[0].weight * vertices[0].first.x,
                      vertices[1].weight * vertices[1].first.y +
                          vertices[0].weight * vertices[0].first.y},
            .second = {vertices[1].weight * vertices[1].second.x +
                           vertices[0].weight * vertices[0].second.x,
                       vertices[1].weight * vertices[1].second.y +
                           vertices[0].weight * vertices[0].second.y},
        };
        if (simplex->count == 3) {
            result.first.x = result.first.x + vertices[2].weight * vertices[2].first.x;
            result.first.y = result.first.y + vertices[2].weight * vertices[2].first.y;
            result.second = result.first;
        }
    }
    AbVec2 delta = ab_distance_subtract(result.first, result.second);
    result.distance = sqrtf(ab_distance_dot(delta, delta));
    return result;
}

static void apply_radii(AbDistanceResult *result, float first_radius,
                        float second_radius) {
    float radius = first_radius + second_radius;
    if (result->distance > radius && result->distance > FLT_EPSILON) {
        AbVec2 normal = ab_distance_subtract(result->second, result->first);
        float inverse = 1.0f / sqrtf(ab_distance_dot(normal, normal));
        normal.x = normal.x * inverse;
        normal.y = normal.y * inverse;
        result->distance = result->distance - radius;
        result->first.x = result->first.x + first_radius * normal.x;
        result->first.y = result->first.y + first_radius * normal.y;
        result->second.x = result->second.x - second_radius * normal.x;
        result->second.y = result->second.y - second_radius * normal.y;
    } else {
        result->first.x = 0.5f * (result->first.x + result->second.x);
        result->first.y = 0.5f * (result->first.y + result->second.y);
        result->second = result->first;
        result->distance = 0;
    }
}

bool ab_shape_distance(AbDistanceProxy first, AbRigidTransform first_transform,
                       AbDistanceProxy second, AbRigidTransform second_transform,
                       bool use_radii, AbDistanceCache *cache,
                       AbDistanceResult *output) {
    if (!cache || !output || cache->count > 3 || !isfinite(cache->metric) ||
        !ab_distance_proxy_valid(first, first_transform) ||
        !ab_distance_proxy_valid(second, second_transform)) {
        return false;
    }
    AbDistanceSimplex simplex = {.count = cache->count};
    for (size_t vertex = 0; vertex < simplex.count; ++vertex) {
        if (cache->first[vertex] >= first.count ||
            cache->second[vertex] >= second.count) {
            return false;
        }
        simplex.vertices[vertex] =
            make_vertex(first, first_transform, second, second_transform,
                        cache->first[vertex], cache->second[vertex]);
    }
    if (simplex.count > 1) {
        float metric = simplex_metric(&simplex);
        if (metric < 0.5f * cache->metric || metric > 2.0f * cache->metric ||
            metric < FLT_EPSILON) {
            simplex.count = 0;
        }
    }
    if (!simplex.count) {
        simplex.count = 1;
        simplex.vertices[0] =
            make_vertex(first, first_transform, second, second_transform, 0, 0);
        simplex.vertices[0].weight = 1;
    }
    uint32_t iterations =
        extend_simplex(&simplex, first, first_transform, second, second_transform);
    AbDistanceResult result = witness_points(&simplex);
    result.iterations = iterations;
    AbDistanceCache updated = *cache;
    updated.metric = simplex_metric(&simplex);
    updated.count = (uint16_t)simplex.count;
    for (size_t vertex = 0; vertex < simplex.count; ++vertex) {
        updated.first[vertex] = simplex.vertices[vertex].first_index;
        updated.second[vertex] = simplex.vertices[vertex].second_index;
    }
    if (use_radii) {
        apply_radii(&result, first.radius, second.radius);
    }
    if (!isfinite(result.distance) || !isfinite(result.first.x) ||
        !isfinite(result.first.y) || !isfinite(result.second.x) ||
        !isfinite(result.second.y) || !isfinite(updated.metric)) {
        return false;
    }
    *cache = updated;
    *output = result;
    return true;
}

bool ab_shape_overlap(AbDistanceProxy first, AbRigidTransform first_transform,
                      AbDistanceProxy second, AbRigidTransform second_transform,
                      bool *overlap) {
    if (!overlap) {
        return false;
    }
    AbDistanceCache cache = {0};
    AbDistanceResult result;
    if (!ab_shape_distance(first, first_transform, second, second_transform, true,
                           &cache, &result)) {
        return false;
    }
    *overlap = result.distance < 10.0f * FLT_EPSILON;
    return true;
}
