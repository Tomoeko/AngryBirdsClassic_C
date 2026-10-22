#include "ab/classic_game_updates.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Game updates failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

typedef struct {
    AbClassicGameUpdates *updates;
    AbClassicGameUpdateFunction fresh;
    size_t old_calls, fresh_calls;
} Replacement;

static bool fresh_call(void *context, float seconds, float time, AbError *error) {
    Replacement *test = context;
    CHECK(seconds == 0.125f && time == 4);
    ++test->fresh_calls;
    return ab_classic_game_updates_remove(test->updates, span("fresh"), error);
}

static bool old_call(void *context, float seconds, float time, AbError *error) {
    Replacement *test = context;
    CHECK(seconds == 0.125f && time == 4 &&
          !ab_classic_game_updates_call(test->updates, seconds, time, NULL));
    if (!test->old_calls) {
        CHECK(ab_classic_game_updates_clear(test->updates, error) &&
              ab_classic_game_updates_add(test->updates, span("discarded"),
                                          &test->fresh, error) &&
              ab_classic_game_updates_clear(test->updates, error) &&
              ab_classic_game_updates_add(test->updates, span("fresh"), &test->fresh,
                                          error));
    }
    ++test->old_calls;
    return true;
}

static bool fail_call(void *context, float seconds, float time, AbError *error) {
    size_t *calls = context;
    (void)seconds;
    (void)time;
    ++*calls;
    return ab_fail(error, "Intentional registered callback failure");
}

static bool check_registry(void) {
    AbError error = {0};
    AbClassicGameUpdates *updates = NULL;
    CHECK(ab_classic_game_updates_open(8, 8192, &updates, &error) &&
          ab_classic_game_updates_bytes(updates) <= 8192 &&
          !ab_classic_game_updates_call(updates, 0, 0, NULL) &&
          !ab_classic_game_updates_add(updates, span("uninitialized"), NULL, NULL) &&
          ab_classic_game_updates_clear(updates, &error) &&
          ab_classic_game_updates_call(updates, NAN, NAN, &error));
    Replacement test = {.updates = updates};
    test.fresh = (AbClassicGameUpdateFunction){&test, fresh_call};
    AbClassicGameUpdateFunction old = {&test, old_call};
    CHECK(ab_classic_game_updates_add(updates, span("first"), &old, &error) &&
          ab_classic_game_updates_add(updates, span("second"), &old, &error) &&
          ab_classic_game_updates_add(updates, span("first"), NULL, &error) &&
          ab_classic_game_updates_call(updates, 0.125f, 4, &error) &&
          test.old_calls == 2 && !test.fresh_calls &&
          ab_classic_game_updates_call(updates, 0.125f, 4, &error) &&
          test.fresh_calls == 1 &&
          ab_classic_game_updates_call(updates, 0.125f, 4, &error) &&
          test.fresh_calls == 1);
    CHECK(ab_classic_game_updates_clear(updates, &error));
    size_t calls = 0;
    AbClassicGameUpdateFunction failure = {&calls, fail_call};
    CHECK(ab_classic_game_updates_add(updates, span("failure"), &failure, &error) &&
          !ab_classic_game_updates_call(updates, 0, 0, &error) && calls == 1 &&
          strstr(error.message, "Intentional registered callback failure") &&
          !ab_classic_game_updates_call(updates, 0, 0, NULL) &&
          !ab_classic_game_updates_clear(updates, NULL) && calls == 1);
    ab_classic_game_updates_close(updates);
    return true;
}

int main(void) {
    return check_registry() ? 0 : 1;
}
