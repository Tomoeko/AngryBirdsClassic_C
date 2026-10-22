#include "ab/bird_retirement.h"

#include <math.h>

bool ab_bird_retirement_frame(const AbBirdRetirementState *state,
                              const AbBirdRetirementInput *input,
                              AbBirdRetirementFrame *output) {
    if (!state || !input || !output) {
        return false;
    }
    AbBirdRetirementFrame result = {.next = *state};
    if (!input->controllable || input->mighty_bait || input->mighty_eagle ||
        !input->shot_literal_true) {
        *output = result;
        return true;
    }
    result.query_definition = true;
    if (input->manually_removed) {
        *output = result;
        return true;
    }
    if (!isfinite(input->seconds) ||
        (state->water_timer_present && !isfinite(state->water_timer))) {
        return false;
    }
    if (state->ready_timer_present) {
        if (!isfinite(state->ready_timer) || !isfinite(input->speed_squared) ||
            !isfinite(input->speed_threshold)) {
            return false;
        }
        result.query_speed_squared = true;
    }
    if (state->ready_timer_present && input->speed_squared < input->speed_threshold) {
        result.next.ready_timer = state->ready_timer - input->seconds;
    } else {
        result.next.ready_timer = input->reset_timer;
    }
    if (!isfinite(result.next.ready_timer)) {
        return false;
    }
    result.next.ready_timer_present = true;
    if (input->water_active) {
        float water_timer = state->water_timer_present ? state->water_timer : 10.0f;
        if (!isfinite(water_timer)) {
            return false;
        }
        result.next.water_timer = water_timer - input->seconds;
        result.next.water_timer_present = true;
        if (!isfinite(result.next.water_timer)) {
            return false;
        }
    }
    if (input->outside_boundaries) {
        result.remove_bird = true;
    } else if (result.next.ready_timer < 0 ||
               (result.next.water_timer_present && result.next.water_timer < 0)) {
        result.query_camera_target = true;
        result.remove_bird = !input->camera_targets_bird;
    }
    *output = result;
    return true;
}
