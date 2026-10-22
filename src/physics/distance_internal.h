#ifndef AB_PHYSICS_DISTANCE_INTERNAL_H
#define AB_PHYSICS_DISTANCE_INTERNAL_H

#include "ab/physics_distance.h"

typedef struct {
    AbVec2 first;
    AbVec2 second;
    AbVec2 difference;
    float weight;
    uint8_t first_index;
    uint8_t second_index;
} AbDistanceVertex;

typedef struct {
    AbDistanceVertex vertices[3];
    size_t count;
} AbDistanceSimplex;

static inline AbVec2 ab_distance_subtract(AbVec2 first, AbVec2 second) {
    return (AbVec2){first.x - second.x, first.y - second.y};
}

static inline float ab_distance_dot(AbVec2 first, AbVec2 second) {
    float result = first.y * second.y;
    return result + first.x * second.x;
}

static inline float ab_distance_cross(AbVec2 first, AbVec2 second) {
    return first.x * second.y - first.y * second.x;
}

void ab_distance_reduce_segment(AbDistanceSimplex *simplex);
void ab_distance_reduce_triangle(AbDistanceSimplex *simplex);
bool ab_distance_proxy_valid(AbDistanceProxy proxy, AbRigidTransform transform);
uint8_t ab_distance_support_index(AbDistanceProxy proxy, AbRigidTransform transform,
                                  AbVec2 direction);

#endif
