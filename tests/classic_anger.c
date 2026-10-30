#include "ab/classic_anger.h"
#include "../tools/oracle/reference_anger.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "AngerLevel failed at %d: %s\n", __LINE__, #condition);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbClassicAnger *owner;
    AbClassicAngerState state;
    AbClassicAngerSettings input, *settings;
    AbEventManager *events;
    AbEventListener observer;
    AbDataValue late, increased;
    unsigned profile, step, clock_calls, count;
    uint32_t rows[64][12];
    bool fail_save;
} Test;

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static void value_words(AbDataValue value, uint32_t *output) {
    static const unsigned kinds[] = {0, 1, 3, 4, 5};
    output[0] = kinds[value.kind];
    output[1] =
        value.kind == AB_DATA_NUMBER || value.kind == AB_DATA_BOOLEAN ? value.bits : 0;
}

static void date_words(const AbCalendarTime *date, uint32_t *output) {
    output[0] = date != NULL;
    if (date) {
        float fields[] = {date->year, date->month,   date->day,
                          date->hour, date->minutes, date->seconds};
        for (unsigned index = 0; index < 6; ++index)
            output[index + 1] = number(fields[index]).bits;
    }
}

static void duration_words(const AbSessionDuration *duration, uint32_t *output) {
    output[0] = duration != NULL;
    if (duration) {
        output[1] = number(duration->hours).bits;
        output[2] = number(duration->minutes).bits;
        output[3] = number(duration->seconds).bits;
    }
}

static bool record(Test *test, unsigned kind) {
    CHECK(test->count < 64);
    uint32_t *row = test->rows[test->count++];
    memset(row, 0, 12 * sizeof(*row));
    row[0] = kind;
    CHECK(test->state.level.kind == AB_DATA_NUMBER &&
          test->state.queued.kind == AB_DATA_NUMBER &&
          test->state.charges.kind == AB_DATA_NUMBER);
    row[1] = test->state.level.bits;
    row[2] = test->state.queued.bits;
    row[3] = test->state.charges.bits;
    value_words(test->state.level_timer.active, row + 4);
    value_words(test->state.cooldown_timer.active, row + 6);
    if (test->settings) {
        value_words(test->settings->level, row + 8);
        value_words(test->settings->queued, row + 10);
    }
    return true;
}

static bool clock_now(void *context, AbCalendarTime *output, AbError *error) {
    Test *test = context;
    (void)error;
    ++test->clock_calls;
    *output =
        (AbCalendarTime){2026, 12, 31, 23, 59, reference_anger_elapsed(test->step)};
    return true;
}

static bool in_level(void *context, AbDataValue *output, AbError *error) {
    Test *test = context;
    (void)error;
    CHECK(record(test, 1));
    *output = (test->profile & 4) && test->step < 8
                  ? number(0)
                  : (AbDataValue){.kind = AB_DATA_BOOLEAN,
                                  .bits = (test->profile & 1) && test->step < 8};
    return true;
}

static bool save_settings(void *context, AbError *error) {
    Test *test = context;
    CHECK(record(test, 2));
    return !test->fail_save || ab_fail(error, "Intentional settings failure");
}

static bool load_popup(void *context, AbError *error) {
    (void)error;
    return record(context, 3);
}

static bool increased(void *context, AbDataTable *event, AbError *error) {
    (void)error;
    const AbDataValue *id = ab_data_get(event, text("id"));
    CHECK(id && id->kind == AB_DATA_STRING && id->string.size == 9 &&
          !memcmp(id->string.data, "increased", 9));
    return record(context, 4);
}

static void settings_input(Test *test, unsigned reload) {
    float values[4], date[6];
    unsigned kinds[4], timers[2];
    reference_anger(test->profile, reload, values, kinds, date, timers);
    AbDataValue fields[4];
    for (unsigned index = 0; index < 4; ++index)
        fields[index] = kinds[index] == 1 ? (AbDataValue){.kind = AB_DATA_BOOLEAN}
                                          : number(values[index]);
    test->input = (AbClassicAngerSettings){.maximum = fields[0],
                                           .level = fields[1],
                                           .charges = fields[2],
                                           .queued = fields[3]};
    AbSessionTimerSave *saved[] = {&test->input.timer, &test->input.cooldown};
    for (unsigned index = 0; index < 2; ++index) {
        if (!timers[index])
            continue;
        *saved[index] = (AbSessionTimerSave){
            .has_start = true,
            .has_duration = true,
            .start = {date[0], date[1], date[2], date[3], date[4], date[5]},
            .duration = {0, index ? 30.0f : 5.0f, 0}};
    }
    test->settings = !reload && !test->profile ? NULL : &test->input;
}

static bool prepare(Test *test, unsigned profile) {
    *test = (Test){
        .profile = profile, .late = text("late"), .increased = text("increased")};
    settings_input(test, 0);
    CHECK(ab_event_manager_open((AbEventManagerLimits){4, 4, 4, 0, 8, 16384},
                                (AbEventManagerHost){0}, &test->events, NULL));
    test->observer = (AbEventListener){text("observer").string, test, increased};
    CHECK(
        ab_event_manager_add(test->events, test->increased, &test->observer, NULL) &&
        ab_classic_anger_open((AbClassicAngerConfig){.state = &test->state,
                                                     .settings = &test->settings,
                                                     .clock = {test, clock_now},
                                                     .events = test->events,
                                                     .load_level_late = test->late,
                                                     .level_increased = test->increased,
                                                     .context = test,
                                                     .is_in_level = in_level,
                                                     .save_settings = save_settings,
                                                     .load_refill_popup = load_popup,
                                                     .records = 64,
                                                     .iteration_limit = 64,
                                                     .storage_bytes = 262144},
                              &test->owner, NULL));
    return true;
}

static void close_test(Test *test) {
    ab_classic_anger_close(test->owner);
    ab_event_manager_close(test->events);
}

static bool action(Test *test) {
    unsigned step = test->step;
    if (step == 1 || step == 2)
        return ab_classic_anger_activate_cooldown(test->owner, NULL);
    if (step == 4) {
        settings_input(test, 1);
        return ab_classic_anger_load(test->owner, NULL);
    }
    if (step == 7 || step == 8) {
        AbDataEntry entry = {text("id"), test->late};
        AbDataTable event = {.entries = &entry, .count = 1, .capacity = 1};
        return ab_event_manager_notify(test->events, &event, NULL);
    }
    if (step == 9)
        return ab_classic_anger_add(test->owner, -1, NULL);
    if (step == 10)
        return ab_classic_anger_save(test->owner, NULL);
    if (step == 11)
        return ab_classic_anger_load(test->owner, NULL);
    if (step == 13) {
        test->settings = NULL;
        return ab_classic_anger_load(test->owner, NULL);
    }
    return ab_classic_anger_update(test->owner, NULL);
}

static bool compare(FILE *reference, const uint32_t *values, size_t count, Test *test,
                    const char *phase) {
    if (!reference)
        return true;
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(reference, "%" SCNx32, &expected) == 1);
        if (values[index] != expected) {
            fprintf(stderr,
                    "AngerLevel %u/%u %s[%zu]: %08" PRIx32 " != %08" PRIx32 "\n",
                    test->profile, test->step, phase, index, values[index], expected);
            return false;
        }
    }
    return true;
}

static bool snapshot(Test *test, FILE *reference) {
    uint32_t state[48] = {0}, saved[31] = {0};
    AbDataValue fields[] = {test->state.initialized, test->state.maximum,
                            test->state.boost_cost,  test->state.level,
                            test->state.charges,     test->state.queued,
                            test->state.used};
    for (unsigned index = 0; index < 7; ++index)
        value_words(fields[index], state + index * 2);
    AbSessionTimerState *timers[] = {&test->state.level_timer,
                                     &test->state.cooldown_timer};
    for (unsigned index = 0; index < 2; ++index) {
        uint32_t *row = state + 14 + index * 17;
        value_words(timers[index]->active, row);
        value_words(timers[index]->force_completed, row + 2);
        value_words(timers[index]->duration_seconds, row + 4);
        date_words(timers[index]->start, row + 6);
        duration_words(timers[index]->duration, row + 13);
    }
    saved[0] = test->settings != NULL;
    if (saved[0]) {
        const AbClassicAngerSettings *input = test->settings;
        AbDataValue values[] = {input->maximum, input->level, input->charges,
                                input->queued};
        for (unsigned index = 0; index < 4; ++index)
            value_words(values[index], saved + 1 + index * 2);
        const AbSessionTimerSave *tables[] = {&input->timer, &input->cooldown};
        for (unsigned index = 0; index < 2; ++index) {
            date_words(tables[index]->has_start ? &tables[index]->start : NULL,
                       saved + 9 + index * 11);
            duration_words(tables[index]->has_duration ? &tables[index]->duration
                                                       : NULL,
                           saved + 16 + index * 11);
        }
    }
    uint32_t header[] = {test->profile, test->step, test->clock_calls, test->count};
    CHECK(compare(reference, header, 4, test, "header") &&
          compare(reference, state, 48, test, "state") &&
          compare(reference, saved, 31, test, "settings"));
    for (unsigned index = 0; index < test->count; ++index)
        CHECK(compare(reference, test->rows[index], 12, test, "trace"));
    return true;
}

static bool corpus(FILE *reference) {
    if (reference) {
        char header[64];
        CHECK(fgets(header, sizeof(header), reference) &&
              !strcmp(header, "AB_ANDROID_ANGER_V1 8 14 UTC\n"));
    }
    unsigned records = 0;
    for (unsigned profile = 0; profile < 8; ++profile) {
        Test test;
        CHECK(prepare(&test, profile));
        for (test.step = 0; test.step < 14; ++test.step) {
            test.count = 0;
            CHECK(action(&test) && snapshot(&test, reference));
            records += test.count;
        }
        close_test(&test);
    }
    if (reference) {
        char marker[80];
        CHECK(fscanf(reference, " %79[^\n]", marker) == 1 &&
              !strcmp(marker, "AB AngerLevel reference complete"));
        int byte;
        do {
            byte = fgetc(reference);
        } while (byte == '\n' || byte == '\r');
        CHECK(byte == EOF && !ferror(reference));
    }
    printf("AngerLevel: 112 checkpoints, %u ordered records\n", records);
    return true;
}

static bool edges(void) {
    Test test;
    CHECK(prepare(&test, 0) && ab_classic_anger_update(test.owner, NULL) &&
          test.state.maximum.bits == number(2).bits &&
          test.state.level.bits == number(2).bits && !test.clock_calls && !test.count &&
          test.state.level_timer.active.kind == AB_DATA_BOOLEAN &&
          !test.state.level_timer.active.bits);
    test.fail_save = true;
    AbError error = {0};
    CHECK(!ab_classic_anger_add(test.owner, -1, &error) &&
          strstr(error.message, "Intentional settings failure") &&
          test.state.level.bits == number(1).bits && test.settings &&
          test.settings->level.bits == number(1).bits &&
          !ab_classic_anger_update(test.owner, NULL));
    close_test(&test);
    return true;
}

int main(int argc, char **argv) {
    const char *zone = getenv("TZ");
    if (!zone || strcmp(zone, "UTC0") || argc > 2)
        return 2;
    FILE *reference = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc == 2 && !reference)
        return 2;
    bool okay = edges() && corpus(reference);
    if (reference && fclose(reference))
        okay = false;
    return okay ? 0 : 1;
}
