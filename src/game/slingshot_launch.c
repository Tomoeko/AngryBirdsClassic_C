#include "ab/slingshot.h"
#include "ab/slingshot_launch.h"

#include <math.h>

bool ab_slingshot_release_ready(float distance, float threshold) {
    return threshold < distance;
}

bool ab_slingshot_counts_backwards(float vector_x, bool eagle_bait_is_true,
                                   bool special_mode_truthy) {
    return !eagle_bait_is_true && !special_mode_truthy && vector_x < 0;
}

static bool calculate_impulse(AbVec2 vector, float distance, float force, float stretch,
                              AbVec2 *output) {
    if (!isfinite(vector.x) || !isfinite(vector.y) || !isfinite(distance) ||
        distance <= 0 || !isfinite(force) || !isfinite(stretch))
        return false;
    AbVec2 impulse = {vector.x / distance, vector.y / distance};
    impulse.x = impulse.x * force;
    impulse.x = impulse.x * stretch;
    impulse.y = impulse.y * force;
    impulse.y = impulse.y * stretch;
    if (!isfinite(impulse.x) || !isfinite(impulse.y))
        return false;
    *output = impulse;
    return true;
}

bool ab_slingshot_launch_impulse(const AbSlingLaunchInput *input,
                                 const AbVec2 *vector_to_rest, AbVec2 *output) {
    if (!input || !output || !input->bird_present || !isfinite(input->rest.x) ||
        !isfinite(input->rest.y) || !isfinite(input->rubber_band.x) ||
        !isfinite(input->rubber_band.y) || !isfinite(input->stretch) ||
        !isfinite(input->launch_force) || !isfinite(input->physics_scale) ||
        !isfinite(input->bird_mass) || input->bird_mass <= 0 ||
        !isfinite(input->slingshot_power)) {
        return false;
    }
    AbVec2 vector = vector_to_rest ? *vector_to_rest
                                   : (AbVec2){input->rest.x - input->rubber_band.x,
                                              input->rest.y - input->rubber_band.y};
    if (!isfinite(vector.x) || !isfinite(vector.y)) {
        return false;
    }
    float squared = vector.x * vector.x + vector.y * vector.y;
    if (!isfinite(squared)) {
        return false;
    }
    float distance = sqrtf(squared);
    float force = -input->launch_force;
    force = force * input->physics_scale;
    force = force * input->bird_mass;
    force = force * input->slingshot_power;
    return calculate_impulse(vector, distance, force, input->stretch, output);
}

static bool launch_number(AbSlingLaunchHost host, void *context,
                          AbSlingLaunchOperation operation, float *output,
                          AbError *error) {
    AbSlingLaunchCall call = {.operation = operation};
    if (!host(context, &call, error))
        return false;
    if (!isfinite(call.number))
        return ab_fail(error, "Unsupported slingshot numeric query result");
    *output = call.number;
    return true;
}

bool ab_slingshot_launch(AbSlingLaunchState *state, AbVec2 *direction,
                         AbSlingLaunchHost host, void *context, AbError *error) {
    if (!state || !host)
        return ab_fail(error, "Missing slingshot launch state or host");
    float distance = 0;
    AbSlingLaunchCall call;
    if (direction) {
        float squared = direction->x * direction->x + direction->y * direction->y;
        if (!isfinite(direction->x) || !isfinite(direction->y) || !isfinite(squared))
            return ab_fail(error, "Unsupported explicit slingshot direction");
        distance = (float)sqrt((double)squared);
    } else {
        call = (AbSlingLaunchCall){.operation = AB_SLING_LAUNCH_DIRECTION};
        if (!host(context, &call, error))
            return false;
        direction = call.direction;
        if (!launch_number(host, context, AB_SLING_LAUNCH_DISTANCE, &distance, error))
            return false;
    }
    if (!state->eagle_bait_is_true && !state->special_mode_truthy) {
        if (!direction || !isfinite(direction->x))
            return ab_fail(error, "Missing slingshot backwards-statistic direction");
        if (direction->x < 0) {
            call = (AbSlingLaunchCall){.operation = AB_SLING_LAUNCH_BACKWARDS};
            if (!host(context, &call, error))
                return false;
        }
    }
    call = (AbSlingLaunchCall){.operation = AB_SLING_LAUNCH_SHOT_COUNT};
    if (!host(context, &call, error))
        return false;
    float stretch = 0;
    if (!launch_number(host, context, AB_SLING_LAUNCH_STRETCH, &stretch, error))
        return false;
    if (!state->dragged || !state->dragged->name)
        return ab_fail(error, "Missing dragged bird at slingshot force lookup");
    call = (AbSlingLaunchCall){.operation = AB_SLING_LAUNCH_FORCE,
                               .name = state->dragged->name};
    if (!host(context, &call, error))
        return false;
    float force = call.force_present ? call.number : state->default_force;
    if (!state->dragged || !isfinite(state->dragged->mass) ||
        state->dragged->mass <= 0 || !isfinite(force) ||
        !isfinite(state->physics_scale))
        return ab_fail(error, "Unsupported slingshot force, scale or bird mass");
    force = -force;
    force = force * state->physics_scale;
    force = force * state->dragged->mass;
    float power = 0;
    if (!launch_number(host, context, AB_SLING_LAUNCH_POWER, &power, error))
        return false;
    force = force * power;
    AbVec2 impulse;
    if (!direction ||
        !calculate_impulse(*direction, distance, force, stretch, &impulse))
        return ab_fail(error, "Unsupported slingshot impulse result");
    call = (AbSlingLaunchCall){.operation = AB_SLING_LAUNCH_POSITION};
    if (!host(context, &call, error))
        return false;
    if (!call.position)
        return ab_fail(error, "Missing slingshot launch position vector");
    call = (AbSlingLaunchCall){.operation = AB_SLING_LAUNCH_WITH_IMPULSE,
                               .position = call.position,
                               .impulse = &impulse};
    return host(context, &call, error);
}

bool ab_slingshot_release(AbSlingReleaseHost host, void *context, AbError *error) {
    if (!host)
        return ab_fail(error, "Missing slingshot release host");
    AbSlingReleaseCall call = {.operation = AB_SLING_RELEASE_CHECK};
    if (!host(context, &call, error))
        return false;
    call.operation = call.stretched ? AB_SLING_RELEASE_LAUNCH : AB_SLING_RELEASE_CANCEL;
    return host(context, &call, error);
}
