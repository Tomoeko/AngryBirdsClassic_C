#include "ab/classic_shot_listeners.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataTable table;
    AbDataEntry entries[3];
} ListenerPayload;

struct AbClassicShotListeners {
    AbClassicShotListenerConfig config;
    AbClassicShotListenerState state;
    AbEventListener achievement, flurry;
    ListenerPayload *payloads;
    size_t used, bytes;
    bool bound, dispatching;
};

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

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool named(AbDataValue value, const char *name) {
    AbSpan expected = span(name);
    return value.kind == AB_DATA_STRING && value.string.size == expected.size &&
           (!expected.size ||
            (value.string.data &&
             !memcmp(value.string.data, expected.data, expected.size)));
}

static bool equal_limit(AbDataValue value, AbDataValue limit) {
    if (value.kind != limit.kind)
        return false;
    if (value.kind == AB_DATA_NIL)
        return true;
    if (value.kind == AB_DATA_BOOLEAN)
        return (value.bits != 0) == (limit.bits != 0);
    if (value.kind == AB_DATA_NUMBER) {
        float a, b;
        memcpy(&a, &value.bits, sizeof(a));
        memcpy(&b, &limit.bits, sizeof(b));
        return a == b;
    }
    if (value.kind == AB_DATA_TABLE)
        return value.table == limit.table;
    return value.string.size == limit.string.size &&
           (!value.string.size ||
            (value.string.data && limit.string.data &&
             !memcmp(value.string.data, limit.string.data, value.string.size)));
}

static ListenerPayload *payload(AbClassicShotListeners *listeners, AbError *error) {
    if (listeners->used == listeners->config.notifications) {
        ab_fail(error, "Shot listener payload storage exhausted");
        return NULL;
    }
    ListenerPayload *result = listeners->payloads + listeners->used++;
    result->table = (AbDataTable){result->entries, 0, 3, NULL};
    return result;
}

static bool achievement_status(AbClassicShotListeners *listeners, const char *name,
                               AbDataTable **output, AbError *error) {
    AbSettingsStatistics *settings = listeners->config.settings;
    AbSpan path[] = {span("achievementStatus"), span(name)};
    AbDataValue root =
        field(ab_settings_statistics_root(settings), "achievementStatus");
    if (root.kind == AB_DATA_NIL) {
        if (!ab_settings_statistics_create_table(settings, path, 1, 0, error))
            return false;
        root = field(ab_settings_statistics_root(settings), "achievementStatus");
    }
    if (root.kind != AB_DATA_TABLE)
        return ab_fail(error, "Achievement status list is not a table");
    AbDataValue status = field(root.table, name);
    if (!truthy(status)) {
        if (!ab_settings_statistics_create_table(settings, path, 2, 1, error))
            return false;
        AbSpan unlocked[] = {path[0], path[1], span("unlocked")};
        if (!ab_settings_statistics_write(
                settings, unlocked, 3, (AbDataValue){.kind = AB_DATA_BOOLEAN}, error))
            return false;
        status = field(root.table, name);
    }
    if (status.kind != AB_DATA_TABLE)
        return ab_fail(error, "Achievement status is not a table");
    *output = status.table;
    return true;
}

static bool unlock(AbClassicShotListeners *listeners, const char *name,
                   AbError *error) {
    AbDataTable *status = NULL;
    if (!achievement_status(listeners, name, &status, error))
        return false;
    bool first = !truthy(field(status, "unlocked"));
    ListenerPayload *event = payload(listeners, error);
    if (!event)
        return false;
    event->entries[0] = (AbDataEntry){text("id"), text("EID_ACHIEVEMENT_UNLOCKED")};
    event->entries[1] = (AbDataEntry){text("name"), text(name)};
    event->entries[2] = (AbDataEntry){text("first_time_unlocked"),
                                      {.kind = AB_DATA_BOOLEAN, .bits = first}};
    event->table.count = 3;
    if (!ab_event_manager_notify(listeners->config.events, &event->table, error))
        return false;
    if (!first)
        return true;
    /* The original mark helper queries status again after notification. */
    if (!achievement_status(listeners, name, &status, error))
        return false;
    AbSpan path[] = {span("achievementStatus"), span(name), span("unlocked")};
    if (!ab_settings_statistics_write(listeners->config.settings, path, 3,
                                      (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1},
                                      error))
        return false;
    if (!listeners->config.host.save_settings)
        return ab_fail(error, "Missing settings save at first achievement unlock");
    if (!listeners->config.host.save_settings(
            listeners->config.host.context,
            ab_settings_statistics_root(listeners->config.settings), error))
        return false;
    if (listeners->state.ios.kind != AB_DATA_BOOLEAN || listeners->state.ios.bits)
        return ab_fail(error, "Unsupported non-Android achievement unlock queue");
    return true;
}

static bool achievement_event(void *context, AbDataTable *event, AbError *error) {
    AbClassicShotListeners *listeners = context;
    if (!listeners || listeners->dispatching ||
        !named(field(event, "id"), "EID_BIRD_SHOT"))
        return ab_fail(error, "Unsupported or reentrant achievement shot event");
    listeners->dispatching = true;
    bool okay = true;
    if (equal_limit(field(event, "birdsShooted"), listeners->state.bird_slinger_limit))
        okay = unlock(listeners, "Bird Slinger", error);
    if (okay && equal_limit(field(event, "backwardsBirdCount"), number(10)))
        okay = unlock(listeners, "Backward Compatibility", error);
    listeners->dispatching = false;
    return okay;
}

static bool analytics_session(AbClassicShotListeners *listeners, AbError *error) {
    AbSettingsStatistics *settings = listeners->config.settings;
    if (truthy(field(ab_settings_statistics_root(settings), "analyticsSessionId")))
        return true;
    float starts;
    if (!ab_settings_statistics_device_sum(settings, span("gameStarts"), &starts,
                                           error))
        return false;
    AbSpan key = span("analyticsSessionId");
    return ab_settings_statistics_write(settings, &key, 1, number(starts), error);
}

static bool flurry_shot(AbClassicShotListeners *listeners, AbError *error) {
    if (!analytics_session(listeners, error))
        return false;
    if (!isfinite(listeners->state.current_bird_index))
        return ab_fail(error, "Invalid analytics bird index");
    if (listeners->state.current_bird_index != 1)
        return true;
    if (!isfinite(listeners->state.score))
        return ab_fail(error, "Invalid analytics shot score");
    if (listeners->state.score <= 0)
        return true;
    AbClassicShotListenerHost *host = &listeners->config.host;
    AbDataValue value = {0};
    if (truthy(listeners->state.powerups_enabled)) {
        if (!host->powerup_used)
            return ab_fail(error, "Missing birdquake powerup query");
        if (!host->powerup_used(host->context, "birdquake", &value, error))
            return false;
        if (truthy(value))
            return true;
    }
    if (!host->level_type)
        return ab_fail(error, "Missing analytics level-type query");
    if (!host->level_type(host->context, listeners->state.level_name, &value, error))
        return false;
    if (named(value, "golden_egg"))
        return true;
    ListenerPayload *parameters = payload(listeners, error);
    if (!parameters)
        return false;
    if (!host->level_text)
        return ab_fail(error, "Missing analytics level-text query");
    AbDataValue level_text = {0};
    if (!host->level_text(host->context, listeners->state.level_name, &level_text,
                          error))
        return false;
    parameters->entries[0] = (AbDataEntry){text("level"), level_text};
    parameters->entries[1] =
        (AbDataEntry){text("score"), number(listeners->state.score)};
    parameters->table.count = 2;
    if (!truthy(listeners->state.analytics_present))
        return true;
    if (!host->analytics)
        return ab_fail(error, "Missing RovioAnalytics logEvent boundary");
    return host->analytics(host->context, "Score generated prior to player input",
                           &parameters->table, error);
}

static bool flurry_event(void *context, AbDataTable *event, AbError *error) {
    AbClassicShotListeners *listeners = context;
    if (!listeners || listeners->dispatching ||
        !named(field(event, "id"), "EID_BIRD_SHOT"))
        return ab_fail(error, "Unsupported or reentrant ClassicFlurry shot event");
    listeners->dispatching = true;
    bool okay = flurry_shot(listeners, error);
    listeners->dispatching = false;
    return okay;
}

bool ab_classic_shot_listeners_open(AbClassicShotListenerConfig config,
                                    AbClassicShotListeners **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing shot listener output");
    *output = NULL;
    if (!config.settings || !config.events ||
        config.storage_bytes < sizeof(AbClassicShotListeners) ||
        config.notifications > (config.storage_bytes - sizeof(AbClassicShotListeners)) /
                                   sizeof(ListenerPayload))
        return ab_fail(error, "Invalid shot listener bindings or byte limit");
    AbClassicShotListeners *listeners = calloc(1, sizeof(*listeners));
    if (!listeners)
        return ab_fail(error, "Cannot allocate shot listeners");
    listeners->config = config;
    listeners->bytes =
        sizeof(*listeners) + config.notifications * sizeof(ListenerPayload);
    if (config.notifications) {
        listeners->payloads =
            calloc(config.notifications, sizeof(*listeners->payloads));
        if (!listeners->payloads) {
            ab_classic_shot_listeners_close(listeners);
            return ab_fail(error, "Cannot allocate shot listener payloads");
        }
    }
    listeners->state.bird_slinger_limit = number(5000);
    listeners->achievement =
        (AbEventListener){span("UNDEFINED"), listeners, achievement_event};
    listeners->flurry = (AbEventListener){span("UNDEFINED"), listeners, flurry_event};
    *output = listeners;
    return true;
}

void ab_classic_shot_listeners_close(AbClassicShotListeners *listeners) {
    if (!listeners)
        return;
    free(listeners->payloads);
    free(listeners);
}

size_t ab_classic_shot_listeners_bytes(const AbClassicShotListeners *listeners) {
    return listeners ? listeners->bytes : 0;
}

AbClassicShotListenerState *
ab_classic_shot_listeners_state(AbClassicShotListeners *listeners) {
    return listeners ? &listeners->state : NULL;
}

bool ab_classic_shot_listeners_bind(AbClassicShotListeners *listeners, AbError *error) {
    if (!listeners || listeners->bound || listeners->dispatching)
        return ab_fail(error, "Invalid or repeated shot listener binding");
    if (!ab_event_manager_add(listeners->config.events, text("EID_BIRD_SHOT"),
                              &listeners->achievement, error))
        return false;
    /* A second registration failure retains the first, as in the original.
     * The session must discard that partially bound manager. */
    if (!ab_event_manager_add(listeners->config.events, text("EID_BIRD_SHOT"),
                              &listeners->flurry, error))
        return false;
    listeners->bound = true;
    return true;
}
