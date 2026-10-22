#include "ab/score.h"

#include <math.h>

bool ab_break_score(const AbBreakScoreInput *input, AbScoreNumber *output) {
    if (!input || !output) {
        return false;
    }
    AbScoreNumber result = input->merged_override_present
                               ? input->base_override
                               : (AbScoreNumber){.value = input->level_goal_truthy
                                                              ? input->goal_increment
                                                              : input->block_increment,
                                                 .present = true};
    if (!result.present) {
        result.value = 0;
    } else if (!isfinite(result.value)) {
        return false;
    }
    *output = result;
    return true;
}

bool ab_damage_score(float damage, float *output) {
    if (!output || !isfinite(damage)) {
        return false;
    }
    float result = floorf(damage) * 10.0f;
    if (!isfinite(result)) {
        return false;
    }
    *output = result;
    return true;
}
