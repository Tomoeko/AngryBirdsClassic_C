#include "ab/session_timer.h"
#include "ab/time_format.h"

#include <math.h>
#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric reached session timer field");
    memcpy(output, &value.bits, sizeof(*output));
    return true;
}

static bool begin(AbSessionTimer *timer, AbError *error) {
    if (!timer || !timer->initialized || !timer->state || timer->busy || timer->failed)
        return ab_fail(error, "Invalid or stopped session timer");
    timer->busy = true;
    return true;
}

static bool finish(AbSessionTimer *timer, bool okay) {
    timer->busy = false;
    timer->failed = timer->failed || !okay;
    return okay;
}

bool ab_session_timer_initialize(AbSessionTimer *timer, AbSessionTimerState *state,
                                 AbSessionClock clock, AbError *error) {
    if (!timer || !state)
        return ab_fail(error, "Missing session timer binding");
    *timer = (AbSessionTimer){.state = state, .clock = clock, .initialized = true};
    state->active = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    return true;
}

bool ab_session_timer_init(AbSessionTimer *timer, AbError *error) {
    if (!begin(timer, error))
        return false;
    timer->state->active = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    return finish(timer, true);
}

bool ab_session_timer_force_complete(AbSessionTimer *timer, AbError *error) {
    if (!begin(timer, error))
        return false;
    timer->state->force_completed = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    return finish(timer, true);
}

static void set_active(AbSessionTimerState *state, AbDataValue active) {
    state->active = active;
    if (!truthy(active)) {
        state->start = NULL;
        state->duration = NULL;
        state->duration_seconds = (AbDataValue){0};
    }
}

bool ab_session_timer_set_active(AbSessionTimer *timer, AbDataValue active,
                                 AbError *error) {
    if (!begin(timer, error))
        return false;
    set_active(timer->state, active);
    return finish(timer, true);
}

bool ab_session_clock_platform(void *context, AbCalendarTime *output, AbError *error) {
    (void)context;
    return ab_calendar_time_now(output, error);
}

static bool current_time(AbSessionTimer *timer, AbCalendarTime *output,
                         AbError *error) {
    if (!timer->clock.now)
        return ab_fail(error, "Reached session timer requires a calendar provider");
    return timer->clock.now(timer->clock.context, output, error);
}

static bool start_timer(AbSessionTimer *timer, const AbSessionDuration *duration,
                        const AbCalendarTime *start, AbError *error) {
    if (!start) {
        AbCalendarTime current;
        if (!current_time(timer, &current, error))
            return false;
        timer->owned_start = current;
        start = &timer->owned_start;
    }
    AbSessionTimerState *state = timer->state;
    state->start = start;
    state->duration = duration;
    if (!duration)
        return ab_fail(error, "Missing reached session timer duration");
    float hours = duration->hours * 3600;
    float minutes = duration->minutes * 60;
    float total = hours + minutes;
    total += duration->seconds;
    state->duration_seconds = number(total);
    set_active(state, (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1});
    return true;
}

bool ab_session_timer_start(AbSessionTimer *timer, const AbSessionDuration *duration,
                            const AbCalendarTime *start, AbError *error) {
    if (!begin(timer, error))
        return false;
    return finish(timer, start_timer(timer, duration, start, error));
}

static bool remaining(AbSessionTimer *timer, AbDataValue *output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing session timer remaining output");
    if (!truthy(timer->state->active)) {
        *output = (AbDataValue){0};
        return true;
    }
    AbCalendarTime current;
    float elapsed = 0, duration = 0;
    if (!current_time(timer, &current, error) ||
        !ab_calendar_time_difference(&current, timer->state->start, &elapsed, error) ||
        !numeric(timer->state->duration_seconds, &duration, error))
        return false;
    *output = number(duration - elapsed);
    return true;
}

bool ab_session_timer_remaining(AbSessionTimer *timer, AbDataValue *output,
                                AbError *error) {
    if (!begin(timer, error))
        return false;
    return finish(timer, remaining(timer, output, error));
}

bool ab_session_timer_complete(AbSessionTimer *timer, AbDataValue *output,
                               AbError *error) {
    if (!begin(timer, error))
        return false;
    bool okay = output != NULL;
    if (!output)
        ab_fail(error, "Missing session timer complete output");
    else if (!truthy(timer->state->active))
        *output = (AbDataValue){0};
    else if (truthy(timer->state->force_completed))
        *output = timer->state->force_completed;
    else {
        AbDataValue value = {0};
        float seconds = 0;
        okay = remaining(timer, &value, error) && numeric(value, &seconds, error);
        if (okay)
            *output = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = seconds <= 0};
    }
    return finish(timer, okay);
}

bool ab_session_timer_progress(AbSessionTimer *timer, float *output, AbError *error) {
    if (!begin(timer, error))
        return false;
    AbDataValue value;
    float seconds, duration;
    bool okay = output != NULL;
    if (!output)
        ab_fail(error, "Missing session timer progress output");
    else if (!(okay = remaining(timer, &value, error))) {
        /* Preserve a reached clock or arithmetic failure. */
    } else if (!truthy(value))
        *output = 0;
    else if ((okay = numeric(value, &seconds, error) &&
                     numeric(timer->state->duration_seconds, &duration, error)))
        *output = 1 - seconds / duration;
    return finish(timer, okay);
}

bool ab_session_timer_formatted(AbSessionTimer *timer, char *buffer, size_t capacity,
                                AbDataValue *output, AbError *error) {
    if (!begin(timer, error))
        return false;
    AbDataValue value;
    float seconds;
    bool okay = output != NULL;
    if (!output)
        ab_fail(error, "Missing session timer formatted output");
    else if (!(okay = remaining(timer, &value, error))) {
        /* Preserve a reached clock or arithmetic failure. */
    } else if (!truthy(value))
        *output = number(0);
    else {
        okay = buffer && capacity && numeric(value, &seconds, error) &&
               ab_time_format(seconds, buffer, capacity, error);
        if (okay)
            *output =
                (AbDataValue){.kind = AB_DATA_STRING,
                              .string = {(const uint8_t *)buffer, strlen(buffer)}};
        else if (!buffer || !capacity)
            ab_fail(error, "Missing session timer formatted buffer");
    }
    return finish(timer, okay);
}

bool ab_session_timer_end(AbSessionTimer *timer, AbCalendarTime *output,
                          AbError *error) {
    if (!begin(timer, error))
        return false;
    AbSessionTimerState *state = timer->state;
    if (!output || !state->start || !state->duration)
        return finish(timer, ab_fail(error, "Invalid session timer end date"));
    AbCalendarTime end = *state->start;
    end.hour += state->duration->hours;
    end.minutes += state->duration->minutes;
    end.seconds += state->duration->seconds;
    *output = end;
    return finish(timer, true);
}

bool ab_session_timer_save(AbSessionTimer *timer, AbSessionTimerSave *output,
                           AbError *error) {
    if (!begin(timer, error))
        return false;
    if (!output)
        return finish(timer, ab_fail(error, "Missing session timer save output"));
    AbSessionTimerSave saved = {0};
    if (truthy(timer->state->active)) {
        saved.has_start = timer->state->start != NULL;
        saved.has_duration = timer->state->duration != NULL;
        if (saved.has_start)
            saved.start = *timer->state->start;
        if (saved.has_duration)
            saved.duration = *timer->state->duration;
    }
    *output = saved;
    return finish(timer, true);
}

bool ab_session_timer_load(AbSessionTimer *timer, const AbSessionTimerSave *input,
                           AbError *error) {
    if (!begin(timer, error))
        return false;
    bool okay = true;
    if (!input || !input->has_start || !input->has_duration)
        timer->state->active = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    else {
        timer->owned_start = input->start;
        timer->owned_duration = input->duration;
        okay = start_timer(timer, &timer->owned_duration, &timer->owned_start, error);
    }
    return finish(timer, okay);
}
