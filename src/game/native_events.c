#include "ab/native_events.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float seconds;
    AbNativeEventCall call;
} DelayedCall;

struct AbNativeEvents {
    AbNativeEventCall *pending, *processing;
    DelayedCall *delayed;
    AbNativeEventState state;
    size_t capacity, bytes;
    bool updating, failed;
};

bool ab_native_events_open(size_t capacity, size_t storage_bytes,
                           AbNativeEvents **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing native event owner output");
    *output = NULL;
    size_t stride = 2 * sizeof(AbNativeEventCall) + sizeof(DelayedCall);
    if (!capacity || capacity > (SIZE_MAX - sizeof(AbNativeEvents)) / stride)
        return ab_fail(error, "Invalid native event capacity");
    size_t bytes = sizeof(AbNativeEvents) + capacity * stride;
    if (bytes > storage_bytes)
        return ab_fail(error, "Native event storage exceeds bound");
    AbNativeEvents *events = calloc(1, bytes);
    if (!events)
        return ab_fail(error, "Cannot allocate native event storage");
    events->pending = (AbNativeEventCall *)(events + 1);
    events->processing = events->pending + capacity;
    events->delayed = (DelayedCall *)(events->processing + capacity);
    events->capacity = capacity;
    events->bytes = bytes;
    *output = events;
    return true;
}

void ab_native_events_close(AbNativeEvents *events) {
    free(events);
}

size_t ab_native_events_bytes(const AbNativeEvents *events) {
    return events ? events->bytes : 0;
}

bool ab_native_events_post(AbNativeEvents *events, float delay_seconds,
                           AbNativeEventCall call, AbError *error) {
    if (!events || events->failed || !isfinite(delay_seconds))
        return ab_fail(error, "Invalid or stopped native event post");
    AbNativeEventState *state = &events->state;
    if (state->pending + state->delayed + state->processing == events->capacity)
        return ab_fail(error, "Native event capacity exhausted");
    if (delay_seconds < 0)
        events->pending[state->pending++] = call;
    else
        events->delayed[state->delayed++] = (DelayedCall){delay_seconds, call};
    return true;
}

static bool prepare_processing(AbNativeEvents *events, float seconds, AbError *error) {
    AbNativeEventState *state = &events->state;
    if (state->pending) {
        memcpy(events->processing + state->processing, events->pending,
               state->pending * sizeof(*events->pending));
        state->processing += state->pending;
        state->pending = 0;
    }
    size_t index = 0;
    while (index < state->delayed) {
        DelayedCall *delayed = events->delayed + index;
        if (delayed->seconds > 0) {
            delayed->seconds -= seconds;
            if (!isfinite(delayed->seconds))
                return ab_fail(error, "Native event delay overflow");
            ++index;
            continue;
        }
        events->processing[state->processing++] = delayed->call;
        --state->delayed;
        if (index < state->delayed)
            memmove(delayed, delayed + 1, (state->delayed - index) * sizeof(*delayed));
    }
    return true;
}

bool ab_native_events_process(AbNativeEvents *events, float seconds, size_t *pending,
                              AbError *error) {
    if (!events || events->failed || events->updating || !pending || !isfinite(seconds))
        return ab_fail(error, "Invalid or stopped native event processing");
    events->updating = true;
    bool okay = prepare_processing(events, seconds, error);
    AbNativeEventState *state = &events->state;
    while (okay && state->cursor < state->processing) {
        AbNativeEventCall call = events->processing[state->cursor++];
        okay = call.call ? call.call(call.context, error)
                         : ab_fail(error, "Reached empty native event callable");
    }
    if (okay) {
        state->processing = 0;
        state->cursor = 0;
        *pending = state->pending;
    }
    events->updating = false;
    events->failed = events->failed || !okay;
    return okay;
}

bool ab_native_events_state(const AbNativeEvents *events, AbNativeEventState *state) {
    if (!events || !state)
        return false;
    *state = events->state;
    return true;
}

bool ab_native_events_delay(const AbNativeEvents *events, size_t index, float *seconds,
                            AbNativeEventCall *call) {
    if (!events || index >= events->state.delayed || !seconds || !call)
        return false;
    *seconds = events->delayed[index].seconds;
    *call = events->delayed[index].call;
    return true;
}

bool ab_native_animation_initialize(AbNativeEvents *events, const size_t *system_count,
                                    bool *updating_systems, AbNativeAnimation *output,
                                    AbError *error) {
    if (!events || !system_count || !updating_systems || !output)
        return ab_fail(error, "Incomplete native animation binding");
    *output = (AbNativeAnimation){.events = events,
                                  .system_count = system_count,
                                  .updating_systems = updating_systems};
    return true;
}

bool ab_native_animation_update(AbNativeAnimation *animation, float seconds,
                                AbError *error) {
    (void)seconds;
    if (!animation || animation->failed || animation->updating || !animation->events ||
        !animation->system_count || !animation->updating_systems)
        return ab_fail(error, "Invalid or stopped native animation update");
    animation->updating = true;
    size_t pending;
    bool okay = ab_native_events_process(animation->events, 0, &pending, error);
    if (okay) {
        *animation->updating_systems = true;
        okay = !*animation->system_count ||
               ab_fail(error, "Nonempty native animation systems need component owner");
        if (okay)
            *animation->updating_systems = false;
    }
    animation->updating = false;
    animation->failed = animation->failed || !okay;
    return okay;
}
