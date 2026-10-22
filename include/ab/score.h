#ifndef AB_SCORE_H
#define AB_SCORE_H

#include <stdbool.h>

typedef struct {
    float value;
    bool present;
} AbScoreNumber;

typedef struct {
    float block_increment;
    float goal_increment;
    AbScoreNumber base_override;
    bool merged_override_present;
    bool level_goal_truthy;
} AbBreakScoreInput;

/* Classic getScoreToAddForBreaking. A non-nil merged-definition override
 * selects the base definition's value, even if that base value is absent. The
 * merged value itself is not returned. Without an override, a truthy levelGoal
 * selects the goal increment; otherwise select the ordinary block increment.
 * Caller resolves definition lookup, presence and Lua truthiness. Used numeric
 * values must be finite; absent output has value positive zero. Failure leaves
 * output unchanged. Scoring side effects and other modes are separate.
 */
bool ab_break_score(const AbBreakScoreInput *input, AbScoreNumber *output);

/* Classic getScoreToAddForDamaging: floor the binary32 input, then multiply by
 * binary32 ten. Preserve negative zero. Input and output must be finite;
 * unsupported range leaves output unchanged. */
bool ab_damage_score(float damage, float *output);

#endif
