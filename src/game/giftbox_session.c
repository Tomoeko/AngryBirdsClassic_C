#include "ab/giftbox_session.h"
#include "ab/data_order.h"

#include <math.h>
#include <string.h>

static const char *const date_fields[] = {"year", "month",   "day",
                                          "hour", "minutes", "seconds"};

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric reached gift-box field");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite reached gift-box field");
}

static AbDataValue get(const AbDataTable *table, const char *key) {
    const AbDataValue *value = ab_data_get(table, text(key));
    return value ? *value : (AbDataValue){0};
}

static bool field(AbDataValue parent, const char *key, AbDataValue *output,
                  AbError *error) {
    if (parent.kind != AB_DATA_TABLE || !parent.table)
        return ab_fail(error, "Reached gift-box field requires a table");
    *output = get(parent.table, key);
    return true;
}

static bool write(AbGiftBoxSession *session, const char *key, AbDataValue value,
                  AbError *error) {
    AbSpan path = span(key);
    return ab_settings_statistics_write(session->settings, &path, 1, value, error);
}

static bool begin(AbGiftBoxSession *session, AbError *error) {
    if (!session || !session->initialized || session->busy || session->failed)
        return ab_fail(error, "Invalid or stopped gift-box session");
    session->busy = true;
    return true;
}

static bool finish(AbGiftBoxSession *session, bool okay) {
    session->busy = false;
    session->failed = session->failed || !okay;
    return okay;
}

bool ab_giftbox_session_initialize(AbGiftBoxSession *session,
                                   AbSettingsStatistics *settings,
                                   const AbDataValue *remote_config,
                                   AbSessionClock clock, AbError *error) {
    if (!session || !settings || !remote_config)
        return ab_fail(error, "Missing gift-box session binding");
    *session = (AbGiftBoxSession){.settings = settings,
                                  .remote_config = remote_config,
                                  .clock = clock,
                                  .initialized = true};
    return true;
}

bool ab_giftbox_session_increase_attempt(AbGiftBoxSession *session, AbError *error) {
    if (!begin(session, error))
        return false;
    AbDataValue value = get(ab_settings_statistics_root(session->settings),
                            "levelTryAmountForCurrentGiftBox");
    float previous = 0;
    bool okay =
        (!truthy(value) || numeric(value, &previous, error)) &&
        isfinite(previous + 1) &&
        write(session, "levelTryAmountForCurrentGiftBox", number(previous + 1), error);
    return finish(session, okay);
}

static bool thresholds(AbGiftBoxSession *session, AbDataValue output[4],
                       AbError *error) {
    const char *const keys[] = {"day", "hour", "minutes", "seconds"};
    AbDataValue config = *session->remote_config;
    output[0] = output[2] = output[3] = number(0);
    output[1] = number(7);
    if (!truthy(config))
        return true;
    if (!field(config, "giftbox", &config, error))
        return false;
    if (!truthy(config))
        return true;
    if (!field(config, "session_time", &config, error))
        return false;
    if (!truthy(config))
        return true;
    for (size_t index = 0; index < 4; ++index) {
        AbDataValue value = {0};
        if (!field(config, keys[index], &value, error))
            return false;
        if (value.kind != AB_DATA_NIL)
            output[index] = value;
    }
    return true;
}

static bool current_time(AbGiftBoxSession *session, AbCalendarTime *output,
                         AbError *error) {
    return session->clock.now
               ? session->clock.now(session->clock.context, output, error)
               : ab_fail(error,
                         "Reached gift-box session requires a calendar provider");
}

static bool saved_time(AbDataValue value, AbCalendarTime *output, AbError *error) {
    if (!truthy(value)) {
        *output = (AbCalendarTime){2000, 1, 1, 0, 0, 0};
        return true;
    }
    float fields[6];
    for (size_t index = 0; index < 6; ++index) {
        AbDataValue item = {0};
        if (!field(value, date_fields[index], &item, error) ||
            !numeric(item, fields + index, error))
            return false;
    }
    *output = (AbCalendarTime){fields[0], fields[1], fields[2],
                               fields[3], fields[4], fields[5]};
    return true;
}

static bool should_reset(AbCalendarDifference parts, const AbDataValue limits[4],
                         bool *output, AbError *error) {
    const float values[] = {parts.days, parts.hours, parts.minutes, parts.seconds};
    for (size_t index = 0; index < 4; ++index) {
        float limit;
        if (!numeric(limits[index], &limit, error))
            return false;
        if (values[index] > limit || index == 3) {
            *output = values[index] >= limit;
            return true;
        }
        if (values[index] < limit) {
            *output = false;
            return true;
        }
    }
    return ab_fail(error, "Invalid gift-box threshold traversal");
}

static bool reset(AbGiftBoxSession *session, AbCalendarTime time, AbError *error) {
    const float values[] = {time.year, time.month,   time.day,
                            time.hour, time.minutes, time.seconds};
    AbDataEntry entries[6];
    size_t indices[6], capacity;
    AbStringOrderNode nodes[8];
    for (size_t index = 0; index < 6; ++index) {
        if (!isfinite(values[index]))
            return ab_fail(error, "Nonfinite gift-box reset clock field");
        entries[index] = (AbDataEntry){text(date_fields[index]), number(values[index])};
    }
    AbDataTable source = {.entries = entries, .count = 6, .capacity = 6};
    AbStringOrderWorkspace order = {indices, 6, nodes, 8};
    if (!ab_data_string_order_growing(&source, &order, &capacity, error))
        return false;
    AbSpan path[2] = {span("giftBoxSessionStartTime"), {0}};
    if (!ab_settings_statistics_create_table(session->settings, path, 1, 0, error))
        return false;
    for (size_t index = 0; index < 6; ++index) {
        const AbDataEntry *entry = entries + indices[index];
        path[1] = entry->key.string;
        if (!ab_settings_statistics_write(session->settings, path, 2, entry->value,
                                          error))
            return false;
    }
    path[1] = span("seconds");
    if (!ab_settings_statistics_write(session->settings, path, 2, number(0), error) ||
        !write(session, "giftBoxCurrentNumberForSession", number(1), error))
        return false;
    path[0] = span("giftBoxSpawnedForLevels");
    if (!ab_settings_statistics_create_table(session->settings, path, 1, 0, error))
        return false;
    path[0] = span("giftBoxClaimedForLevels");
    return ab_settings_statistics_create_table(session->settings, path, 1, 0, error) &&
           write(session, "levelTryAmountForCurrentGiftBox", number(0), error);
}

bool ab_giftbox_session_update(AbGiftBoxSession *session, AbError *error) {
    if (!begin(session, error))
        return false;
    AbDataValue limits[4];
    bool okay = thresholds(session, limits, error), expired = false;
    if (okay) {
        AbDataValue saved = get(ab_settings_statistics_root(session->settings),
                                "giftBoxSessionStartTime");
        AbCalendarTime first, second;
        AbCalendarDifference parts;
        okay = current_time(session, &second, error) &&
               saved_time(saved, &first, error) &&
               ab_calendar_time_difference_parts(&first, &second, &parts, error) &&
               should_reset(parts, limits, &expired, error);
    }
    if (okay && expired) {
        AbCalendarTime time;
        okay = current_time(session, &time, error) && reset(session, time, error);
    }
    return finish(session, okay);
}
