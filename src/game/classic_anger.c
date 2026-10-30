#include "ab/classic_anger.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataTable table;
    AbDataEntry entry;
} AngerNotice;

struct AbClassicAnger {
    AbClassicAngerConfig config;
    AbSessionTimer level_timer, cooldown_timer;
    AbCalendarTime level_restart;
    AbClassicAngerSettings *saves;
    AngerNotice *notices;
    AbEventListener listener;
    size_t save_count, notice_count, bytes;
    bool updating, initializing, saving, loading, registered, failed;
};

static const AbSessionDuration level_duration = {0, 5, 0};
static const AbSessionDuration cooldown_duration = {0, 30, 0};

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric reached AngerLevel field");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite reached AngerLevel field");
}

static bool valid(AbClassicAnger *anger, AbError *error) {
    return (anger && !anger->failed) || ab_fail(error, "Invalid or stopped AngerLevel");
}

static bool finish(AbClassicAnger *anger, bool okay) {
    anger->failed = anger->failed || !okay;
    return okay;
}

static bool listener_event(void *context, AbDataTable *event, AbError *error) {
    return ab_classic_anger_event(context, event, error);
}

static bool bind_timers(AbClassicAnger *anger, AbError *error) {
    AbClassicAngerState *state = anger->config.state;
    AbSessionTimerState level = state->level_timer, cooldown = state->cooldown_timer;
    bool okay =
        ab_session_timer_initialize(&anger->level_timer, &state->level_timer,
                                    anger->config.clock, error) &&
        ab_session_timer_initialize(&anger->cooldown_timer, &state->cooldown_timer,
                                    anger->config.clock, error);
    state->level_timer = level;
    state->cooldown_timer = cooldown;
    return okay;
}

bool ab_classic_anger_open(AbClassicAngerConfig config, AbClassicAnger **output,
                           AbError *error) {
    if (!output)
        return ab_fail(error, "Missing AngerLevel output");
    *output = NULL;
    size_t record_size = sizeof(AbClassicAngerSettings) + sizeof(AngerNotice);
    if (!config.state || !config.settings || !config.events || !config.records ||
        !config.iteration_limit ||
        config.records > (SIZE_MAX - sizeof(AbClassicAnger)) / record_size)
        return ab_fail(error, "Invalid AngerLevel bindings/capacity");
    size_t bytes = sizeof(AbClassicAnger) + config.records * record_size;
    if (bytes > config.storage_bytes)
        return ab_fail(error, "AngerLevel storage exceeds bound");
    AbClassicAnger *anger = calloc(1, bytes);
    if (!anger)
        return ab_fail(error, "Cannot allocate AngerLevel");
    anger->config = config;
    anger->bytes = bytes;
    anger->saves = (AbClassicAngerSettings *)(anger + 1);
    anger->notices = (AngerNotice *)(anger->saves + config.records);
    anger->listener =
        (AbEventListener){text("AngerLevel").string, anger, listener_event};
    if (!bind_timers(anger, error)) {
        free(anger);
        return false;
    }
    *output = anger;
    return true;
}

void ab_classic_anger_close(AbClassicAnger *anger) {
    if (!anger)
        return;
    if (anger->registered)
        (void)ab_event_manager_remove(anger->config.events,
                                      anger->config.load_level_late, &anger->listener,
                                      NULL);
    free(anger);
}

size_t ab_classic_anger_bytes(const AbClassicAnger *anger) {
    return anger ? anger->bytes : 0;
}

bool ab_classic_anger_load(AbClassicAnger *anger, AbError *error) {
    if (!valid(anger, error) || anger->loading)
        return ab_fail(error, "Invalid or recursive AngerLevel settings load");
    anger->loading = true;
    AbClassicAngerState *state = anger->config.state;
    const AbClassicAngerSettings *input = *anger->config.settings;
    bool okay = true;
    if (!input)
        state->level = state->maximum;
    else {
        state->queued = truthy(input->queued) ? input->queued : number(0);
        state->level = input->level;
        state->maximum = truthy(input->maximum) ? input->maximum : number(2);
        okay = ab_session_timer_load(&anger->level_timer, &input->timer, error) &&
               ab_session_timer_load(&anger->cooldown_timer, &input->cooldown, error);
        if (okay)
            state->charges = input->charges;
    }
    anger->loading = false;
    return finish(anger, okay);
}

bool ab_classic_anger_init(AbClassicAnger *anger, AbError *error) {
    if (!valid(anger, error) || anger->initializing)
        return ab_fail(error, "Invalid or recursive AngerLevel initialization");
    anger->initializing = true;
    AbClassicAngerState *state = anger->config.state;
    state->maximum = number(2);
    state->boost_cost = number(1);
    state->queued = number(0);
    state->level_timer = (AbSessionTimerState){0};
    bool okay = ab_session_timer_initialize(&anger->level_timer, &state->level_timer,
                                            anger->config.clock, error);
    if (okay) {
        state->charges = number(2);
        state->cooldown_timer = (AbSessionTimerState){0};
        okay =
            ab_session_timer_initialize(&anger->cooldown_timer, &state->cooldown_timer,
                                        anger->config.clock, error) &&
            ab_event_manager_add(anger->config.events, anger->config.load_level_late,
                                 &anger->listener, error);
    }
    if (okay) {
        anger->registered = true;
        okay = ab_classic_anger_load(anger, error);
    }
    if (okay)
        state->initialized = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    anger->initializing = false;
    return finish(anger, okay);
}

bool ab_classic_anger_save(AbClassicAnger *anger, AbError *error) {
    if (!valid(anger, error) || anger->saving)
        return ab_fail(error, "Invalid or recursive AngerLevel settings save");
    if (anger->save_count == anger->config.records)
        return finish(anger, ab_fail(error, "AngerLevel settings records exhausted"));
    anger->saving = true;
    AbClassicAngerState *state = anger->config.state;
    AbClassicAngerSettings *saved = &anger->saves[anger->save_count++];
    saved->level = state->level;
    saved->maximum = state->maximum;
    bool okay = ab_session_timer_save(&anger->level_timer, &saved->timer, error) &&
                ab_session_timer_save(&anger->cooldown_timer, &saved->cooldown, error);
    if (okay) {
        saved->charges = state->charges;
        saved->queued = state->queued;
        *anger->config.settings = saved;
        okay =
            anger->config.save_settings
                ? anger->config.save_settings(anger->config.context, error)
                : ab_fail(error, "AngerLevel requires settings persistence provider");
    }
    anger->saving = false;
    return finish(anger, okay);
}

static bool has_maximum(AbClassicAnger *anger, bool *output, AbError *error) {
    float level = 0, maximum = 0;
    if (!numeric(anger->config.state->level, &level, error) ||
        !numeric(anger->config.state->maximum, &maximum, error))
        return false;
    *output = level >= maximum;
    return true;
}

static bool notify_increased(AbClassicAnger *anger, AbError *error) {
    if (anger->notice_count == anger->config.records)
        return ab_fail(error, "AngerLevel event records exhausted");
    AngerNotice *notice = &anger->notices[anger->notice_count++];
    notice->entry = (AbDataEntry){text("id"), anger->config.level_increased};
    notice->table = (AbDataTable){.entries = &notice->entry, .count = 1, .capacity = 1};
    return ab_event_manager_notify(anger->config.events, &notice->table, error);
}

bool ab_classic_anger_add(AbClassicAnger *anger, float amount, AbError *error) {
    if (!valid(anger, error))
        return false;
    AbClassicAngerState *state = anger->config.state;
    float level = 0, maximum = 0;
    bool reached_maximum = false;
    bool okay = numeric(state->maximum, &maximum, error) &&
                numeric(state->level, &level, error) && isfinite(amount);
    if (okay) {
        float sum = level + amount;
        if (!isfinite(sum))
            okay = ab_fail(error, "Nonfinite AngerLevel addition");
        else {
            state->level = number(sum < maximum ? sum : maximum);
            okay = has_maximum(anger, &reached_maximum, error);
        }
    }
    if (okay && reached_maximum)
        okay = ab_session_timer_set_active(
            &anger->level_timer, (AbDataValue){.kind = AB_DATA_BOOLEAN}, error);
    if (okay)
        okay = notify_increased(anger, error) && ab_classic_anger_save(anger, error);
    return finish(anger, okay);
}

static bool in_level(AbClassicAnger *anger, AbDataValue *output, AbError *error) {
    return anger->config.is_in_level
               ? anger->config.is_in_level(anger->config.context, output, error)
               : ab_fail(error, "AngerLevel requires LevelFlow provider");
}

static bool refill_level(AbClassicAnger *anger, AbError *error) {
    AbDataValue playing = {0};
    if (!in_level(anger, &playing, error))
        return false;
    if (truthy(playing)) {
        float queued = 0;
        if (!numeric(anger->config.state->queued, &queued, error))
            return false;
        queued += 1;
        if (!isfinite(queued))
            return ab_fail(error, "Nonfinite queued AngerLevel refills");
        anger->config.state->queued = number(queued);
        if (!ab_classic_anger_save(anger, error))
            return false;
    } else if (!ab_classic_anger_add(anger, 1, error))
        return false;
    bool maximum;
    if (!has_maximum(anger, &maximum, error))
        return false;
    if (maximum)
        return ab_session_timer_set_active(
            &anger->level_timer, (AbDataValue){.kind = AB_DATA_BOOLEAN}, error);
    return ab_session_timer_end(&anger->level_timer, &anger->level_restart, error) &&
           ab_session_timer_start(&anger->level_timer, &level_duration,
                                  &anger->level_restart, error);
}

static bool refill_charge(AbClassicAnger *anger, AbError *error) {
    float charges = 0;
    if (!numeric(anger->config.state->charges, &charges, error))
        return false;
    if (charges < 1) {
        anger->config.state->charges = number(2);
        return ab_session_timer_set_active(
            &anger->cooldown_timer, (AbDataValue){.kind = AB_DATA_BOOLEAN}, error);
    }
    charges += 1;
    anger->config.state->charges = number(charges);
    if (charges >= 2)
        return ab_session_timer_set_active(
            &anger->cooldown_timer, (AbDataValue){.kind = AB_DATA_BOOLEAN}, error);
    return ab_session_timer_start(&anger->cooldown_timer, &cooldown_duration, NULL,
                                  error);
}

static bool update_timers(AbClassicAnger *anger, AbError *error) {
    AbSessionTimer *timers[] = {&anger->level_timer, &anger->cooldown_timer};
    size_t iterations = 0;
    for (unsigned which = 0; which < 2; ++which) {
        for (;;) {
            AbDataValue complete;
            if (!ab_session_timer_complete(timers[which], &complete, error))
                return false;
            if (!truthy(complete))
                break;
            if (iterations++ == anger->config.iteration_limit)
                return ab_fail(error, "AngerLevel catch-up exceeds iteration bound");
            if (!(which ? refill_charge(anger, error) : refill_level(anger, error)))
                return false;
        }
    }
    return true;
}

bool ab_classic_anger_update(AbClassicAnger *anger, AbError *error) {
    if (!valid(anger, error) || anger->updating)
        return ab_fail(error, "Invalid or recursive AngerLevel update");
    anger->updating = true;
    bool okay = (truthy(anger->config.state->initialized) ||
                 ab_classic_anger_init(anger, error)) &&
                update_timers(anger, error);
    anger->updating = false;
    return finish(anger, okay);
}

bool ab_classic_anger_activate_cooldown(AbClassicAnger *anger, AbError *error) {
    if (!valid(anger, error))
        return false;
    float charges = 0;
    bool okay = numeric(anger->config.state->charges, &charges, error);
    if (okay) {
        anger->config.state->charges = number(charges - 1);
        if (!truthy(anger->config.state->cooldown_timer.active))
            okay = ab_session_timer_start(&anger->cooldown_timer, &cooldown_duration,
                                          NULL, error);
    }
    return finish(anger, okay);
}

static bool matching_id(AbDataValue a, AbDataValue b) {
    if (a.kind != b.kind)
        return false;
    if (a.kind == AB_DATA_STRING)
        return a.string.size == b.string.size &&
               (!a.string.size || !memcmp(a.string.data, b.string.data, a.string.size));
    if (a.kind == AB_DATA_NUMBER) {
        float first, second;
        memcpy(&first, &a.bits, sizeof(first));
        memcpy(&second, &b.bits, sizeof(second));
        return first == second;
    }
    return a.kind == AB_DATA_BOOLEAN && !!a.bits == !!b.bits;
}

bool ab_classic_anger_event(AbClassicAnger *anger, AbDataTable *event, AbError *error) {
    if (!valid(anger, error) || !event)
        return ab_fail(error, "Invalid AngerLevel event");
    const AbDataValue *id = ab_data_get(event, text("id"));
    if (!id || !matching_id(*id, anger->config.load_level_late))
        return true;
    if (!anger->config.load_refill_popup)
        return finish(anger, ab_fail(error, "AngerLevel requires refill-popup owner"));
    bool okay = anger->config.load_refill_popup(anger->config.context, error);
    if (okay)
        anger->config.state->used = number(0);
    AbDataValue playing = {0};
    if (okay)
        okay = in_level(anger, &playing, error);
    if (okay && !truthy(playing) && truthy(anger->config.state->queued)) {
        float queued = 0;
        okay = numeric(anger->config.state->queued, &queued, error);
        if (okay && queued > 0) {
            okay = ab_classic_anger_add(anger, queued, error);
            if (okay) {
                anger->config.state->queued = number(0);
                okay = ab_classic_anger_save(anger, error);
            }
        }
    }
    return finish(anger, okay);
}
