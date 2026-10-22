#include "ab/physics_separation.h"

#include "distance_internal.h"
#include "internal.h"

#include <float.h>
#include <math.h>

static bool make_transforms(const AbSeparation *state, float fraction,
                            AbRigidTransform *first, AbRigidTransform *second) {
    return state && state->first && state->second &&
           (unsigned)state->kind <= AB_SEPARATION_FACE_SECOND &&
           isfinite(state->axis.x) && isfinite(state->axis.y) &&
           (state->kind == AB_SEPARATION_POINTS ||
            (isfinite(state->local_point.x) && isfinite(state->local_point.y))) &&
           ab_sweep_transform(&state->first_sweep, fraction, first) &&
           ab_sweep_transform(&state->second_sweep, fraction, second) &&
           ab_distance_proxy_valid(*state->first, *first) &&
           ab_distance_proxy_valid(*state->second, *second);
}

static float normalize_axis(AbVec2 *axis) {
    float length = sqrtf(ab_distance_dot(*axis, *axis));
    if (length >= FLT_EPSILON) {
        float inverse = 1.0f / length;
        axis->x = axis->x * inverse;
        axis->y = axis->y * inverse;
    }
    return length;
}

static float initialize_face(AbSeparation *state, const AbDistanceCache *cache,
                             AbRigidTransform first_transform,
                             AbRigidTransform second_transform) {
    bool second_face = cache->first[0] == cache->first[1];
    const AbDistanceProxy *face = second_face ? state->second : state->first;
    const uint8_t *indices = second_face ? cache->second : cache->first;
    AbVec2 first = face->vertices[indices[0]];
    AbVec2 second = face->vertices[indices[1]];
    AbVec2 edge = ab_distance_subtract(second, first);
    state->kind = second_face ? AB_SEPARATION_FACE_SECOND : AB_SEPARATION_FACE_FIRST;
    state->axis = (AbVec2){edge.y, -edge.x};
    normalize_axis(&state->axis);
    state->local_point =
        (AbVec2){0.5f * (first.x + second.x), 0.5f * (first.y + second.y)};
    AbRigidTransform face_transform = second_face ? second_transform : first_transform;
    AbVec2 normal = ab_physics_rotate(state->axis, face_transform);
    AbVec2 face_point = ab_physics_point(state->local_point, face_transform);
    AbVec2 other_point =
        second_face
            ? ab_physics_point(state->first->vertices[cache->first[0]], first_transform)
            : ab_physics_point(state->second->vertices[cache->second[0]],
                               second_transform);
    float value =
        ab_distance_dot(ab_distance_subtract(other_point, face_point), normal);
    if (value < 0) {
        state->axis.x = -state->axis.x;
        state->axis.y = -state->axis.y;
        value = -value;
    }
    return value;
}

bool ab_separation_initialize(AbSeparation *output, const AbDistanceCache *cache,
                              const AbDistanceProxy *first, const AbSweep *first_sweep,
                              const AbDistanceProxy *second,
                              const AbSweep *second_sweep, float fraction,
                              float *separation) {
    if (!output || !cache || !first || !second || !first_sweep || !second_sweep ||
        !separation || cache->count < 1 || cache->count > 2) {
        return false;
    }
    AbSeparation result = {.first = first,
                           .second = second,
                           .first_sweep = *first_sweep,
                           .second_sweep = *second_sweep,
                           .local_point = output->local_point};
    AbRigidTransform first_transform, second_transform;
    if (!make_transforms(&result, fraction, &first_transform, &second_transform)) {
        return false;
    }
    for (size_t index = 0; index < cache->count; ++index) {
        if (cache->first[index] >= first->count ||
            cache->second[index] >= second->count) {
            return false;
        }
    }
    float value;
    if (cache->count == 1) {
        AbVec2 first_point =
            ab_physics_point(first->vertices[cache->first[0]], first_transform);
        AbVec2 second_point =
            ab_physics_point(second->vertices[cache->second[0]], second_transform);
        result.axis = ab_distance_subtract(second_point, first_point);
        value = normalize_axis(&result.axis);
    } else {
        value = initialize_face(&result, cache, first_transform, second_transform);
    }
    if (!isfinite(value) || !isfinite(result.axis.x) || !isfinite(result.axis.y)) {
        return false;
    }
    *output = result;
    *separation = value;
    return true;
}

static bool evaluate_transforms(const AbSeparation *state, int32_t first,
                                int32_t second, AbRigidTransform first_transform,
                                AbRigidTransform second_transform, float *output) {
    if ((state->kind != AB_SEPARATION_FACE_FIRST &&
         (first < 0 || (size_t)first >= state->first->count)) ||
        (state->kind != AB_SEPARATION_FACE_SECOND &&
         (second < 0 || (size_t)second >= state->second->count))) {
        return false;
    }
    AbVec2 normal, first_point, second_point;
    bool second_face = state->kind == AB_SEPARATION_FACE_SECOND;
    if (state->kind == AB_SEPARATION_POINTS) {
        normal = state->axis;
        first_point = ab_physics_point(state->first->vertices[first], first_transform);
        second_point =
            ab_physics_point(state->second->vertices[second], second_transform);
    } else if (second_face) {
        normal = ab_physics_rotate(state->axis, second_transform);
        first_point = ab_physics_point(state->first->vertices[first], first_transform);
        second_point = ab_physics_point(state->local_point, second_transform);
    } else {
        normal = ab_physics_rotate(state->axis, first_transform);
        first_point = ab_physics_point(state->local_point, first_transform);
        second_point =
            ab_physics_point(state->second->vertices[second], second_transform);
    }
    AbVec2 delta = second_face ? ab_distance_subtract(first_point, second_point)
                               : ab_distance_subtract(second_point, first_point);
    float value = ab_distance_dot(delta, normal);
    if (!isfinite(value)) {
        return false;
    }
    *output = value;
    return true;
}

bool ab_separation_evaluate(const AbSeparation *state, int32_t first, int32_t second,
                            float fraction, float *separation) {
    AbRigidTransform first_transform, second_transform;
    return separation &&
           make_transforms(state, fraction, &first_transform, &second_transform) &&
           evaluate_transforms(state, first, second, first_transform, second_transform,
                               separation);
}

bool ab_separation_find(const AbSeparation *state, float fraction, int32_t *first,
                        int32_t *second, float *separation) {
    AbRigidTransform first_transform, second_transform;
    if (!first || !second || !separation ||
        !make_transforms(state, fraction, &first_transform, &second_transform)) {
        return false;
    }
    int32_t first_index = -1, second_index = -1;
    AbVec2 normal = state->axis;
    if (state->kind == AB_SEPARATION_POINTS) {
        first_index = ab_distance_support_index(*state->first, first_transform, normal);
        second_index = ab_distance_support_index(*state->second, second_transform,
                                                 (AbVec2){-normal.x, -normal.y});
    } else if (state->kind == AB_SEPARATION_FACE_FIRST) {
        normal = ab_physics_rotate(normal, first_transform);
        second_index = ab_distance_support_index(*state->second, second_transform,
                                                 (AbVec2){-normal.x, -normal.y});
    } else {
        normal = ab_physics_rotate(normal, second_transform);
        first_index = ab_distance_support_index(*state->first, first_transform,
                                                (AbVec2){-normal.x, -normal.y});
    }
    float value;
    if (!evaluate_transforms(state, first_index, second_index, first_transform,
                             second_transform, &value)) {
        return false;
    }
    *first = first_index;
    *second = second_index;
    *separation = value;
    return true;
}
