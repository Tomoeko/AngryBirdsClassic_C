#ifndef AB_LEVEL_FLOW_H
#define AB_LEVEL_FLOW_H

#include "ab/base.h"

typedef struct {
    float eagle_timer;
    bool prevent_ending;
    bool eagle_launched_literal_true;
    bool eagle_timer_present;
    bool do_not_timeout;
    bool timed_out;
    bool moving_objects;
    bool do_not_wait_literal_true;
} AbLevelCompletionState;

typedef bool (*AbLevelGoalQuery)(void *context, bool *cleared, AbError *error);

/* The goal query may change state. The movement/timeout gate reads state after
 * that call; the prevention/eagle prefix is not repeated. Boolean fields encode
 * Lua truthiness except the two explicitly named literal-true fields. A missing
 * eagle timer blocks completion only when eagle_launched_literal_true is set.
 * No callback is needed when the prefix already blocks completion. A callback
 * failure preserves output and leaves its own state changes applied. */
bool ab_level_check_complete(AbLevelCompletionState *state, AbLevelGoalQuery query,
                             void *context, bool *output, AbError *error);

typedef struct {
    const bool *remaining_goals;
    size_t goal_count;
    bool disabled;
    bool notification_sent;
} AbLevelGoals;

typedef bool (*AbLevelFlowHook)(void *context, AbError *error);

/* The borrowed goal flags represent each entry's levelGoal truthiness. Notify
 * runs once when no goal remains and checking is enabled. notification_sent is
 * set only after the notification returns successfully. The callback may change
 * goals/state; the current check does not scan them again. */
bool ab_level_check_goals(AbLevelGoals *state, AbLevelFlowHook notify, void *context,
                          bool *output, AbError *error);

typedef struct {
    float timer;
    bool script_called;
} AbLevelBeforeEnding;

/* Set script_called before the optional, argument-free level hook. Subtract
 * seconds on every successful call, including later calls after the hook ran.
 * The timer is read after the hook. On host failure preceding changes remain;
 * output arithmetic outside the finite supported domain returns an error. */
bool ab_level_before_ending(AbLevelBeforeEnding *state, float seconds,
                            AbLevelFlowHook hook, void *context, AbError *error);

typedef struct {
    float timer;
    float bird_index;
    float bird_score;
    float score_increment;
    bool skip_ending;
    bool completed;
    bool eagle_launched_literal_true;
} AbLevelBonusState;

typedef struct {
    AbLevelBonusState next;
    float queried_bird_index;
    bool clear_eagle_darkness;
    bool query_bird;
    bool append_score;
    bool present_bird_effects;
    bool before_ending;
    bool update_score;
} AbLevelBonusGate;

/* Numeric remaining-bird bonus prefix of updateLevelEnding. Resolve the next
 * bird's non-nil presence at queried_bird_index; false is still present. Completed
 * updates clear eagle darkness and subtract seconds, but query only for a
 * strictly negative resulting timer. A present bird advances the index, awards
 * an increment unless eagle is literal true, requests effects and resets the
 * timer to 0.5. A missing bird requests before-ending without resetting it.
 * This plans callbacks with nonmutating host boundaries; their execution,
 * total-score aggregation and the later ending-delay/event logic are separate.
 * Used fields/results must be finite. Failure leaves output unchanged. */
bool ab_level_bonus_gate(const AbLevelBonusState *state, float seconds,
                         bool next_bird_present, AbLevelBonusGate *output);

typedef struct {
    float timer;
    float movement_delay;
    float default_delay;
    float extensions;
    bool movement_delay_present;
    bool do_not_wait_literal_true;
    bool delay_set;
    bool moving_objects;
    bool timed_out;
} AbLevelEndingDelay;

typedef struct {
    AbLevelEndingDelay next;
    bool query_win_condition;
    bool queue_level_ended;
    bool level_complete;
} AbLevelEndingGate;

/* updateLevelEnding's delay/event suffix, after its bonus or ordinary timer
 * prefix. The caller already subtracts seconds from timer for an incomplete
 * level, or invokes before-ending when no bonus bird remains. A present movement
 * delay counts down and, at <= zero, sets do_not_wait_literal_true and becomes
 * absent. First use of that flag selects default_delay. Moving objects can
 * extend a timer strictly below one to two, at most three times. A timer <= zero
 * sets timed_out, then queries the win condition only if not already completed,
 * and requests a level-ended event. Repeated calls can request repeated events.
 * completed/win_condition are resolved truthiness; original payload type and
 * host mutations are outside this numeric/boolean plan. Call only when the
 * skip-ending prefix permits work. Used numeric values/results must be finite;
 * failure preserves output. */
bool ab_level_ending_delay(const AbLevelEndingDelay *state, float seconds,
                           bool completed, bool win_condition,
                           AbLevelEndingGate *output);

#endif
