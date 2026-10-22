#include "distance_internal.h"

void ab_distance_reduce_segment(AbDistanceSimplex *simplex) {
    AbDistanceVertex *vertices = simplex->vertices;
    AbVec2 edge = ab_distance_subtract(vertices[1].difference, vertices[0].difference);
    float toward_second = -ab_distance_dot(vertices[0].difference, edge);
    if (toward_second <= 0) {
        vertices[0].weight = 1;
        simplex->count = 1;
        return;
    }
    float toward_first = ab_distance_dot(vertices[1].difference, edge);
    if (toward_first <= 0) {
        vertices[1].weight = 1;
        vertices[0] = vertices[1];
        simplex->count = 1;
        return;
    }
    float inverse = 1.0f / (toward_first + toward_second);
    vertices[0].weight = inverse * toward_first;
    vertices[1].weight = toward_second * inverse;
    simplex->count = 2;
}

void ab_distance_reduce_triangle(AbDistanceSimplex *simplex) {
    AbDistanceVertex *vertices = simplex->vertices;
    AbVec2 first = vertices[0].difference, second = vertices[1].difference;
    AbVec2 third = vertices[2].difference;
    AbVec2 edge_first = ab_distance_subtract(second, first);
    AbVec2 edge_second = ab_distance_subtract(third, first);
    float first_to_second = -ab_distance_dot(first, edge_first);
    float second_to_first = ab_distance_dot(second, edge_first);
    float first_to_third = -ab_distance_dot(first, edge_second);
    float third_to_first = ab_distance_dot(third, edge_second);
    float area = ab_distance_cross(edge_first, edge_second);
    float first_region = area * ab_distance_cross(second, third);
    float second_region = area * ab_distance_cross(third, first);
    float third_region = area * ab_distance_cross(first, second);

    if (first_to_second <= 0 && first_to_third <= 0) {
        vertices[0].weight = 1;
        simplex->count = 1;
        return;
    }
    if (second_to_first > 0 && first_to_second > 0 && third_region <= 0) {
        float inverse = 1.0f / (second_to_first + first_to_second);
        vertices[0].weight = inverse * second_to_first;
        vertices[1].weight = first_to_second * inverse;
        simplex->count = 2;
        return;
    }
    if (third_to_first > 0 && first_to_third > 0 && second_region <= 0) {
        float inverse = 1.0f / (third_to_first + first_to_third);
        vertices[0].weight = inverse * third_to_first;
        vertices[2].weight = first_to_third * inverse;
        vertices[1] = vertices[2];
        simplex->count = 2;
        return;
    }
    AbVec2 edge_third = ab_distance_subtract(third, second);
    float second_to_third = -ab_distance_dot(second, edge_third);
    if (second_to_first <= 0 && second_to_third <= 0) {
        vertices[1].weight = 1;
        vertices[0] = vertices[1];
        simplex->count = 1;
        return;
    }
    float third_to_second = ab_distance_dot(third, edge_third);
    if (third_to_first <= 0 && third_to_second <= 0) {
        vertices[2].weight = 1;
        vertices[0] = vertices[2];
        simplex->count = 1;
        return;
    }
    if (third_to_second > 0 && second_to_third > 0 && first_region <= 0) {
        float inverse = 1.0f / (third_to_second + second_to_third);
        vertices[1].weight = inverse * third_to_second;
        vertices[2].weight = second_to_third * inverse;
        vertices[0] = vertices[2];
        simplex->count = 2;
        return;
    }
    float inverse = 1.0f / ((first_region + second_region) + third_region);
    vertices[0].weight = first_region * inverse;
    vertices[1].weight = second_region * inverse;
    vertices[2].weight = third_region * inverse;
    simplex->count = 3;
}
