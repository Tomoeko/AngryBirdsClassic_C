#include "ab/native_events.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Native events failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct EventTest EventTest;
typedef struct {
    EventTest *test;
    unsigned id;
} EventContext;

struct EventTest {
    AbNativeEvents *events;
    AbNativeAnimation animation;
    EventContext contexts[7];
    uint32_t trace[32][6];
    size_t count, systems;
    unsigned case_id;
    bool updating_systems, fail;
};

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool posted(void *context, AbError *error);

static bool post(EventTest *test, unsigned id, float seconds) {
    return ab_native_events_post(
        test->events, seconds, (AbNativeEventCall){test->contexts + id, posted}, NULL);
}

static bool posted(void *context, AbError *error) {
    EventContext *item = context;
    EventTest *test = item->test;
    AbNativeEventState state;
    size_t unused;
    CHECK(test->count < 32 && ab_native_events_state(test->events, &state) &&
          !ab_native_events_process(test->events, 0, &unused, NULL));
    if (test->case_id >= 8)
        CHECK(!ab_native_animation_update(&test->animation, 0, NULL));
    uint32_t *row = test->trace[test->count++];
    row[0] = item->id;
    row[1] = (uint32_t)state.pending;
    row[2] = (uint32_t)state.delayed;
    row[3] = (uint32_t)state.processing;
    row[4] = (uint32_t)state.cursor;
    row[5] = test->updating_systems;
    if (!item->id && (test->case_id & 1))
        CHECK(post(test, 4, -1) && post(test, 5, 0));
    if (item->id == 1 && (test->case_id & 2))
        CHECK(post(test, 6, 0.25f));
    return !test->fail || ab_fail(error, "Intentional native event failure");
}

static bool prepare(EventTest *test, unsigned case_id) {
    *test = (EventTest){.case_id = case_id};
    for (unsigned id = 0; id < 7; ++id)
        test->contexts[id] = (EventContext){test, id};
    CHECK(ab_native_events_open(32, 8192, &test->events, NULL) &&
          ab_native_events_bytes(test->events) <= 8192 &&
          ab_native_animation_initialize(test->events, &test->systems,
                                         &test->updating_systems, &test->animation,
                                         NULL));
    return true;
}

static bool words(FILE *reference, const uint32_t *expected, size_t count) {
    if (!reference)
        return true;
    for (size_t index = 0; index < count; ++index) {
        uint32_t actual;
        CHECK(fscanf(reference, "%" SCNx32, &actual) == 1);
        if (actual != expected[index]) {
            fprintf(stderr, "Native word %zu: %08" PRIx32 " != %08" PRIx32 "\n", index,
                    actual, expected[index]);
            return false;
        }
    }
    return true;
}

static bool snapshot(EventTest *test, FILE *reference, unsigned step, size_t result) {
    AbNativeEventState state;
    CHECK(ab_native_events_state(test->events, &state));
    uint32_t header[] = {test->case_id, step};
    uint32_t values[] = {(uint32_t)state.pending,    (uint32_t)state.delayed,
                         (uint32_t)state.processing, (uint32_t)state.cursor,
                         (uint32_t)result,           (uint32_t)test->count,
                         test->updating_systems};
    CHECK(words(reference, header, 2) && words(reference, values, 7));
    for (size_t index = 0; index < state.delayed; ++index) {
        float seconds;
        AbNativeEventCall call;
        CHECK(ab_native_events_delay(test->events, index, &seconds, &call));
        unsigned id = 0;
        while (id < 7 && call.context != test->contexts + id)
            ++id;
        CHECK(id < 7 && call.call == posted);
        uint32_t row[] = {bits(seconds), id};
        CHECK(words(reference, row, 2));
    }
    for (size_t index = 0; index < test->count; ++index)
        CHECK(words(reference, test->trace[index], 6));
    return true;
}

static bool corpus(FILE *reference) {
    if (reference) {
        char header[64];
        CHECK(fgets(header, sizeof(header), reference) &&
              !strcmp(header, "AB_ANDROID_NATIVE_EVENTS_V1 16 8\n"));
    }
    for (unsigned case_id = 0; case_id < 16; ++case_id) {
        EventTest test;
        CHECK(prepare(&test, case_id) && post(&test, 0, case_id & 4 ? -0.0f : -1) &&
              post(&test, 1, case_id & 4 ? -1 : 0) &&
              post(&test, 2, case_id & 2 ? 0.125f : 0.25f) &&
              post(&test, 3, case_id & 2 ? 0.25f : 0.5f) &&
              snapshot(&test, reference, 0, 0));
        static const float steps[] = {0, 0.25f, 0.25f, 0.25f, 0, 1, 0};
        for (unsigned step = 1; step < 8; ++step) {
            float seconds = steps[step - 1];
            if (step == 2 && (case_id & 2))
                seconds = -0.25f;
            size_t pending;
            if (case_id < 8)
                CHECK(ab_native_events_process(test.events, seconds, &pending, NULL));
            else {
                AbNativeEventState state;
                CHECK(ab_native_animation_update(&test.animation, seconds, NULL) &&
                      ab_native_events_state(test.events, &state));
                pending = state.pending;
            }
            CHECK(snapshot(&test, reference, step, pending));
        }
        ab_native_events_close(test.events);
    }
    if (reference) {
        char marker[64];
        CHECK(fscanf(reference, " %63[^\n]", marker) == 1 &&
              !strcmp(marker, "AB native events reference complete"));
        int byte;
        do {
            byte = fgetc(reference);
        } while (byte == '\n' || byte == '\r');
        CHECK(byte == EOF && !ferror(reference));
    }
    return true;
}

static bool increment(void *context, AbError *error) {
    (void)error;
    ++*(unsigned *)context;
    return true;
}

static bool edges(void) {
    AbNativeEvents *events = NULL;
    CHECK(!ab_native_events_open(0, 8192, &events, NULL) && !events &&
          !ab_native_events_open(SIZE_MAX, SIZE_MAX, &events, NULL) && !events &&
          !ab_native_events_open(32, 1, &events, NULL) && !events &&
          ab_native_events_open(2, 8192, &events, NULL));
    unsigned calls = 0;
    AbNativeEventCall call = {&calls, increment};
    CHECK(!ab_native_events_post(events, NAN, call, NULL) &&
          ab_native_events_post(events, -1, call, NULL));
    call.call = NULL;
    CHECK(ab_native_events_post(events, -0.0f, call, NULL) &&
          !ab_native_events_post(events, 0, call, NULL));
    size_t pending = 99;
    AbError error = {0};
    AbNativeEventState state;
    CHECK(!ab_native_events_process(events, 0, &pending, &error) && calls == 1 &&
          pending == 99 && strstr(error.message, "empty native event callable") &&
          ab_native_events_state(events, &state) && state.processing == 2 &&
          state.cursor == 2 && !state.pending && !state.delayed &&
          !ab_native_events_process(events, 0, &pending, NULL) &&
          !ab_native_events_post(events, -1, call, NULL));
    ab_native_events_close(events);

    EventTest test;
    CHECK(prepare(&test, 1) && post(&test, 0, -1));
    test.fail = true;
    CHECK(!ab_native_events_process(test.events, 0, &pending, NULL) &&
          ab_native_events_state(test.events, &state) && test.count == 1 &&
          state.cursor == 1 && state.processing == 1 && state.pending == 1 &&
          state.delayed == 1);
    ab_native_events_close(test.events);

    CHECK(prepare(&test, 8) && post(&test, 0, -1) && post(&test, 2, 1) &&
          ab_native_animation_update(&test.animation, NAN, NULL) && test.count == 1 &&
          !test.updating_systems);
    test.systems = 1;
    CHECK(post(&test, 3, -1) &&
          !ab_native_animation_update(&test.animation, 1, &error) && test.count == 2 &&
          test.updating_systems && test.animation.failed && !test.animation.updating &&
          strstr(error.message, "component owner") &&
          !ab_native_animation_update(&test.animation, 0, NULL));
    float seconds;
    CHECK(ab_native_events_delay(test.events, 0, &seconds, &call) && seconds == 1);
    ab_native_events_close(test.events);
    return true;
}

int main(int argc, char **argv) {
    FILE *reference = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc > 2 || (argc == 2 && !reference))
        return 2;
    bool okay = edges() && corpus(reference);
    if (reference)
        okay = fclose(reference) == 0 && okay;
    if (!okay)
        return 1;
    puts(reference ? "Native events match 128 original queue/wrapper checkpoints"
                   : "Native event bounds, failure and callback checks passed");
    return 0;
}
