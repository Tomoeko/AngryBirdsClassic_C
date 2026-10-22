#include "ab/level_complete.h"

#include <math.h>

bool ab_level_complete_begin(AbLevelCompleteState *state, AbLevelCompleteHost host,
                             void *context, AbError *error) {
    if (!state || !host) {
        return ab_fail(error, "Missing level completion state or host");
    }
    AbLevelCompleteCall call = {.operation = AB_LEVEL_COMPLETE_NOTIFY};
    if (!host(context, &call, error)) {
        return false;
    }
    call = (AbLevelCompleteCall){.operation = AB_LEVEL_COMPLETE_CANCEL_DRAG};
    if (!host(context, &call, error)) {
        return false;
    }
    state->completed = true;
    if (!isfinite(state->current_bird_index)) {
        return ab_fail(error, "Unsupported current bird index at completion");
    }
    state->remaining_bird_index = state->current_bird_index;
    if (!state->current_bird_present || state->current_bird_shot_literal_true) {
        if (!isfinite(state->birds_shot)) {
            return ab_fail(error, "Unsupported shot count at completion");
        }
        if (state->birds_shot > 0) {
            state->remaining_bird_index = state->remaining_bird_index + 1.0f;
            if (!isfinite(state->remaining_bird_index)) {
                return ab_fail(error, "Unsupported remaining bird index");
            }
        }
    }
    if (state->special_mode || state->disable_bird_score) {
        state->remaining_bird_index = 0;
    }
    call = (AbLevelCompleteCall){.operation = AB_LEVEL_COMPLETE_QUERY_BIRD,
                                 .bird_index = state->remaining_bird_index};
    if (!host(context, &call, error)) {
        return false;
    }
    if (call.bird_present) {
        state->bonus_timer = 3.5f;
        call = (AbLevelCompleteCall){.operation = AB_LEVEL_COMPLETE_CAMERA,
                                     .text = "LevelEndSequence"};
        if (!host(context, &call, error)) {
            return false;
        }
    }
    call = (AbLevelCompleteCall){.operation = AB_LEVEL_COMPLETE_RESOLVE_AUDIO,
                                 .text = state->clear_audio_group};
    if (!host(context, &call, error)) {
        return false;
    }
    call.operation = AB_LEVEL_COMPLETE_PLAY_AUDIO;
    if (!host(context, &call, error)) {
        return false;
    }
    state->ending_timer = state->eagle_launched ? 2.0f : 1.0f;
    if (state->has_level_hook) {
        call = (AbLevelCompleteCall){.operation = AB_LEVEL_COMPLETE_SCRIPT};
        if (!host(context, &call, error)) {
            return false;
        }
    }
    state->completing = true;
    return true;
}
