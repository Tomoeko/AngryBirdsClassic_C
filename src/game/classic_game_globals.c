#include "ab/classic_game_globals.h"

#include <math.h>
#include <string.h>

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Unsupported shared gameplay global number");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite shared gameplay global");
}

static bool equal(AbDataValue left, AbDataValue right) {
    if (left.kind != right.kind)
        return false;
    if (left.kind == AB_DATA_NUMBER) {
        float first, second;
        memcpy(&first, &left.bits, sizeof(first));
        memcpy(&second, &right.bits, sizeof(second));
        return first == second;
    }
    if (left.kind == AB_DATA_NIL)
        return true;
    if (left.kind == AB_DATA_BOOLEAN)
        return !!left.bits == !!right.bits;
    if (left.kind == AB_DATA_TABLE)
        return left.table == right.table;
    return left.string.size == right.string.size &&
           (!left.string.size ||
            (left.string.data && right.string.data &&
             !memcmp(left.string.data, right.string.data, left.string.size)));
}

static bool add(AbDataValue value, float seconds, AbDataValue *output, AbError *error) {
    float prior;
    if (!numeric(value, &prior, error))
        return false;
    float sum = prior + seconds;
    if (!isfinite(seconds) || !isfinite(sum))
        return ab_fail(error, "Unsupported gameplay clock arithmetic");
    *output = number(sum);
    return true;
}

bool ab_classic_game_globals_advance_time(AbClassicGameGlobals *globals, float seconds,
                                          AbError *error) {
    if (!globals)
        return ab_fail(error, "Missing shared gameplay globals");
    if (!truthy(globals->time)) {
        globals->time = number(0);
        return true;
    }
    return add(globals->time, seconds, &globals->time, error);
}

bool ab_classic_game_globals_level_time(AbClassicGameGlobals *globals, float seconds,
                                        AbError *error) {
    if (!globals)
        return ab_fail(error, "Missing shared gameplay globals");
    AbDataValue prior = truthy(globals->time_spent) ? globals->time_spent : number(0);
    return add(prior, seconds, &globals->time_spent, error);
}

bool ab_classic_game_globals_timeout(AbClassicGameGlobals *globals,
                                     AbDataValue do_not_timeout, float seconds,
                                     AbDataValue time, AbError *error) {
    if (!globals)
        return ab_fail(error, "Missing shared gameplay globals");
    globals->level_timeout = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    if (truthy(do_not_timeout) || time.kind == AB_DATA_NIL ||
        equal(globals->last_score_time, number(0)))
        return true;
    if (!truthy(globals->last_score_timer))
        globals->last_score_timer = time;
    else if (!add(globals->last_score_timer, seconds, &globals->last_score_timer,
                  error))
        return false;
    if (!equal(globals->old_score, globals->score)) {
        globals->old_score = globals->score;
        globals->last_score_time = time;
        globals->last_score_timer = time;
    }
    float timer, last;
    if (!numeric(globals->last_score_timer, &timer, error) ||
        !numeric(globals->last_score_time, &last, error))
        return false;
    float elapsed = timer - last;
    if (!isfinite(elapsed))
        return ab_fail(error, "Unsupported gameplay timeout subtraction");
    if (elapsed >= 15)
        globals->level_timeout.bits = 1;
    return true;
}

bool ab_classic_game_globals_refresh_shot(const AbClassicGameGlobals *globals,
                                          AbShotState *shot, AbError *error) {
    if (!globals || !shot)
        return ab_fail(error, "Missing shared shot globals");
    float time, score, last, old, tutorial;
    if (!numeric(globals->time, &time, error) ||
        !numeric(globals->score, &score, error) ||
        !numeric(globals->last_score_time, &last, error) ||
        !numeric(globals->old_score, &old, error) ||
        !numeric(globals->extra_tutorial_timer, &tutorial, error))
        return false;
    shot->time = time;
    shot->score = score;
    shot->last_score_time = last;
    shot->old_score = old;
    shot->tutorial_timer = tutorial;
    shot->specialty_available = truthy(globals->specialty_available);
    return true;
}

bool ab_classic_game_globals_publish_shot(AbClassicGameGlobals *globals,
                                          const AbShotState *shot, AbError *error) {
    if (!globals || !shot || !isfinite(shot->last_score_time) ||
        !isfinite(shot->old_score))
        return ab_fail(error, "Unsupported shared shot score publication");
    globals->last_score_time = number(shot->last_score_time);
    globals->old_score = number(shot->old_score);
    return true;
}

bool ab_classic_game_globals_publish_score(AbClassicGameGlobals *globals, float score,
                                           AbError *error) {
    if (!globals || !isfinite(score))
        return ab_fail(error, "Unsupported shared root score publication");
    globals->score = number(score);
    return true;
}
