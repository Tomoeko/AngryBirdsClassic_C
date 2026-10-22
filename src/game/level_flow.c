#include "ab/level_flow.h"

#include <math.h>

bool ab_level_check_complete(AbLevelCompletionState *state, AbLevelGoalQuery query,
                             void *context, bool *output, AbError *error) {
    if (!state || !output) {
        return ab_fail(error, "Missing level completion state or output");
    }
    if (state->prevent_ending ||
        (state->eagle_launched_literal_true && !state->eagle_timer_present)) {
        *output = false;
        return true;
    }
    if (state->eagle_launched_literal_true) {
        if (!isfinite(state->eagle_timer)) {
            return ab_fail(error, "Unsupported eagle completion timer");
        }
        if (state->eagle_timer > 0) {
            *output = false;
            return true;
        }
    }
    if (!query) {
        return ab_fail(error, "Missing level goal query");
    }
    bool goals_cleared;
    if (!query(context, &goals_cleared, error)) {
        return false;
    }
    *output =
        goals_cleared && (state->do_not_timeout || state->timed_out ||
                          !state->moving_objects || state->do_not_wait_literal_true);
    return true;
}

bool ab_level_check_goals(AbLevelGoals *state, AbLevelFlowHook notify, void *context,
                          bool *output, AbError *error) {
    if (!state || !output || (state->goal_count && !state->remaining_goals)) {
        return ab_fail(error, "Invalid level goal state");
    }
    if (state->disabled) {
        *output = false;
        return true;
    }
    for (size_t goal = 0; goal < state->goal_count; ++goal) {
        if (state->remaining_goals[goal]) {
            *output = false;
            return true;
        }
    }
    if (!state->notification_sent) {
        if (!notify) {
            return ab_fail(error, "Missing level goals cleared notification");
        }
        if (!notify(context, error)) {
            return false;
        }
        state->notification_sent = true;
    }
    *output = true;
    return true;
}

bool ab_level_before_ending(AbLevelBeforeEnding *state, float seconds,
                            AbLevelFlowHook hook, void *context, AbError *error) {
    if (!state || !isfinite(state->timer) || !isfinite(seconds)) {
        return ab_fail(error, "Invalid before-ending timer");
    }
    if (!state->script_called) {
        state->script_called = true;
        if (hook && !hook(context, error)) {
            return false;
        }
    }
    float next_timer = state->timer - seconds;
    if (!isfinite(next_timer)) {
        return ab_fail(error, "Unsupported before-ending timer range");
    }
    state->timer = next_timer;
    return true;
}
