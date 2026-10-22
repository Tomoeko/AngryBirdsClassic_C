#include "ab/game_collision.h"

bool ab_game_collision_set(AbGameCollisionMatrix *matrix, uint32_t first,
                           uint32_t second, bool allowed) {
    if (!matrix || first >= AB_GAME_COLLIDER_TYPES ||
        second >= AB_GAME_COLLIDER_TYPES) {
        return false;
    }
    matrix->allowed[first][second] = allowed;
    matrix->allowed[second][first] = allowed;
    return true;
}

bool ab_game_collision_setup(AbGameCollisionMatrix *matrix) {
    if (!matrix) {
        return false;
    }
    for (uint32_t first = 0; first <= AB_COLLIDER_PIG; ++first) {
        for (uint32_t second = 0; second <= AB_COLLIDER_PIG; ++second) {
            matrix->allowed[first][second] =
                !((first == AB_COLLIDER_EAGLE && second == AB_COLLIDER_STATIC) ||
                  (first == AB_COLLIDER_STATIC && second == AB_COLLIDER_EAGLE));
        }
        ab_game_collision_set(matrix, first, AB_COLLIDER_INVISIBLE, true);
    }
    for (uint32_t first = 0; first < AB_GAME_COLLIDER_TYPES; ++first) {
        ab_game_collision_set(matrix, first, AB_COLLIDER_GHOST,
                              first != AB_COLLIDER_BIRD && first != AB_COLLIDER_PIG);
    }
    for (uint32_t first = 0; first < AB_GAME_COLLIDER_TYPES; ++first) {
        ab_game_collision_set(matrix, first, AB_COLLIDER_EGG,
                              first == AB_COLLIDER_STATIC);
    }
    for (uint32_t first = 0; first < AB_COLLIDER_PORTAL; ++first) {
        ab_game_collision_set(matrix, first, AB_COLLIDER_PORTAL, true);
    }
    ab_game_collision_set(matrix, AB_COLLIDER_EAGLE, AB_COLLIDER_PORTAL, false);
    for (uint32_t first = 0; first < AB_COLLIDER_NONE; ++first) {
        ab_game_collision_set(matrix, first, AB_COLLIDER_NONE, false);
    }
    for (uint32_t first = 0; first < AB_GAME_COLLIDER_TYPES; ++first) {
        ab_game_collision_set(matrix, first, AB_COLLIDER_GLUE, true);
    }
    for (uint32_t first = 0; first < AB_GAME_COLLIDER_TYPES; ++first) {
        ab_game_collision_set(matrix, first, AB_COLLIDER_BUBBLE,
                              first != AB_COLLIDER_PIG && first != AB_COLLIDER_EGG);
    }
    return true;
}

bool ab_game_collision_initialize(AbGameCollisionMatrix *matrix) {
    if (!matrix) {
        return false;
    }
    for (size_t first = 0; first < AB_GAME_COLLIDER_TYPES; ++first) {
        for (size_t second = 0; second < AB_GAME_COLLIDER_TYPES; ++second) {
            matrix->allowed[first][second] = true;
        }
    }
    return ab_game_collision_setup(matrix);
}

bool ab_game_collision_allowed(AbCollisionFilter first, AbCollisionFilter second,
                               const AbGameCollisionObject *first_object,
                               const AbGameCollisionObject *second_object,
                               const AbGameCollisionMatrix *matrix) {
    if (!ab_fixture_filters_collide(first, second)) {
        return false;
    }
    if (first.group && first.group == second.group) {
        return true;
    }
    if (!first_object || !second_object) {
        return true;
    }
    return matrix && first_object->collider < AB_GAME_COLLIDER_TYPES &&
           second_object->collider < AB_GAME_COLLIDER_TYPES && first_object->enabled &&
           second_object->enabled &&
           matrix->allowed[first_object->collider][second_object->collider];
}
