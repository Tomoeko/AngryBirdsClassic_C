#include "ab/ui_eagle_services.h"
#include "ab/script_time.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Eagle service check failed at %d: %s\n", __LINE__,        \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue boolean(bool value) {
    return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = value ? 1 : 0};
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static bool put(AbDataTable *target, const char *name, AbDataValue value) {
    AbDataValue key = text(name);
    const AbDataValue *existing = ab_data_get(target, key);
    if (existing) {
        for (size_t index = 0; index < target->count; ++index)
            if (&target->entries[index].value == existing) {
                target->entries[index].value = value;
                return true;
            }
    }
    if (target->count == target->capacity)
        return false;
    target->entries[target->count++] = (AbDataEntry){key, value};
    return true;
}

typedef struct {
    AbCalendarTime now;
    size_t clocks, transitions, trials;
    AbDataValue transitioning, trial;
} Fixture;

static bool clock_now(void *context, AbCalendarTime *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    ++fixture->clocks;
    *output = fixture->now;
    return true;
}

static bool transition(void *context, AbDataValue *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    ++fixture->transitions;
    *output = fixture->transitioning;
    return true;
}

static bool trial(void *context, AbDataValue *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    ++fixture->trials;
    *output = fixture->trial;
    return true;
}

static bool result(AbUiEagleServices *services, AbUiEagleQuery kind, bool expected,
                   AbError *error) {
    AbUiPointerValue value;
    return ab_ui_eagle_services_query(services, kind, &value, error) &&
           value.kind == AB_UI_POINTER_BOOLEAN && value.boolean == expected;
}

static bool time_difference(AbError *error) {
    AbCalendarTime first = {NAN, NAN, 20, 10, 0, 30};
    AbCalendarTime second = {NAN, NAN, 20, 10, 0, 0};
    float value;
    CHECK(ab_script_time_difference(&first, &second, &value, error) && value == 30);
    first.day = 1;
    second.day = 31;
    first.hour = 0;
    second.hour = 23;
    second.minutes = 59;
    CHECK(ab_script_time_difference(&first, &second, &value, error) && value == 90);
    CHECK(ab_script_time_difference(&second, &first, &value, error) && value == 172710);
    first.day = 15;
    CHECK(ab_script_time_difference(&first, &second, &value, error) && value == 90);
    first.day = second.day;
    CHECK(ab_script_time_difference(&first, &second, &value, error) && value == -86310);
    first.minutes = INFINITY;
    value = 123;
    CHECK(!ab_script_time_difference(&first, &second, &value, NULL) && value == 123);
    return true;
}

static bool queries(AbError *error) {
    AbDataEntry global_entries[32], setting_entries[4], purchase_entries[4],
        local_entries[12], episode_entries[4], page_entries[4], level_entries[4],
        date_entries[4], score_entries[4], highscore_entries[4];
    AbDataTable globals = {global_entries, 0, 32, NULL};
    AbDataTable settings = {setting_entries, 0, 4, NULL};
    AbDataTable purchases = {purchase_entries, 0, 4, NULL};
    AbDataTable local = {local_entries, 0, 12, NULL};
    AbDataTable episode = {episode_entries, 0, 4, NULL};
    AbDataTable page = {page_entries, 0, 4, NULL};
    AbDataTable level = {level_entries, 0, 4, NULL};
    AbDataTable date = {date_entries, 0, 4, NULL};
    AbDataTable score = {score_entries, 0, 4, NULL};
    AbDataTable highscores = {highscore_entries, 0, 4, NULL};
    AbDataTable empty = {0};
    AbClassicLevelRequest request = {
        .episode = &episode, .page = &page, .level = &level, .name = text("Level1")};
    Fixture fixture = {.now = {2026, 10, 20, 11, 0, 0}};
    AbUiEagleServices services = {.progress = {.episodes = &empty,
                                               .local_settings = &local,
                                               .globals = &globals,
                                               .purchases = &purchases},
                                  .settings = &settings,
                                  .level = &request,
                                  .clock = {&fixture, clock_now},
                                  .context = &fixture,
                                  .powerup_transitioning = transition,
                                  .gamer_trial = trial,
                                  .marketplace_trial = trial};
    CHECK(put(&globals, "g_freeEagleUnlockStatus", table(&empty)) &&
          put(&globals, "levelName", text("Level1")) &&
          put(&globals, "highscores", table(&highscores)) &&
          put(&globals, "hasIaps", boolean(true)) &&
          put(&globals, "eagleLockedTime", number(3600)) &&
          result(&services, AB_UI_EAGLE_ENABLED, false, error) &&
          result(&services, AB_UI_EAGLE_SUBSYSTEM_ENABLED, false, error) &&
          result(&services, AB_UI_EAGLE_SHOW_ELEMENTS, true, error) &&
          result(&services, AB_UI_EAGLE_AVAILABLE, true, error) && !fixture.clocks);
    CHECK(put(&purchases, "mightyEagleEnabled", number(0)));
    AbUiPointerValue raw;
    CHECK(ab_ui_eagle_services_query(&services, AB_UI_EAGLE_ENABLED, &raw, error) &&
          raw.kind == AB_UI_POINTER_NUMBER && !raw.number_bits);
    CHECK(put(&globals, "g_registrationEnabled", boolean(true)) &&
          result(&services, AB_UI_EAGLE_SHOW_ELEMENTS, false, error) &&
          put(&globals, "g_isRegistered", number(0)) &&
          result(&services, AB_UI_EAGLE_SHOW_ELEMENTS, true, error));
    CHECK(put(&globals, "g_registrationEnabled", boolean(false)) &&
          put(&globals, "gamerService", table(&empty)) &&
          result(&services, AB_UI_EAGLE_SHOW_ELEMENTS, true, error) &&
          fixture.trials == 1);
    fixture.trial = number(0);
    CHECK(result(&services, AB_UI_EAGLE_SHOW_ELEMENTS, false, error));
    services.gamer_trial = NULL;
    CHECK(
        !ab_ui_eagle_services_query(&services, AB_UI_EAGLE_SHOW_ELEMENTS, &raw, NULL));
    CHECK(put(&globals, "gamerService", boolean(false)) &&
          put(&settings, "shopChildlockOn", number(0)) &&
          ab_ui_eagle_services_query(&services, AB_UI_EAGLE_CHILDLOCK, &raw, error) &&
          raw.kind == AB_UI_POINTER_NUMBER && !raw.number_bits);
    CHECK(put(&episode, "mighty_eagle_disabled", boolean(true)) &&
          result(&services, AB_UI_EAGLE_LEVEL_DISABLED, true, error) &&
          put(&page, "mighty_eagle_disabled", boolean(false)) &&
          result(&services, AB_UI_EAGLE_LEVEL_DISABLED, false, error) &&
          put(&level, "mighty_eagle_disabled", number(0)) &&
          result(&services, AB_UI_EAGLE_LEVEL_DISABLED, true, error) &&
          put(&globals, "startedFromEditor", number(0)) &&
          result(&services, AB_UI_EAGLE_LEVEL_DISABLED, false, error) &&
          put(&globals, "g_eagle_disabled", number(0)) &&
          result(&services, AB_UI_EAGLE_LEVEL_DISABLED, true, error));
    CHECK(put(&local, "eaglesUsedIn", text("Level1")) &&
          result(&services, AB_UI_EAGLE_AVAILABLE, true, error) && !fixture.clocks);
    CHECK(put(&date, "day", number(20)) && put(&date, "hour", number(10)) &&
          put(&date, "minutes", number(0)) && put(&date, "seconds", number(0)) &&
          put(&local, "eagleUsedTime", table(&date)) &&
          result(&services, AB_UI_EAGLE_AVAILABLE, false, error) &&
          fixture.clocks == 1);
    fixture.now.seconds = 1;
    CHECK(result(&services, AB_UI_EAGLE_AVAILABLE, true, error));
    fixture.now.seconds = 0;
    CHECK(put(&globals, "eagleUsedInCurrentLevel", number(0)) &&
          result(&services, AB_UI_EAGLE_AVAILABLE, true, error) &&
          put(&globals, "eagleUsedInCurrentLevel", boolean(false)) &&
          put(&score, "score", number(0)) && put(&score, "completed", boolean(true)) &&
          put(&score, "eagleScore", number(0)) &&
          put(&highscores, "Level1", table(&score)) &&
          result(&services, AB_UI_EAGLE_AVAILABLE, true, error));
    CHECK(put(&score, "completed", number(0)) &&
          result(&services, AB_UI_EAGLE_AVAILABLE, false, error));
    fixture.now.day = 19;
    CHECK(result(&services, AB_UI_EAGLE_AVAILABLE, true, error));
    fixture.now.day = 20;
    CHECK(put(&globals, "g_powerups_enabled", boolean(true)) &&
          put(&globals, "powerupManager", table(&empty)));
    fixture.transitioning = number(0);
    size_t clocks = fixture.clocks;
    CHECK(result(&services, AB_UI_EAGLE_AVAILABLE, false, error) &&
          fixture.transitions == 1 && fixture.clocks == clocks);
    services.powerup_transitioning = NULL;
    CHECK(!ab_ui_eagle_services_query(&services, AB_UI_EAGLE_AVAILABLE, &raw, NULL));
    CHECK(put(&local, "freeEagleUsedTime", number(123)));
    AbDataValue used;
    CHECK(ab_ui_eagle_services_used(&services,
                                    (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER},
                                    &used, error) &&
          used.kind == AB_DATA_NUMBER && used.bits == number(123).bits);
    CHECK(put(&episode, "mightyPower", table(&empty)));
    const AbDataTable *power;
    CHECK(ab_ui_eagle_services_power(&services, &power, error) && power == &empty);
    CHECK(put(&globals, "levelName", text("Level57")) &&
          !ab_ui_eagle_services_power(&services, &power, NULL));
    return true;
}

int main(void) {
    AbError error = {0};
    if (!time_difference(&error) || !queries(&error)) {
        fprintf(stderr, "%s\n", error.message);
        return 1;
    }
    puts(
        "Eagle services preserve live settings, eligibility, raw flags and cooldowns.");
    return 0;
}
