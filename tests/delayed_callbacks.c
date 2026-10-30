#include "ab/delayed_callbacks.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Delayed callbacks failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbDelayedCallbacks *callbacks;
    AbDelayedFunction function;
    uint32_t calls[128];
    size_t count;
    bool append, reset, fail;
} Fixture;

static AbDataValue label(uint32_t id) {
    return (AbDataValue){.kind = AB_DATA_OBJECT, .object = (const void *)(uintptr_t)id};
}

static bool call(void *context, AbDataValue parameters, AbError *error) {
    Fixture *test = context;
    CHECK(parameters.kind == AB_DATA_OBJECT && test->count < 128);
    uint32_t id = (uint32_t)(uintptr_t)parameters.object;
    test->calls[test->count++] = id;
    CHECK(!ab_delayed_callbacks_update(test->callbacks, 1, NULL));
    if (test->fail)
        return ab_fail(error, "Intentional delayed callback failure");
    if (id == 1 && test->reset) {
        CHECK(ab_delayed_callbacks_initialize(test->callbacks, error) &&
              ab_delayed_callbacks_add(test->callbacks, 0, &test->function, label(90),
                                       error) &&
              ab_delayed_callbacks_add(test->callbacks, 0, &test->function, label(91),
                                       error) &&
              ab_delayed_callbacks_add(test->callbacks, 0, &test->function, label(92),
                                       error));
    } else if (id == 1 && test->append) {
        CHECK(ab_delayed_callbacks_add(test->callbacks, 0, &test->function, label(3),
                                       error));
    }
    return true;
}

static bool check_callbacks(void) {
    AbClassicGameUpdates *updates = NULL;
    Fixture test = {0};
    test.function = (AbDelayedFunction){&test, call};
    CHECK(
        ab_classic_game_updates_open(16, 65536, &updates, NULL) &&
        ab_classic_game_updates_clear(updates, NULL) &&
        !ab_delayed_callbacks_open(updates, 3, 65536, &test.callbacks, NULL) &&
        ab_delayed_callbacks_open(updates, 8, 65536, &test.callbacks, NULL) &&
        ab_delayed_callbacks_bytes(test.callbacks) <= 65536 &&
        !ab_delayed_callbacks_add(test.callbacks, 0, &test.function, label(1), NULL) &&
        ab_delayed_callbacks_initialize(test.callbacks, NULL) &&
        !ab_delayed_callbacks_add(test.callbacks, NAN, &test.function, label(1), NULL));
    test.append = true;
    CHECK(
        ab_delayed_callbacks_add(test.callbacks, .25f, &test.function, label(1),
                                 NULL) &&
        ab_delayed_callbacks_add(test.callbacks, .5f, &test.function, label(2), NULL) &&
        ab_classic_game_updates_call(updates, .125f, 0, NULL) && !test.count &&
        ab_classic_game_updates_call(updates, .125f, 0, NULL) && test.count == 1 &&
        test.calls[0] == 1 && ab_classic_game_updates_call(updates, 0, 0, NULL) &&
        test.count == 2 && test.calls[1] == 3 &&
        ab_classic_game_updates_call(updates, .25f, 0, NULL) && test.count == 3 &&
        test.calls[2] == 2);
    const uint8_t key[] = "updateDelayedCallbacks";
    const AbClassicGameUpdateFunction *registered = NULL;
    CHECK(ab_classic_game_updates_lookup(updates, (AbSpan){key, sizeof(key) - 1},
                                         &registered, NULL) &&
          registered && ab_classic_game_updates_call(updates, 1, 0, NULL) &&
          test.count == 3);
    test.count = 0;
    test.reset = true;
    CHECK(ab_delayed_callbacks_initialize(test.callbacks, NULL) &&
          ab_delayed_callbacks_add(test.callbacks, 0, &test.function, label(1), NULL) &&
          ab_delayed_callbacks_add(test.callbacks, 0, &test.function, label(2), NULL) &&
          ab_classic_game_updates_call(updates, 0, 0, NULL) && test.count == 2 &&
          test.calls[0] == 1 && test.calls[1] == 2 &&
          ab_classic_game_updates_call(updates, 0, 0, NULL) && test.count == 3 &&
          test.calls[2] == 92);
    /* Repeated expiry reuses record slots but continues numeric IDs and forces
     * sparse hash growth/rehash, rather than treating IDs as pool indices. */
    test.reset = test.append = false;
    for (size_t index = 0; index < 64; ++index) {
        test.count = 0;
        CHECK(ab_delayed_callbacks_add(test.callbacks, -.1f, &test.function, label(4),
                                       NULL) &&
              ab_classic_game_updates_call(updates, 0, 0, NULL) && test.count == 1 &&
              test.calls[0] == 4);
    }
    test.fail = true;
    CHECK(ab_delayed_callbacks_add(test.callbacks, 0, &test.function, label(4), NULL) &&
          !ab_classic_game_updates_call(updates, 0, 0, NULL) &&
          !ab_delayed_callbacks_update(test.callbacks, 0, NULL) &&
          !ab_delayed_callbacks_initialize(test.callbacks, NULL));
    ab_classic_game_updates_close(updates);
    ab_delayed_callbacks_close(test.callbacks);
    puts(
        "Delayed timeout boundaries, unscaled clock, shared snapshots, reentrant "
        "append/reset, current-table deletion, sparse IDs and failure retention pass.");
    return true;
}

int main(void) {
    return check_callbacks() ? 0 : 1;
}
