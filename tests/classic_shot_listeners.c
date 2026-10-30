#include "ab/classic_shot_listeners.h"
#include "classic_shot_listener_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Shot listener check failed at %d: %s\n", __LINE__,        \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t sample, saves, power, types, texts, analytics, notifications;
    uint32_t first, seen, level_tag, score, order[2], order_count;
} ListenerTest;

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool named(AbDataValue value, const char *name) {
    AbSpan expected = span(name);
    return value.kind == AB_DATA_STRING && value.string.size == expected.size &&
           !memcmp(value.string.data, expected.data, expected.size);
}

static bool record(ListenerTest *test, uint32_t code) {
    CHECK(test->order_count < 16);
    test->order[test->order_count / 8] |= code << (4 * (test->order_count % 8));
    ++test->order_count;
    return true;
}

static bool save(void *context, const AbDataTable *settings, AbError *error) {
    (void)error;
    ListenerTest *test = context;
    CHECK(field(settings, "achievementStatus").kind == AB_DATA_TABLE);
    ++test->saves;
    return record(test, 3);
}

static bool powerup(void *context, const char *name, AbDataValue *output,
                    AbError *error) {
    (void)error;
    ListenerTest *test = context;
    CHECK(!strcmp(name, "birdquake"));
    ++test->power;
    *output = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = (test->sample & 32) != 0};
    return record(test, 4);
}

static bool level_type(void *context, AbDataValue level, AbDataValue *output,
                       AbError *error) {
    (void)error;
    ListenerTest *test = context;
    CHECK(named(level, "fixture-level-id"));
    ++test->types;
    *output = text(test->sample % 5 == 0 ? "golden_egg" : "level");
    return record(test, 5);
}

static bool level_text(void *context, AbDataValue level, AbDataValue *output,
                       AbError *error) {
    (void)error;
    ListenerTest *test = context;
    CHECK(named(level, "fixture-level-id"));
    ++test->texts;
    *output = test->sample % 7 == 0 ? (AbDataValue){0} : text("fixture-level");
    return record(test, 6);
}

static bool analytics(void *context, const char *name, AbDataTable *parameters,
                      AbError *error) {
    (void)error;
    ListenerTest *test = context;
    CHECK(!strcmp(name, "Score generated prior to player input"));
    ++test->analytics;
    AbDataValue level = field(parameters, "level");
    CHECK(level.kind == AB_DATA_NIL || named(level, "fixture-level"));
    test->level_tag = level.kind == AB_DATA_NIL ? 0 : 4;
    test->score = field(parameters, "score").bits;
    return record(test, 7);
}

static bool unlocked(void *context, AbDataTable *event, AbError *error) {
    (void)error;
    ListenerTest *test = context;
    CHECK(named(field(event, "id"), "EID_ACHIEVEMENT_UNLOCKED"));
    AbDataValue name = field(event, "name");
    uint32_t index = named(name, shot_listener_achievements[0]) ? 0u : 1u;
    CHECK(!index || named(name, shot_listener_achievements[1]));
    if (field(event, "first_time_unlocked").bits)
        test->first |= 1u << index;
    test->seen |= 1u << index;
    ++test->notifications;
    return record(test, index + 1);
}

static bool seed_settings(AbSettingsStatistics *settings, uint32_t sample) {
    CHECK(ab_settings_statistics_set_local(settings, span("gameStarts"), number(2),
                                           NULL));
    AbSpan previous[] = {span("gameStarts"), span("device_fixture"),
                         span("installationId_previous")};
    CHECK(ab_settings_statistics_write(settings, previous, 3, number(3), NULL));
    if (sample & 1) {
        AbSpan key = span("analyticsSessionId");
        CHECK(ab_settings_statistics_write(settings, &key, 1, number(0), NULL));
    }
    AbSpan status = span("achievementStatus");
    CHECK(ab_settings_statistics_create_table(settings, &status, 1, 0, NULL));
    for (size_t index = 0; index < 2; ++index) {
        AbSpan path[] = {status, span(shot_listener_achievements[index]),
                         span("unlocked")};
        CHECK(ab_settings_statistics_create_table(settings, path, 2, 1, NULL));
        CHECK(ab_settings_statistics_write(
            settings, path, 3,
            (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = sample % 3 == 0}, NULL));
    }
    return true;
}

static void snapshot(AbSettingsStatistics *settings, AbClassicShotListeners *listeners,
                     const ListenerTest *test, bool okay,
                     uint32_t output[SHOT_LISTENER_WORDS]) {
    uint32_t prefix[] = {okay,        test->saves,     test->power,         test->types,
                         test->texts, test->analytics, test->notifications, test->first,
                         test->seen,  test->level_tag, test->score};
    memcpy(output, prefix, sizeof(prefix));
    const AbDataTable *root = ab_settings_statistics_root(settings);
    AbDataValue session = field(root, "analyticsSessionId");
    output[11] = session.kind == AB_DATA_NUMBER ? 3u : 0u;
    output[12] = session.bits;
    AbDataValue status = field(root, "achievementStatus");
    for (size_t index = 0; index < 2; ++index) {
        AbDataValue row = field(status.table, shot_listener_achievements[index]);
        output[13 + index] = field(row.table, "unlocked").bits != 0;
    }
    output[15] = test->order[0];
    output[16] = test->order[1];
    output[17] = ab_classic_shot_listeners_state(listeners)->bird_slinger_limit.bits;
}

static bool compare(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_CLASSIC_SHOT_LISTENERS_V1 64 2\n"));
    for (uint32_t sample = 0; sample < SHOT_LISTENER_CASES; ++sample) {
        uint32_t expected_sample;
        CHECK(fscanf(file, "%8" SCNx32, &expected_sample) == 1 &&
              sample == expected_sample);
        ListenerTest test = {.sample = sample};
        AbSettingsStatistics *settings = NULL;
        AbEventManager *events = NULL;
        AbClassicShotListeners *listeners = NULL;
        CHECK(ab_settings_statistics_open(
            (AbSettingsStatisticsConfig){span("fixture"), span("fixture"), 16, 16,
                                         256 * 1024},
            &settings, NULL));
        CHECK(seed_settings(settings, sample));
        CHECK(ab_event_manager_open((AbEventManagerLimits){4, 4, 4, 0, 8, 16384},
                                    (AbEventManagerHost){0}, &events, NULL));
        AbClassicShotListenerHost host = {&test,      save,       powerup,
                                          level_type, level_text, analytics};
        CHECK(ab_classic_shot_listeners_open(
            (AbClassicShotListenerConfig){settings, events, host, 8, 16384}, &listeners,
            NULL));
        AbClassicShotListenerState *state = ab_classic_shot_listeners_state(listeners);
        state->current_bird_index = sample & 8 ? 2.0f : 1.0f;
        state->score = sample & 4 ? 123.0f : 0.0f;
        state->powerups_enabled =
            (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = (sample & 16) != 0};
        state->level_name = text("fixture-level-id");
        state->analytics_present =
            sample % 11 ? (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}
                        : (AbDataValue){0};
        state->ios = (AbDataValue){.kind = AB_DATA_BOOLEAN};
        CHECK(ab_classic_shot_listeners_bind(listeners, NULL));
        AbEventListener observer = {span("fixture"), &test, unlocked};
        CHECK(ab_event_manager_add(events, text("EID_ACHIEVEMENT_UNLOCKED"), &observer,
                                   NULL));
        AbDataEntry entries[] = {
            {text("id"), text("EID_BIRD_SHOT")},
            {text("birdsShooted"), number(sample & 1 ? 5000.0f : 4999.0f)},
            {text("backwardsBirdCount"), number(sample & 2 ? 10.0f : 9.0f)}};
        AbDataTable event = {entries, 3, 3, NULL};
        for (uint32_t pass = 0; pass < SHOT_LISTENER_PASSES; ++pass) {
            test.order[0] = test.order[1] = test.order_count = 0;
            uint32_t actual[SHOT_LISTENER_WORDS], expected[SHOT_LISTENER_WORDS];
            bool okay = ab_event_manager_notify(events, &event, NULL);
            snapshot(settings, listeners, &test, okay, actual);
            for (size_t word = 0; word < SHOT_LISTENER_WORDS; ++word) {
                CHECK(fscanf(file, "%8" SCNx32, expected + word) == 1);
                if (actual[word] != expected[word]) {
                    fprintf(stderr,
                            "Shot listener native mismatch case %u pass %u word %zu: "
                            "%08x != %08x\n",
                            sample, pass, word, actual[word], expected[word]);
                    return false;
                }
            }
        }
        ab_event_manager_close(events);
        ab_classic_shot_listeners_close(listeners);
        ab_settings_statistics_close(settings);
    }
    CHECK(fgetc(file) == '\n' && fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB classic shot listeners reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    puts("Two actual shot listener routes match 128 original deliveries, shared "
         "achievement/session state and ordered save/query/analytics boundaries.");
    return true;
}

static bool partial_save_failure(void) {
    AbSettingsStatistics *settings = NULL;
    AbEventManager *events = NULL;
    AbClassicShotListeners *listeners = NULL;
    CHECK(ab_settings_statistics_open(
        (AbSettingsStatisticsConfig){span("fixture"), span("fixture"), 8, 8, 65536},
        &settings, NULL));
    CHECK(ab_event_manager_open((AbEventManagerLimits){2, 2, 0, 0, 4, 8192},
                                (AbEventManagerHost){0}, &events, NULL));
    CHECK(ab_classic_shot_listeners_open(
        (AbClassicShotListenerConfig){settings, events, {0}, 2, 4096}, &listeners,
        NULL));
    CHECK(ab_classic_shot_listeners_bind(listeners, NULL) &&
          !ab_classic_shot_listeners_bind(listeners, NULL));
    AbDataEntry entries[] = {{text("id"), text("EID_BIRD_SHOT")},
                             {text("birdsShooted"), number(5000)}};
    AbDataTable event = {entries, 2, 2, NULL};
    CHECK(!ab_event_manager_notify(events, &event, NULL));
    AbDataValue status =
        field(ab_settings_statistics_root(settings), "achievementStatus");
    CHECK(status.kind == AB_DATA_TABLE);
    AbDataValue bird = field(status.table, "Bird Slinger");
    CHECK(bird.kind == AB_DATA_TABLE && field(bird.table, "unlocked").bits == 1 &&
          field(ab_settings_statistics_root(settings), "analyticsSessionId").kind ==
              AB_DATA_NIL);
    ab_event_manager_close(events);
    ab_classic_shot_listeners_close(listeners);
    ab_settings_statistics_close(settings);
    return true;
}

int main(int argc, char **argv) {
    if (!partial_save_failure())
        return 1;
    if (argc == 1)
        return 0;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = compare(file);
    if (fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
