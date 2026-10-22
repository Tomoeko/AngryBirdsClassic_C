#include "ab/hero_screen_tinter.h"

#include <math.h>
#include <stdio.h>

typedef struct {
    AbHeroScreenTinter *state;
    AbHeroTintTween *replacement;
    size_t updates, values;
    bool replace, finish, fail;
} TintTest;

static bool update(void *context, float seconds, AbError *error) {
    TintTest *test = context;
    if (seconds != 0.125f)
        return ab_fail(error, "Unexpected tint update time");
    ++test->updates;
    if (test->replace)
        test->state->tween = test->replacement;
    test->state->finished = test->finish;
    return !test->fail || ab_fail(error, "Controlled tween update failure");
}

static bool values(void *context, AbError *error) {
    TintTest *test = context;
    ++test->values;
    test->state->finished = test->finish;
    return !test->fail || ab_fail(error, "Controlled tween values failure");
}

static bool check(void) {
    AbHeroScreenTinter state;
    if (!ab_hero_screen_tinter_init(&state) || !state.finished || state.tween ||
        !ab_hero_screen_tinter_update(&state, NAN, NULL))
        return false;
    TintTest first = {.state = &state, .replace = true};
    TintTest second = {.state = &state, .finish = true};
    AbHeroTintTween old = {&first, update, values}, next = {&second, update, values};
    first.replacement = &next;
    state.tween = &old;
    state.finished = false;
    if (!ab_hero_screen_tinter_update(&state, 0.125f, NULL) || state.tween ||
        first.updates != 1 || first.values || second.updates || second.values != 1)
        return false;
    first.replace = false;
    first.fail = true;
    first.finish = true;
    state.tween = &old;
    if (ab_hero_screen_tinter_update(&state, 0.125f, NULL) || state.tween != &old ||
        !state.finished || first.values || state.updating)
        return false;
    first.fail = false;
    first.replace = true;
    first.replacement = NULL;
    AbError missing = {0};
    if (ab_hero_screen_tinter_update(&state, 0.125f, &missing) || state.tween ||
        !missing.message[0] || state.updating)
        return false;
    return true;
}

int main(void) {
    if (!check())
        return 1;
    puts("Hero update rereads replacement tween values and retains late failure state");
    return 0;
}
