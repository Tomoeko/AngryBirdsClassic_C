#include "ab/ui_eagle_services.h"
#include "ab/script_time.h"

#include <math.h>
#include <string.h>

static AbDataValue boolean(bool value) {
    return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = value ? 1 : 0};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue get(const AbDataTable *table, AbDataValue key) {
    const AbDataValue *value = ab_data_get(table, key);
    return value ? *value : (AbDataValue){0};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    return get(table, text(name));
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool literal(AbDataValue value, bool expected) {
    return value.kind == AB_DATA_BOOLEAN && (value.bits != 0) == expected;
}

static bool same(AbDataValue left, AbDataValue right) {
    AbDataEntry entry = {left, {.kind = AB_DATA_BOOLEAN}};
    AbDataTable table = {&entry, 1, 1, NULL};
    return ab_data_get(&table, right) != NULL;
}

static bool table(AbDataValue value, const AbDataTable **output, AbError *error) {
    if (value.kind != AB_DATA_TABLE || !value.table ||
        value.table->count > value.table->capacity ||
        (value.table->count && !value.table->entries))
        return ab_fail(error, "Eagle query indexes a non-table source value");
    *output = value.table;
    return true;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind == AB_DATA_NUMBER) {
        memcpy(output, &value.bits, sizeof(*output));
        if (isfinite(*output))
            return true;
    }
    return ab_fail(error, "Eagle query requires a finite numeric source value");
}

static bool valid(const AbUiEagleServices *services, AbError *error) {
    return (services && services->progress.globals) ||
           ab_fail(error, "Missing canonical Eagle service globals");
}

static bool free_enabled(AbUiEagleServices *services, AbDataValue *output,
                         AbError *error) {
    bool result = false;
    if (!ab_episode_progress_free_eagle(&services->progress, &result, error))
        return false;
    *output = boolean(result);
    return true;
}

static bool setting(const AbDataTable *settings, const char *name, AbDataValue *output,
                    AbError *error) {
    if (!settings)
        return ab_fail(error, "Missing actual reached Eagle settings table");
    AbDataValue value = field(settings, name);
    *output = truthy(value) ? value : boolean(false);
    return true;
}

static bool enabled(AbUiEagleServices *services, AbDataValue *output, AbError *error) {
    if (truthy(field(services->progress.globals, "Sugotoku"))) {
        *output = boolean(true);
        return true;
    }
    AbDataValue result = {0};
    if (!setting(services->progress.purchases, "mightyEagleEnabled", &result, error))
        return false;
    if (!truthy(result) && !free_enabled(services, &result, error))
        return false;
    *output = result;
    return true;
}

static bool trial(AbUiEagleServices *services, AbDataValue *output, AbError *error) {
    const AbDataTable *globals = services->progress.globals;
    if (truthy(field(globals, "g_registrationEnabled"))) {
        *output = boolean(!truthy(field(globals, "g_isRegistered")));
        return true;
    }
    AbDataValue gamer = field(globals, "gamerService");
    if (truthy(gamer)) {
        if (gamer.kind != AB_DATA_TABLE && gamer.kind != AB_DATA_OBJECT)
            return ab_fail(error, "Eagle query indexes an invalid gamerService");
        return services->gamer_trial
                   ? services->gamer_trial(services->context, output, error)
                   : ab_fail(error, "Missing actual gamerService.isTrial");
    }
    AbDataValue marketplace = field(globals, "marketplace");
    if (truthy(marketplace)) {
        if (marketplace.kind != AB_DATA_TABLE && marketplace.kind != AB_DATA_OBJECT)
            return ab_fail(error, "Eagle query indexes an invalid marketplace");
        return services->marketplace_trial
                   ? services->marketplace_trial(services->context, output, error)
                   : ab_fail(error, "Missing actual marketplace.isTrial");
    }
    *output = boolean(false);
    return true;
}

bool ab_ui_eagle_services_trial(AbUiEagleServices *services, AbDataValue *output,
                                AbError *error) {
    if (!output || !valid(services, error))
        return ab_fail(error, "Missing actual trial query binding/output");
    return trial(services, output, error);
}

static bool show(AbUiEagleServices *services, AbDataValue *output, AbError *error) {
    AbDataValue result = {0};
    if (!enabled(services, &result, error))
        return false;
    if (!truthy(result))
        result = field(services->progress.globals, "hasIaps");
    if (truthy(result)) {
        /* isIapEnabled returns the same live raw hasIaps field. */
        if (truthy(field(services->progress.globals, "hasIaps"))) {
            if (!trial(services, &result, error))
                return false;
            result = boolean(!truthy(result));
        } else
            result = field(services->progress.globals, "hasIaps");
    }
    *output = result;
    return true;
}

static bool current_level(AbUiEagleServices *services,
                          const AbClassicLevelRequest **output, AbError *error) {
    const AbClassicLevelRequest *level = services->level;
    if (level && !same(level->name, field(services->progress.globals, "levelName")))
        return ab_fail(error, "Eagle query has a stale current-level binding");
    *output = level;
    return true;
}

static bool disabled(AbUiEagleServices *services, AbDataValue *output, AbError *error) {
    const AbDataTable *globals = services->progress.globals;
    if (truthy(field(globals, "g_eagle_disabled")) ||
        truthy(field(globals, "g_is_free_version"))) {
        *output = boolean(true);
        return true;
    }
    if (truthy(field(globals, "startedFromEditor"))) {
        *output = boolean(false);
        return true;
    }
    const AbClassicLevelRequest *level = NULL;
    if (!current_level(services, &level, error))
        return false;
    bool result = true;
    if (level) {
        if (!level->episode || !level->page || !level->level)
            return ab_fail(error, "Invalid reached Eagle level metadata");
        result = truthy(field(level->episode, "mighty_eagle_disabled"));
        AbDataValue override = field(level->page, "mighty_eagle_disabled");
        if (truthy(override))
            result = true;
        else if (literal(override, false))
            result = false;
        override = field(level->level, "mighty_eagle_disabled");
        if (truthy(override))
            result = true;
        else if (literal(override, false))
            result = false;
    }
    *output = boolean(result);
    return true;
}

bool ab_ui_eagle_services_power(void *context, const AbDataTable **output,
                                AbError *error) {
    AbUiEagleServices *services = context;
    if (!output || !valid(services, error))
        return ab_fail(error, "Invalid Eagle episode-power lookup");
    const AbClassicLevelRequest *level = NULL;
    if (!current_level(services, &level, error))
        return false;
    const AbDataTable *power = NULL;
    if (level) {
        if (!level->episode)
            return ab_fail(error, "Missing reached Eagle episode metadata");
        AbDataValue value = field(level->episode, "mightyPower");
        if (truthy(value) && !table(value, &power, error))
            return false;
    }
    *output = power;
    return true;
}

bool ab_ui_eagle_services_used(void *context, AbUiPointerValue free_eagle,
                               AbDataValue *output, AbError *error) {
    AbUiEagleServices *services = context;
    if (!output || !valid(services, error) || !services->progress.local_settings ||
        !ab_ui_pointer_value_valid(&free_eagle))
        return ab_fail(error, "Missing actual Eagle used-time settings binding");
    *output = field(services->progress.local_settings,
                    ab_ui_pointer_value_truthy(&free_eagle) ? "freeEagleUsedTime"
                                                            : "eagleUsedTime");
    return true;
}

static bool used_time(AbUiEagleServices *services, AbDataValue free_eagle,
                      AbDataValue *output, AbError *error) {
    if (!services->progress.local_settings)
        return ab_fail(error, "Missing actual Eagle usage settings");
    *output = field(services->progress.local_settings,
                    truthy(free_eagle) ? "freeEagleUsedTime" : "eagleUsedTime");
    return true;
}

static bool has_used(AbUiEagleServices *services, AbDataValue free_eagle, bool *output,
                     AbError *error) {
    if (!services->progress.local_settings)
        return ab_fail(error, "Missing actual Eagle used-in settings");
    AbDataValue used = field(services->progress.local_settings,
                             truthy(free_eagle) ? "freeEaglesUsedIn" : "eaglesUsedIn");
    if (!truthy(used)) {
        *output = false;
        return true;
    }
    if (used.kind == AB_DATA_STRING) {
        if (used.string.size && !used.string.data)
            return ab_fail(error, "Invalid Eagle used-in string");
        *output = used.string.size != 0;
        return true;
    }
    const AbDataTable *array = NULL;
    if (!table(used, &array, error))
        return false;
    /* Only zero versus nonzero length is observed, but reject sparse positive
     * integer arrays rather than guessing a Lua table border. */
    float first = 1;
    uint32_t bits;
    memcpy(&bits, &first, sizeof(bits));
    bool present =
        get(array, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits}).kind !=
        AB_DATA_NIL;
    if (!present) {
        for (size_t index = 0; index < array->count; ++index) {
            AbDataEntry entry = array->entries[index];
            if (entry.key.kind == AB_DATA_NUMBER && entry.value.kind != AB_DATA_NIL) {
                float key;
                if (!numeric(entry.key, &key, error))
                    return false;
                if (key >= 1 && truncf(key) == key)
                    return ab_fail(error, "Sparse Eagle used-in array unsupported");
            }
        }
    }
    *output = present;
    return true;
}

static bool unfinished(AbUiEagleServices *services, bool *output, AbError *error) {
    const AbDataTable *globals = services->progress.globals, *scores = NULL;
    if (!table(field(globals, "highscores"), &scores, error))
        return false;
    AbDataValue value = get(scores, field(globals, "levelName"));
    if (value.kind == AB_DATA_NIL) {
        *output = true;
        return true;
    }
    const AbDataTable *record = NULL;
    if (!table(value, &record, error))
        return false;
    float zero = 0;
    uint32_t bits;
    memcpy(&bits, &zero, sizeof(bits));
    bool zero_score = same(field(record, "score"),
                           (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits});
    *output = (zero_score && !literal(field(record, "completed"), true)) ||
              field(record, "eagleScore").kind == AB_DATA_NIL;
    return true;
}

static bool elapsed_time(AbUiEagleServices *services, AbDataValue used, float *output,
                         AbError *error) {
    if (!services->clock.now)
        return ab_fail(error, "Missing actual Eagle availability clock");
    AbCalendarTime now = {0}, date = {0};
    if (!services->clock.now(services->clock.context, &now, error))
        return false;
    const AbDataTable *record = NULL;
    if (!table(used, &record, error))
        return false;
    const char *const names[] = {"day", "hour", "minutes", "seconds"};
    float *const fields[] = {&date.day, &date.hour, &date.minutes, &date.seconds};
    for (size_t index = 0; index < 4; ++index)
        if (!numeric(field(record, names[index]), fields[index], error))
            return false;
    return ab_script_time_difference(&now, &date, output, error);
}

static bool available(AbUiEagleServices *services, AbDataValue *output,
                      AbError *error) {
    const AbDataTable *globals = services->progress.globals;
    if (truthy(field(globals, "g_powerups_enabled")) &&
        field(globals, "powerupManager").kind != AB_DATA_NIL) {
        AbDataValue manager = field(globals, "powerupManager");
        if (manager.kind != AB_DATA_TABLE && manager.kind != AB_DATA_OBJECT)
            return ab_fail(error, "Eagle query indexes an invalid PowerupManager");
        AbDataValue transitioning;
        if (!services->powerup_transitioning)
            return ab_fail(error, "Missing actual PowerupManager.isTransitioning");
        if (!services->powerup_transitioning(services->context, &transitioning, error))
            return false;
        if (truthy(transitioning)) {
            *output = boolean(false);
            return true;
        }
    }
    AbDataValue free_eagle = {0};
    bool used = false;
    if (!free_enabled(services, &free_eagle, error) ||
        !has_used(services, free_eagle, &used, error))
        return false;
    if (!used) {
        *output = boolean(true);
        return true;
    }
    AbDataValue date = {0};
    if (!free_enabled(services, &free_eagle, error) ||
        !used_time(services, free_eagle, &date, error))
        return false;
    if (date.kind == AB_DATA_NIL) {
        *output = boolean(true);
        return true;
    }
    bool needs_cooldown = false;
    float elapsed = 0, locked = 0;
    if (!unfinished(services, &needs_cooldown, error) ||
        !elapsed_time(services, date, &elapsed, error) ||
        !numeric(field(globals, "eagleLockedTime"), &locked, error))
        return false;
    bool expired = locked < elapsed;
    *output = boolean(!needs_cooldown || expired ||
                      truthy(field(globals, "eagleUsedInCurrentLevel")));
    return true;
}

static bool query(AbUiEagleServices *services, AbUiEagleQuery kind, AbDataValue *output,
                  AbError *error) {
    switch (kind) {
        case AB_UI_EAGLE_AVAILABLE:
            return available(services, output, error);
        case AB_UI_EAGLE_LEVEL_ENDING: {
            AbDataValue result =
                field(services->progress.globals, "g_level_completing");
            *output = truthy(result) ? result : boolean(false);
            return true;
        }
        case AB_UI_EAGLE_ENABLED:
        case AB_UI_EAGLE_SUBSYSTEM_ENABLED:
            /* subsystemsapi1298 delegates to the live common-game getter. */
            return enabled(services, output, error);
        case AB_UI_EAGLE_SHOW_ELEMENTS:
            return show(services, output, error);
        case AB_UI_EAGLE_LEVEL_DISABLED:
            return disabled(services, output, error);
        case AB_UI_EAGLE_CHILDLOCK:
            return setting(services->settings, "shopChildlockOn", output, error);
        case AB_UI_EAGLE_SETTINGS_ENABLED:
            return setting(services->progress.purchases, "mightyEagleEnabled", output,
                           error);
        case AB_UI_EAGLE_FREE_ENABLED:
            return free_enabled(services, output, error);
        default:
            return ab_fail(error, "Unsupported Eagle service query");
    }
}

bool ab_ui_eagle_services_query(void *context, AbUiEagleQuery kind,
                                AbUiPointerValue *output, AbError *error) {
    AbUiEagleServices *services = context;
    if (!output || !valid(services, error))
        return ab_fail(error, "Invalid Eagle query binding");
    AbDataValue value = {0};
    if (!query(services, kind, &value, error))
        return false;
    AbUiPointerValue result = {0};
    switch (value.kind) {
        case AB_DATA_NIL:
            break;
        case AB_DATA_BOOLEAN:
            result = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN,
                                        .boolean = value.bits != 0};
            break;
        case AB_DATA_NUMBER:
            result = (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                        .number_bits = value.bits};
            break;
        case AB_DATA_STRING:
            result =
                (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = value.string};
            break;
        case AB_DATA_TABLE:
            result = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT,
                                        .object = (uintptr_t)value.table};
            break;
        default:
            return ab_fail(error, "Unsupported actual Eagle result type");
    }
    if (!ab_ui_pointer_value_valid(&result))
        return ab_fail(error, "Invalid actual Eagle result");
    *output = result;
    return true;
}

bool ab_ui_eagle_services_bind(AbUiEagleServices *services, AbUiMightyEagleHost *host,
                               AbError *error) {
    if (!host || !valid(services, error))
        return ab_fail(error, "Missing actual Eagle host/service binding");
    host->globals = services->progress.globals;
    host->clock = services->clock;
    host->context = services;
    host->query = ab_ui_eagle_services_query;
    host->episode_power = ab_ui_eagle_services_power;
    host->used_time = ab_ui_eagle_services_used;
    return true;
}
