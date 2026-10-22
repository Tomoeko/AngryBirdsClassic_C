#include "ab/physics_impact.h"

#include "distance_internal.h"

#include <math.h>

typedef struct {
    AbImpactInput input;
    AbImpactResult result;
    AbDistanceCache cache;
    float target;
    float tolerance;
} ImpactSearch;

typedef struct {
    float lower_time;
    float upper_time;
    float lower_separation;
    float upper_separation;
    int32_t first;
    int32_t second;
} ImpactBracket;

static bool refine_root(ImpactSearch *search, const AbSeparation *separation,
                        ImpactBracket bracket, float *upper_time) {
    for (size_t iteration = 0; iteration < 50; ++iteration) {
        float time;
        if (iteration % 2) {
            float numerator = (search->target - bracket.lower_separation) *
                              (bracket.upper_time - bracket.lower_time);
            time = bracket.lower_time +
                   numerator / (bracket.upper_separation - bracket.lower_separation);
        } else {
            time = 0.5f * (bracket.lower_time + bracket.upper_time);
        }
        float value;
        if (!ab_separation_evaluate(separation, bracket.first, bracket.second, time,
                                    &value)) {
            return false;
        }
        ++search->result.root_iterations;
        if (fabsf(value - search->target) < search->tolerance) {
            *upper_time = time;
            return true;
        }
        if (value > search->target) {
            bracket.lower_time = time;
            bracket.lower_separation = value;
        } else {
            bracket.upper_time = time;
            bracket.upper_separation = value;
        }
    }
    return true;
}

static bool advance_separation(ImpactSearch *search, const AbSeparation *separation,
                               float *start_time) {
    float end_time = search->input.maximum_fraction;
    for (size_t push = 0; push < AB_POLYGON_VERTICES; ++push) {
        int32_t first, second;
        float end_separation;
        if (!ab_separation_find(separation, end_time, &first, &second,
                                &end_separation)) {
            return false;
        }
        if (end_separation > search->target + search->tolerance) {
            search->result.kind = AB_IMPACT_SEPARATED;
            search->result.fraction = search->input.maximum_fraction;
            return true;
        }
        if (end_separation > search->target - search->tolerance) {
            *start_time = end_time;
            return true;
        }
        float start_separation;
        if (!ab_separation_evaluate(separation, first, second, *start_time,
                                    &start_separation)) {
            return false;
        }
        if (start_separation < search->target - search->tolerance) {
            search->result.kind = AB_IMPACT_FAILED;
            search->result.fraction = *start_time;
            return true;
        }
        if (start_separation <= search->target + search->tolerance) {
            search->result.kind = AB_IMPACT_TOUCHING;
            search->result.fraction = *start_time;
            return true;
        }
        ImpactBracket bracket = {*start_time,    end_time, start_separation,
                                 end_separation, first,    second};
        if (!refine_root(search, separation, bracket, &end_time)) {
            return false;
        }
    }
    return true;
}

bool ab_time_of_impact(const AbImpactInput *input, AbImpactResult *output) {
    AbRigidTransform identity = {{0, 0}, 0, 1};
    if (!input || !output || !isfinite(input->maximum_fraction) ||
        input->maximum_fraction < 0 || input->maximum_fraction > 1 ||
        !ab_distance_proxy_valid(input->first, identity) ||
        !ab_distance_proxy_valid(input->second, identity)) {
        return false;
    }
    ImpactSearch search = {.input = *input,
                           .result = {.fraction = input->maximum_fraction},
                           .tolerance = 0x1.99999ap-7f};
    if (!ab_sweep_normalize(&search.input.first_sweep) ||
        !ab_sweep_normalize(&search.input.second_sweep)) {
        return false;
    }
    float radius = input->first.radius + input->second.radius;
    if (!isfinite(radius)) {
        return false;
    }
    float target = radius - 0x1.333334p-3f;
    search.target = target > 0x1.99999ap-5f ? target : 0x1.99999ap-5f;
    float time = 0;
    while (search.result.iterations < 20) {
        AbRigidTransform first_transform, second_transform;
        AbDistanceResult distance;
        if (!ab_sweep_transform(&search.input.first_sweep, time, &first_transform) ||
            !ab_sweep_transform(&search.input.second_sweep, time, &second_transform) ||
            !ab_shape_distance(input->first, first_transform, input->second,
                               second_transform, false, &search.cache, &distance)) {
            return false;
        }
        if (distance.distance <= 0) {
            search.result.kind = AB_IMPACT_OVERLAPPED;
            search.result.fraction = 0;
            break;
        }
        if (distance.distance < search.target + search.tolerance) {
            search.result.kind = AB_IMPACT_TOUCHING;
            search.result.fraction = time;
            break;
        }
        AbSeparation separation = {0};
        float initial_separation;
        if (!ab_separation_initialize(&separation, &search.cache, &search.input.first,
                                      &search.input.first_sweep, &search.input.second,
                                      &search.input.second_sweep, time,
                                      &initial_separation) ||
            !advance_separation(&search, &separation, &time)) {
            return false;
        }
        ++search.result.iterations;
        if (search.result.kind != AB_IMPACT_UNKNOWN) {
            break;
        }
    }
    if (search.result.kind == AB_IMPACT_UNKNOWN) {
        search.result.kind = AB_IMPACT_FAILED;
        search.result.fraction = time;
    }
    *output = search.result;
    return true;
}
