#include "ab/level_flow.h"

#include <math.h>

bool ab_level_ending_delay(const AbLevelEndingDelay *state, float seconds,
                           bool completed, bool win_condition,
                           AbLevelEndingGate *output) {
    if (!state || !output || !isfinite(state->timer)) {
        return false;
    }
    AbLevelEndingGate result = {.next = *state};
    if (state->movement_delay_present) {
        if (!isfinite(state->movement_delay) || !isfinite(seconds)) {
            return false;
        }
        result.next.movement_delay = state->movement_delay - seconds;
        if (!isfinite(result.next.movement_delay)) {
            return false;
        }
        if (result.next.movement_delay <= 0) {
            result.next.do_not_wait_literal_true = true;
            result.next.movement_delay_present = false;
        }
    }
    if (result.next.do_not_wait_literal_true && !state->delay_set) {
        if (!isfinite(state->default_delay)) {
            return false;
        }
        result.next.delay_set = true;
        result.next.timer = state->default_delay;
    }
    if (state->moving_objects && !result.next.do_not_wait_literal_true &&
        result.next.timer < 1.0f) {
        if (!isfinite(state->extensions)) {
            return false;
        }
        if (state->extensions < 3.0f) {
            result.next.timer = 2.0f;
            result.next.extensions = state->extensions + 1.0f;
            if (!isfinite(result.next.extensions)) {
                return false;
            }
        }
    }
    if (result.next.timer <= 0) {
        result.next.timed_out = true;
        result.query_win_condition = !completed;
        result.queue_level_ended = true;
        result.level_complete = completed || win_condition;
    }
    *output = result;
    return true;
}
