#include "ab/bird_impact_prediction.h"
#include "script_float_internal.h"

static float add(float first, float second) {
    return script_float_binary(first, second, AB_SCRIPT_FLOAT_ADD);
}

static float multiply(float first, float second) {
    return script_float_binary(first, second, AB_SCRIPT_FLOAT_MULTIPLY);
}

static float divide(float first, float second) {
    return script_float_binary(first, second, AB_SCRIPT_FLOAT_DIVIDE);
}

static AbVec2 velocity_at(const AbBirdImpactInput *input, float time) {
    return (AbVec2){input->velocity.x,
                    add(input->velocity.y, multiply(time, input->gravity))};
}

static AbVec2 position_at(const AbBirdImpactInput *input, AbVec2 origin, float time) {
    AbVec2 velocity = velocity_at(input, time);
    AbVec2 normal = {-velocity.y, velocity.x};
    float squared = add(multiply(normal.x, normal.x), multiply(normal.y, normal.y));
    float length = (float)sqrt((double)squared);
    normal.x = divide(normal.x, length);
    normal.y = divide(normal.y, length);
    AbVec2 position = {add(origin.x, normal.x), add(origin.y, normal.y)};
    position.x = add(position.x, multiply(time, input->velocity.x));
    position.y = add(position.y, multiply(time, input->velocity.y));
    float acceleration = multiply(0.5f, multiply(time, time));
    position.x = add(position.x, multiply(acceleration, 0));
    position.y = add(position.y, multiply(acceleration, input->gravity));
    return position;
}

static bool cast(AbBirdImpactInput *input, AbVec2 origin, float time, float interval,
                 AbBirdImpactHost *host, AbBirdImpactPrediction *result,
                 AbBirdImpactRayHit *hit, AbError *error) {
    if (result->raycasts >= host->maximum_raycasts) {
        return ab_fail(error, "Bird impact prediction exceeds its query budget");
    }
    AbVec2 first = position_at(input, origin, time);
    AbVec2 velocity = velocity_at(input, time);
    AbVec2 second = {add(first.x, multiply(velocity.x, interval)),
                     add(first.y, multiply(velocity.y, interval))};
    ++result->raycasts;
    if (!host->raycast) {
        return ab_fail(error, "Missing bird impact raycast host");
    }
    *hit = (AbBirdImpactRayHit){0};
    return host->raycast(host->context, first, second, hit, error);
}

bool ab_bird_impact_predict(AbBirdImpactInput *input, AbBirdImpactHost *host,
                            AbBirdImpactPrediction *output, AbError *error) {
    if (!input || !host || !output || host->maximum_raycasts > UINT32_C(16777216)) {
        return ab_fail(error, "Invalid bird impact prediction input or budget");
    }
    AbVec2 origin = input->position;
    float steps = input->steps_present ? input->steps : 10.0f;
    float interval = divide(1, steps);
    float minimum = divide(1, multiply(steps, 8));
    AbBirdImpactPrediction result = {0};
    size_t misses = 0;
    while (minimum <= interval && misses < 200) {
        ++misses;
        AbBirdImpactRayHit rays[3] = {0};
        if (!cast(input, origin, result.time, interval, host, &result, rays, error)) {
            return false;
        }
        for (size_t index = 1; index < 3; ++index) {
            float radius = 0;
            if (!host->radius) {
                return ab_fail(error, "Missing bird impact radius property host");
            }
            if (!host->radius(host->context, &radius, error)) {
                return false;
            }
            /* The original computes +/-radius*2.1, then passes it to a helper
             * that never reads that argument. Keep both property boundaries. */
            (void)radius;
            if (!cast(input, origin, result.time, interval, host, &result, rays + index,
                      error)) {
                return false;
            }
        }
        const AbBirdImpactRayHit *selected = NULL;
        for (size_t index = 0; index < 3; ++index) {
            if (rays[index].hit) {
                selected = rays + index;
                break;
            }
        }
        if (!selected) {
            result.time = add(result.time, interval);
            continue;
        }
        result.point = selected->point;
        result.name = selected->name;
        result.point_present = true;
        interval = divide(interval, 2);
        misses = 0;
    }
    if (!host->print) {
        return ab_fail(error, "Missing bird impact query-count print host");
    }
    if (!host->print(host->context, result.raycasts, error)) {
        return false;
    }
    *output = result;
    return true;
}
