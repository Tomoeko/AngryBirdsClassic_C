#include "ab/frame_clock.h"

#include <math.h>

float ab_app_frame_seconds(float seconds) {
    return seconds < 0.1f ? seconds : 0.1f;
}

bool ab_frame_clock_advance(const AbFrameClock *clock, const AbFrameClockInput *input,
                            uint32_t step_limit, AbFrameClockPlan *output) {
    if (!clock || !input || !output || step_limit > 4096 || !isfinite(clock->time) ||
        !isfinite(clock->accumulator) || !isfinite(input->seconds)) {
        return false;
    }
    AbFrameClockPlan plan = {.next = *clock};
    plan.next.time = clock->time + input->seconds;
    if (!isfinite(plan.next.time)) {
        return false;
    }
    if (!input->physics_enabled) {
        *output = plan;
        return true;
    }
    if (input->speed_factor_present && !isfinite(input->speed_factor)) {
        return false;
    }
    plan.physics_seconds = input->speed_factor_present
                               ? input->seconds * input->speed_factor
                               : input->seconds;
    plan.next.accumulator = plan.physics_seconds + clock->accumulator;
    if (!isfinite(plan.physics_seconds) || !isfinite(plan.next.accumulator)) {
        return false;
    }
    const float timestep = 0x1.111112p-5f;
    const float history_threshold = 0x1.111112p-4f;
    while (plan.next.accumulator >= timestep) {
        if (plan.steps == step_limit) {
            return false;
        }
        float remaining = plan.next.accumulator - timestep;
        if (remaining == plan.next.accumulator) {
            return false;
        }
        plan.next.accumulator = remaining;
        ++plan.steps;
        if (remaining <= history_threshold) {
            ++plan.history_updates;
        }
    }
    plan.interpolation = plan.next.accumulator / timestep;
    if (!isfinite(plan.interpolation)) {
        return false;
    }
    plan.interpolation_present = true;
    *output = plan;
    return true;
}
