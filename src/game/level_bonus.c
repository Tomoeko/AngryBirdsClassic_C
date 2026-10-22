#include "ab/level_flow.h"

#include <math.h>

bool ab_level_bonus_gate(const AbLevelBonusState *state, float seconds,
                         bool next_bird_present, AbLevelBonusGate *output) {
    if (!state || !output) {
        return false;
    }
    AbLevelBonusGate result = {.next = *state};
    if (state->skip_ending || !state->completed) {
        *output = result;
        return true;
    }
    if (!isfinite(state->timer) || !isfinite(seconds)) {
        return false;
    }
    result.clear_eagle_darkness = true;
    result.next.timer = state->timer - seconds;
    if (!isfinite(result.next.timer)) {
        return false;
    }
    if (result.next.timer < 0) {
        if (!isfinite(state->bird_index)) {
            return false;
        }
        result.query_bird = true;
        result.queried_bird_index = state->bird_index;
        if (next_bird_present) {
            result.next.bird_index = state->bird_index + 1.0f;
            if (!isfinite(result.next.bird_index)) {
                return false;
            }
            if (!state->eagle_launched_literal_true) {
                result.next.bird_score = state->bird_score + state->score_increment;
                if (!isfinite(state->bird_score) || !isfinite(state->score_increment) ||
                    !isfinite(result.next.bird_score)) {
                    return false;
                }
                result.append_score = true;
            }
            result.present_bird_effects = true;
            result.next.timer = 0.5f;
        } else {
            result.before_ending = true;
        }
        result.update_score = true;
    }
    *output = result;
    return true;
}
