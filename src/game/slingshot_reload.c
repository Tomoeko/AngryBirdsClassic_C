#include "ab/slingshot.h"

#include <math.h>

bool ab_slingshot_reload_gate(const AbSlingReloadState *state, float seconds,
                              bool next_bird_present, AbSlingReloadGate *output) {
    if (!state || !output || !isfinite(state->timer) || !isfinite(seconds) ||
        !isfinite(state->current_bird_index)) {
        return false;
    }
    AbSlingReloadGate result = {.next = *state};
    if (state->timer > 0) {
        result.next.timer = state->timer - seconds;
        if (!isfinite(result.next.timer)) {
            return false;
        }
        if (result.next.timer <= 0) {
            result.query_next_bird = true;
            result.queried_bird_index = state->current_bird_index + 1.0f;
            if (!isfinite(result.queried_bird_index)) {
                return false;
            }
            if (!next_bird_present) {
                result.next.allow_reset_to_bird_camera = false;
            } else if (state->bird_fired) {
                result.next.bird_fired = false;
                result.next.fill_in_next_bird = true;
                result.clear_launch_cue = true;
            }
        }
    }
    *output = result;
    return true;
}
