#include "ab/session_timer.h"
#include "../tools/oracle/reference_session_timer.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Session timer failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbCalendarTime base;
    float elapsed;
    unsigned calls;
    AbSessionTimerState *mutate;
    const AbCalendarTime *replacement_start;
} Clock;

static uint32_t bits(float value) {
    uint32_t output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static AbDataValue number(float value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(value)};
}

static bool clock_now(void *context, AbCalendarTime *output, AbError *error) {
    (void)error;
    Clock *clock = context;
    ++clock->calls;
    *output = clock->base;
    output->seconds += clock->elapsed;
    if (clock->mutate) {
        clock->mutate->start = clock->replacement_start;
        clock->mutate->duration_seconds = number(77);
    }
    return true;
}

static void value_words(AbDataValue value, uint32_t *output) {
    output[0] = value.kind == AB_DATA_NUMBER ? 3 : (uint32_t)value.kind;
    output[1] =
        value.kind == AB_DATA_NUMBER || value.kind == AB_DATA_BOOLEAN ? value.bits : 0;
}

static void date_words(const AbCalendarTime *date, uint32_t *output) {
    output[0] = date != NULL;
    if (date) {
        const float fields[] = {date->year, date->month,   date->day,
                                date->hour, date->minutes, date->seconds};
        for (unsigned index = 0; index < 6; ++index)
            output[index + 1] = bits(fields[index]);
    }
}

static void duration_words(const AbSessionDuration *duration, uint32_t *output) {
    output[0] = duration != NULL;
    if (duration) {
        output[1] = bits(duration->hours);
        output[2] = bits(duration->minutes);
        output[3] = bits(duration->seconds);
    }
}

static bool compare(FILE *reference, const uint32_t *values, size_t count,
                    unsigned profile, unsigned step, const char *phase) {
    if (!reference)
        return true;
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(reference, "%" SCNx32, &expected) == 1);
        if (values[index] != expected) {
            fprintf(stderr,
                    "Session profile %u step %u %s word %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    profile, step, phase, index, values[index], expected);
            return false;
        }
    }
    return true;
}

static bool snapshot(AbSessionTimer *timer, Clock *clock, FILE *reference,
                     unsigned profile, unsigned step) {
    uint32_t state[24] = {0}, results[11] = {0}, save[11] = {0};
    unsigned clock_before = clock->calls;
    value_words(timer->state->active, state);
    value_words(timer->state->force_completed, state + 2);
    value_words(timer->state->duration_seconds, state + 4);
    date_words(timer->state->start, state + 6);
    duration_words(timer->state->duration, state + 13);
    if (timer->state->start && timer->state->duration) {
        AbCalendarTime end;
        CHECK(ab_session_timer_end(timer, &end, NULL));
        date_words(&end, state + 17);
    }
    AbDataValue remaining, complete = {0}, formatted;
    float progress = 0.0f;
    char text[64];
    CHECK(ab_session_timer_remaining(timer, &remaining, NULL) &&
          ab_session_timer_complete(timer, &complete, NULL) &&
          ab_session_timer_progress(timer, &progress, NULL) &&
          ab_session_timer_formatted(timer, text, sizeof(text), &formatted, NULL));
    value_words(remaining, results);
    value_words(complete, results + 2);
    results[4] = isnan(progress) ? UINT32_C(0x7fc00000) : bits(progress);
    results[5] = formatted.kind == AB_DATA_STRING ? 4u : 3u;
    if (formatted.kind == AB_DATA_STRING) {
        CHECK(formatted.string.size <= 16);
        results[6] = (uint32_t)formatted.string.size;
        for (size_t byte = 0; byte < formatted.string.size; ++byte)
            results[7 + byte / 4] |= (uint32_t)formatted.string.data[byte]
                                     << ((byte % 4) * 8);
    } else
        CHECK(formatted.kind == AB_DATA_NUMBER && !formatted.bits);
    AbSessionTimerSave saved;
    CHECK(ab_session_timer_save(timer, &saved, NULL));
    date_words(saved.has_start ? &saved.start : NULL, save);
    duration_words(saved.has_duration ? &saved.duration : NULL, save + 7);
    uint32_t header[] = {profile, step, clock_before, clock->calls};
    return compare(reference, header, 4, profile, step, "header") &&
           compare(reference, state, 24, profile, step, "state") &&
           compare(reference, results, 11, profile, step, "results") &&
           compare(reference, save, 11, profile, step, "save");
}

static bool action(AbSessionTimer *timer, Clock *clock, AbCalendarTime *start,
                   AbSessionDuration *duration, AbSessionTimerSave *saved,
                   unsigned profile, unsigned step) {
    if (step == 1 || step == 11 || step == 15)
        return ab_session_timer_start(timer, duration, profile & 1 ? start : NULL,
                                      NULL);
    if (step == 3) {
        duration->seconds += 1;
        if (profile & 1)
            start->seconds = clock->base.seconds + 1;
    } else if (step == 4)
        return ab_session_timer_save(timer, saved, NULL);
    else if (step == 5 || step == 16)
        return ab_session_timer_load(timer, NULL, NULL);
    else if (step == 8)
        return ab_session_timer_load(timer, saved, NULL);
    else if (step == 12) {
        AbSessionTimerSave partial = {.has_start = true, .start = *start};
        return ab_session_timer_load(timer, &partial, NULL);
    } else if (step == 6 || step == 14 || step == 17)
        return ab_session_timer_set_active(
            timer,
            step == 6 ? number(0)
                      : (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = step == 17},
            NULL);
    else if (step == 7)
        return ab_session_timer_init(timer, NULL);
    else if (step == 10)
        return ab_session_timer_force_complete(timer, NULL);
    else if (step == 9) {
        saved->start.seconds = 103;
        saved->duration.seconds = -101;
    }
    return true;
}

static bool corpus(FILE *reference) {
    if (reference) {
        char header[80];
        CHECK(fgets(header, sizeof(header), reference) &&
              !strcmp(header, "AB_ANDROID_SESSION_TIMER_V1 24 18 UTC\n"));
    }
    for (unsigned profile = 0; profile < 24; ++profile) {
        float dates[6], durations[3];
        reference_session_timer(profile, dates, durations);
        AbCalendarTime start = {dates[0], dates[1], dates[2],
                                dates[3], dates[4], dates[5]};
        AbSessionDuration duration = {durations[0], durations[1], durations[2]};
        Clock clock = {.base = start};
        AbSessionTimerState state = {
            .duration_seconds = number(123), .start = &start, .duration = &duration};
        if (profile % 4 == 1 || profile % 4 == 3)
            state.force_completed =
                (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = profile % 4 == 3};
        else if (profile % 4 == 2)
            state.force_completed = number(0);
        AbSessionTimer timer;
        AbSessionTimerSave saved = {0};
        CHECK(ab_session_timer_initialize(&timer, &state,
                                          (AbSessionClock){&clock, clock_now}, NULL));
        for (unsigned step = 0; step < 18; ++step) {
            clock.elapsed = reference_session_elapsed(step);
            CHECK(action(&timer, &clock, &start, &duration, &saved, profile, step));
            CHECK(snapshot(&timer, &clock, reference, profile, step));
        }
    }
    if (reference) {
        char marker[80];
        CHECK(fscanf(reference, " %79[^\n]", marker) == 1 &&
              !strcmp(marker, "AB session timer reference complete"));
        int byte;
        do {
            byte = fgetc(reference);
        } while (byte == '\n' || byte == '\r');
        CHECK(byte == EOF && !ferror(reference));
    }
    return true;
}

static bool edges(void) {
    AbSessionTimerState state = {.force_completed = number(0)};
    AbSessionTimer timer;
    AbDataValue result;
    CHECK(ab_session_timer_initialize(&timer, &state, (AbSessionClock){0}, NULL) &&
          ab_session_timer_complete(&timer, &result, NULL) &&
          result.kind == AB_DATA_NIL &&
          ab_session_timer_set_active(&timer, number(0), NULL) &&
          ab_session_timer_complete(&timer, &result, NULL) &&
          result.kind == AB_DATA_NUMBER && !result.bits);
    AbCalendarTime start = {2026, 1, 1, 0, 0, 0};
    AbSessionDuration duration = {0, 5, 0};
    state.duration_seconds = number(23);
    CHECK(!ab_session_timer_start(&timer, NULL, &start, NULL) && timer.failed &&
          state.start == &start && !state.duration &&
          state.duration_seconds.bits == bits(23) &&
          state.active.kind == AB_DATA_NUMBER);
    state = (AbSessionTimerState){.start = &start, .duration = &duration};
    CHECK(ab_session_timer_initialize(&timer, &state, (AbSessionClock){0}, NULL) &&
          !ab_session_timer_start(&timer, &duration, NULL, NULL) && timer.failed &&
          state.start == &start && state.duration == &duration &&
          state.active.kind == AB_DATA_BOOLEAN && !state.active.bits);
    AbCalendarTime replacement = start;
    replacement.seconds = 10;
    Clock clock = {.base = start,
                   .elapsed = 11,
                   .mutate = &state,
                   .replacement_start = &replacement};
    state = (AbSessionTimerState){0};
    CHECK(ab_session_timer_initialize(&timer, &state,
                                      (AbSessionClock){&clock, clock_now}, NULL) &&
          ab_session_timer_start(&timer, &duration, &start, NULL) &&
          ab_session_timer_remaining(&timer, &result, NULL) &&
          result.kind == AB_DATA_NUMBER && result.bits == bits(76) && clock.calls == 1);
    timer.busy = true;
    CHECK(!ab_session_timer_init(&timer, NULL) && timer.busy && !timer.failed);
    timer.busy = false;
    return true;
}

int main(int argc, char **argv) {
    const char *zone = getenv("TZ");
    if (!zone || strcmp(zone, "UTC0") || argc > 2) {
        fputs("Session timer comparison requires TZ=UTC0 and optional corpus\n",
              stderr);
        return 2;
    }
    FILE *reference = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc == 2 && !reference)
        return 2;
    bool okay = edges() && corpus(reference);
    if (reference && fclose(reference))
        okay = false;
    if (okay)
        puts("Session timer: 432 checkpoints, 21600 state/result/clock/save words");
    return okay ? 0 : 1;
}
