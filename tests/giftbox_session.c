#include "ab/giftbox_session.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Gift-box check failed at %d: %s\n", __LINE__,             \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbSettingsStatistics *settings;
    AbGiftBoxSession session;
    AbDataValue remote;
    AbCalendarTime times[2];
    unsigned calls, fail_call;
    bool nested, mutate_saved;
} Fixture;

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

static AbDataValue get(const AbDataTable *table, const char *key) {
    const AbDataValue *value = ab_data_get(table, text(key));
    return value ? *value : (AbDataValue){0};
}

static bool equal(AbDataValue value, float expected) {
    float actual;
    memcpy(&actual, &value.bits, sizeof(actual));
    return value.kind == AB_DATA_NUMBER && actual == expected;
}

static bool put(Fixture *fixture, const char *key, AbDataValue value) {
    AbSpan path = span(key);
    return ab_settings_statistics_write(fixture->settings, &path, 1, value, NULL);
}

static bool clock_now(void *context, AbCalendarTime *output, AbError *error) {
    Fixture *fixture = context;
    ++fixture->calls;
    if (fixture->nested && ab_giftbox_session_update(&fixture->session, NULL))
        return ab_fail(error, "Recursive gift-box update unexpectedly accepted");
    if (fixture->fail_call == fixture->calls)
        return ab_fail(error, "Controlled calendar failure");
    if (fixture->mutate_saved) {
        AbSpan path[] = {span("giftBoxSessionStartTime"), span("hour")};
        if (!ab_settings_statistics_write(fixture->settings, path, 2, number(8), error))
            return false;
    }
    *output = fixture->times[fixture->calls == 1 ? 0 : 1];
    return true;
}

static bool open_fixture(Fixture *fixture) {
    *fixture = (Fixture){.times = {{2026, 10, 22, 7, 0, 59}, {2026, 10, 22, 7, 1, 13}}};
    AbSettingsStatisticsConfig config = {span("fixture"), span("fixture"), 32, 16,
                                         512 * 1024};
    CHECK(ab_settings_statistics_open(config, &fixture->settings, NULL));
    CHECK(ab_giftbox_session_initialize(&fixture->session, fixture->settings,
                                        &fixture->remote,
                                        (AbSessionClock){fixture, clock_now}, NULL));
    return true;
}

static bool date(Fixture *fixture, AbCalendarTime time) {
    const char *const keys[] = {"year", "month", "day", "hour", "minutes", "seconds"};
    const float fields[] = {time.year, time.month,   time.day,
                            time.hour, time.minutes, time.seconds};
    for (size_t index = 0; index < 6; ++index) {
        AbSpan path[] = {span("giftBoxSessionStartTime"), span(keys[index])};
        CHECK(ab_settings_statistics_write(fixture->settings, path, 2,
                                           number(fields[index]), NULL));
    }
    return true;
}

static bool reset_state(void) {
    Fixture fixture;
    CHECK(open_fixture(&fixture));
    CHECK(!fixture.calls && !ab_settings_statistics_root(fixture.settings)->count);
    CHECK(put(&fixture, "giftBoxTotalNumberForSession", number(9)) &&
          put(&fixture, "giftBoxCurrentSpawnLevel", text("Level57")));
    CHECK(ab_giftbox_session_increase_attempt(&fixture.session, NULL));
    fixture.nested = true;
    CHECK(ab_giftbox_session_update(&fixture.session, NULL) && fixture.calls == 2);
    const AbDataTable *root = ab_settings_statistics_root(fixture.settings);
    AbDataValue saved = get(root, "giftBoxSessionStartTime");
    CHECK(saved.kind == AB_DATA_TABLE && saved.table->count == 6 &&
          equal(get(saved.table, "minutes"), 1) &&
          equal(get(saved.table, "seconds"), 0));
    CHECK(equal(get(root, "giftBoxTotalNumberForSession"), 9) &&
          get(root, "giftBoxCurrentSpawnLevel").kind == AB_DATA_STRING &&
          equal(get(root, "giftBoxCurrentNumberForSession"), 1) &&
          equal(get(root, "levelTryAmountForCurrentGiftBox"), 0));
    AbDataValue spawned = get(root, "giftBoxSpawnedForLevels"),
                claimed = get(root, "giftBoxClaimedForLevels");
    CHECK(spawned.kind == AB_DATA_TABLE && claimed.kind == AB_DATA_TABLE &&
          spawned.table != claimed.table && spawned.table->count == 0);
    CHECK(ab_giftbox_session_increase_attempt(&fixture.session, NULL) &&
          equal(get(root, "levelTryAmountForCurrentGiftBox"), 1));
    fixture.calls = 0;
    fixture.times[0] = fixture.times[1];
    CHECK(ab_giftbox_session_update(&fixture.session, NULL) && fixture.calls == 1 &&
          get(root, "giftBoxSessionStartTime").table == saved.table);
    fixture.calls = 0;
    fixture.times[0].day += 1;
    CHECK(ab_giftbox_session_update(&fixture.session, NULL) && fixture.calls == 2 &&
          get(root, "giftBoxSessionStartTime").table != saved.table &&
          equal(get(saved.table, "minutes"), 1));
    ab_settings_statistics_close(fixture.settings);
    return true;
}

static bool boundary_cases(void) {
    for (unsigned sample = 0; sample < 7; ++sample) {
        Fixture fixture;
        CHECK(open_fixture(&fixture));
        AbCalendarTime start = {2026, 10, 22, 0, 0, 0};
        CHECK(date(&fixture, start));
        fixture.times[0] = start;
        fixture.times[0].hour = sample == 0 ? 6 : sample == 2 ? -7 : 7;
        fixture.times[0].minutes = sample == 0 ? 59 : 0;
        fixture.times[0].seconds = sample == 0 ? 59 : 0;
        if (sample == 3) {
            fixture.mutate_saved = true;
            fixture.times[0].hour = 9;
        } else if (sample == 4) {
            fixture.fail_call = 2;
        } else if (sample == 5) {
            CHECK(put(&fixture, "giftBoxSessionStartTime",
                      (AbDataValue){.kind = AB_DATA_BOOLEAN}));
        } else if (sample == 6) {
            fixture.times[0].seconds = NAN;
        }
        bool okay = ab_giftbox_session_update(&fixture.session, NULL);
        CHECK(okay == (sample != 4 && sample != 6));
        CHECK(fixture.calls ==
              ((sample == 1 || sample == 2 || sample == 4 || sample == 5) ? 2u : 1u));
        const AbDataTable *root = ab_settings_statistics_root(fixture.settings);
        if (sample == 4 || sample == 6)
            CHECK(!ab_giftbox_session_increase_attempt(&fixture.session, NULL));
        else if (fixture.calls == 2)
            CHECK(equal(get(root, "levelTryAmountForCurrentGiftBox"), 0));
        else
            CHECK(get(root, "levelTryAmountForCurrentGiftBox").kind == AB_DATA_NIL);
        ab_settings_statistics_close(fixture.settings);
    }
    return true;
}

static bool configured_cases(void) {
    for (unsigned sample = 0; sample < 6; ++sample) {
        Fixture fixture;
        CHECK(open_fixture(&fixture));
        CHECK(date(&fixture, (AbCalendarTime){2026, 10, 22, 0, 0, 0}));
        fixture.times[0] = (AbCalendarTime){2026, 10, 22, 6, 0, 0};
        AbDataEntry limit_entries[] = {{text("day"), number(0)},
                                       {text("hour"), number(6)},
                                       {text("minutes"), number(0)},
                                       {text("seconds"), number(1)}};
        AbDataTable limits = {.entries = limit_entries, .count = 4, .capacity = 4};
        AbDataEntry time_entry = {text("session_time"),
                                  {.kind = AB_DATA_TABLE, .table = &limits}};
        AbDataTable gift = {.entries = &time_entry, .count = 1, .capacity = 1};
        AbDataEntry gift_entry = {text("giftbox"),
                                  {.kind = AB_DATA_TABLE, .table = &gift}};
        AbDataTable remote = {.entries = &gift_entry, .count = 1, .capacity = 1};
        fixture.remote = (AbDataValue){.kind = AB_DATA_TABLE, .table = &remote};
        if (sample == 1)
            fixture.times[0].seconds = 1;
        else if (sample == 2 || sample == 3) {
            limit_entries[1].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
            if (sample == 3)
                fixture.times[0].day += 1;
        } else if (sample == 4)
            fixture.remote = number(0);
        else if (sample == 5)
            limit_entries[1].value = (AbDataValue){0};
        bool okay = ab_giftbox_session_update(&fixture.session, NULL);
        CHECK(okay == (sample != 2 && sample != 4));
        CHECK(fixture.calls == (sample == 4                  ? 0u
                                : sample == 1 || sample == 3 ? 2u
                                                             : 1u));
        ab_settings_statistics_close(fixture.settings);
    }
    return true;
}

static bool attempts(void) {
    Fixture fixture;
    CHECK(open_fixture(&fixture));
    CHECK(put(&fixture, "levelTryAmountForCurrentGiftBox",
              (AbDataValue){.kind = AB_DATA_BOOLEAN}));
    CHECK(ab_giftbox_session_increase_attempt(&fixture.session, NULL) &&
          ab_giftbox_session_increase_attempt(&fixture.session, NULL));
    const AbDataTable *root = ab_settings_statistics_root(fixture.settings);
    CHECK(equal(get(root, "levelTryAmountForCurrentGiftBox"), 2));
    CHECK(put(&fixture, "levelTryAmountForCurrentGiftBox", number(16777216)) &&
          ab_giftbox_session_increase_attempt(&fixture.session, NULL) &&
          equal(get(root, "levelTryAmountForCurrentGiftBox"), 16777216));
    CHECK(put(&fixture, "levelTryAmountForCurrentGiftBox", text("bad")) &&
          !ab_giftbox_session_increase_attempt(&fixture.session, NULL) &&
          get(root, "levelTryAmountForCurrentGiftBox").kind == AB_DATA_STRING &&
          !fixture.calls);
    ab_settings_statistics_close(fixture.settings);
    return true;
}

int main(void) {
    const char *zone = getenv("TZ");
    if (!zone || strcmp(zone, "UTC0"))
        return 2;
    if (!reset_state() || !boundary_cases() || !configured_cases() || !attempts())
        return 1;
    puts("Gift-box local sessions preserve clock, threshold and saved-state "
         "boundaries.");
    return 0;
}
