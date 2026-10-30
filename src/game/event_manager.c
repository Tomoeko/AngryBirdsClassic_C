#include "ab/event_manager.h"
#include "ab/object_property_access.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataValue id;
    AbEventListener **listeners;
    size_t count;
} EventGroup;

struct AbEventManager {
    AbEventManagerLimits limits;
    AbEventManagerHost host;
    EventGroup *groups;
    AbEventListener **listeners;
    AbDataTable **queued, **buffered, **delayed, **expired;
    size_t group_count, queued_count, buffered_count, delayed_count;
    size_t bytes, depth;
    bool buffering, ticking;
};

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool valid_id(AbDataValue value) {
    switch (value.kind) {
        case AB_DATA_BOOLEAN:
            return true;
        case AB_DATA_NUMBER: {
            float number;
            memcpy(&number, &value.bits, sizeof(number));
            return !isnan(number);
        }
        case AB_DATA_STRING:
            return !value.string.size || value.string.data;
        case AB_DATA_TABLE:
            return value.table != NULL;
        default:
            return false;
    }
}

static bool same(AbDataValue first, AbDataValue second) {
    if (first.kind != second.kind)
        return false;
    switch (first.kind) {
        case AB_DATA_BOOLEAN:
            return (first.bits != 0) == (second.bits != 0);
        case AB_DATA_NUMBER: {
            float a, b;
            memcpy(&a, &first.bits, sizeof(a));
            memcpy(&b, &second.bits, sizeof(b));
            return a == b;
        }
        case AB_DATA_STRING:
            return first.string.size == second.string.size &&
                   (!first.string.size ||
                    !memcmp(first.string.data, second.string.data, first.string.size));
        case AB_DATA_TABLE:
            return first.table == second.table;
        default:
            return false;
    }
}

static EventGroup *find_group(AbEventManager *manager, AbDataValue id) {
    for (size_t index = 0; index < manager->group_count; ++index)
        if (same(manager->groups[index].id, id))
            return manager->groups + index;
    return NULL;
}

static bool diagnostic(AbEventManager *manager, AbEventDiagnostic kind, AbDataValue id,
                       AbEventListener *listener, AbDataTable *event, AbError *error) {
    return !manager->host.diagnostic ||
           manager->host.diagnostic(manager->host.context, kind, id, listener, event,
                                    error);
}

static bool reserve(size_t count, size_t element, size_t limit, size_t *bytes) {
    if (*bytes > limit || count > (limit - *bytes) / element)
        return false;
    *bytes += count * element;
    return true;
}

bool ab_event_manager_open(AbEventManagerLimits limits, AbEventManagerHost host,
                           AbEventManager **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing event manager output");
    *output = NULL;
    size_t bytes = sizeof(AbEventManager);
    if (!limits.notification_depth || limits.notification_depth > 64 ||
        (limits.event_types &&
         limits.listeners_per_type > SIZE_MAX / limits.event_types))
        return ab_fail(error, "Invalid event manager limits");
    size_t listeners = limits.event_types * limits.listeners_per_type;
    if (!reserve(limits.event_types, sizeof(EventGroup), limits.storage_bytes,
                 &bytes) ||
        !reserve(listeners, sizeof(AbEventListener *), limits.storage_bytes, &bytes) ||
        !reserve(limits.queued_events, 2 * sizeof(AbDataTable *), limits.storage_bytes,
                 &bytes) ||
        !reserve(limits.delayed_events, 2 * sizeof(AbDataTable *), limits.storage_bytes,
                 &bytes))
        return ab_fail(error, "Event manager exceeds byte limit");
    AbEventManager *manager = calloc(1, sizeof(*manager));
    if (!manager)
        return ab_fail(error, "Cannot allocate event manager");
    manager->limits = limits;
    manager->host = host;
    manager->bytes = bytes;
    if (limits.event_types)
        manager->groups = calloc(limits.event_types, sizeof(*manager->groups));
    if (listeners)
        manager->listeners = calloc(listeners, sizeof(*manager->listeners));
    if (limits.queued_events) {
        manager->queued = calloc(limits.queued_events, sizeof(*manager->queued));
        manager->buffered = calloc(limits.queued_events, sizeof(*manager->buffered));
    }
    if (limits.delayed_events) {
        manager->delayed = calloc(limits.delayed_events, sizeof(*manager->delayed));
        manager->expired = calloc(limits.delayed_events, sizeof(*manager->expired));
    }
    if ((limits.event_types && !manager->groups) ||
        (listeners && !manager->listeners) ||
        (limits.queued_events && (!manager->queued || !manager->buffered)) ||
        (limits.delayed_events && (!manager->delayed || !manager->expired))) {
        ab_event_manager_close(manager);
        return ab_fail(error, "Cannot allocate event manager arrays");
    }
    *output = manager;
    return true;
}

void ab_event_manager_close(AbEventManager *manager) {
    if (!manager)
        return;
    free(manager->expired);
    free(manager->delayed);
    free(manager->buffered);
    free(manager->queued);
    free(manager->listeners);
    free(manager->groups);
    free(manager);
}

size_t ab_event_manager_bytes(const AbEventManager *manager) {
    return manager ? manager->bytes : 0;
}

AbEventManagerView ab_event_manager_view(const AbEventManager *manager) {
    return manager ? (AbEventManagerView){manager->group_count, manager->queued_count,
                                          manager->buffered_count,
                                          manager->delayed_count, manager->buffering}
                   : (AbEventManagerView){0};
}

bool ab_event_manager_add(AbEventManager *manager, AbDataValue id,
                          AbEventListener *listener, AbError *error) {
    if (!manager || !valid_id(id) || !listener)
        return ab_fail(error, "Invalid event listener registration");
    /* The original constructs the listener assertion's ID message even when
     * its condition is true, before creating a list or checking the callback. */
    if (id.kind != AB_DATA_STRING && id.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Unsupported subscription ID diagnostic concatenation");
    if (!listener->event_triggered) {
        if (!diagnostic(manager, AB_EVENT_DIAGNOSTIC_INVALID_LISTENER, id, listener,
                        NULL, error))
            return false;
        return ab_fail(error, "Event listener has no callback");
    }
    EventGroup *group = find_group(manager, id);
    if (!group) {
        if (manager->group_count == manager->limits.event_types)
            return ab_fail(error, "Event type capacity exhausted");
        group = manager->groups + manager->group_count++;
        group->id = id;
        group->listeners =
            manager->limits.listeners_per_type
                ? manager->listeners +
                      (manager->group_count - 1) * manager->limits.listeners_per_type
                : NULL;
    }
    for (size_t index = 0; index < group->count; ++index) {
        if (group->listeners[index] == listener) {
            if (!diagnostic(manager, AB_EVENT_DIAGNOSTIC_DUPLICATE, id, listener, NULL,
                            error))
                return false;
            AbSpan name = listener->name;
            int name_size = (int)(name.data ? (name.size < 64 ? name.size : 64) : 0);
            if (id.kind == AB_DATA_STRING)
                return ab_fail(error, "Duplicate event listener for %.*s (%.*s)",
                               (int)(id.string.size < 96 ? id.string.size : 96),
                               id.string.data, name_size,
                               name.data ? (const char *)name.data : "");
            return ab_fail(error, "Duplicate event listener for ID %08x (%.*s)",
                           id.bits, name_size,
                           name.data ? (const char *)name.data : "");
        }
    }
    if (group->count == manager->limits.listeners_per_type)
        return ab_fail(error, "Event listener capacity exhausted");
    group->listeners[group->count++] = listener;
    return true;
}

bool ab_event_manager_remove(AbEventManager *manager, AbDataValue id,
                             AbEventListener *listener, AbError *error) {
    if (!manager || !valid_id(id) || !listener)
        return ab_fail(error, "Invalid event listener removal");
    EventGroup *group = find_group(manager, id);
    if (!group || !group->count) {
        if (id.kind != AB_DATA_STRING && id.kind != AB_DATA_NUMBER)
            return ab_fail(error, "Unsupported removal ID diagnostic concatenation");
        if (!diagnostic(manager, AB_EVENT_DIAGNOSTIC_EMPTY_REMOVAL, id, listener, NULL,
                        error))
            return false;
        return ab_fail(error,
                       "Cannot remove listener from an absent or empty event list");
    }
    for (size_t index = group->count; index > 0; --index) {
        if (group->listeners[index - 1] == listener) {
            memmove(group->listeners + index - 1, group->listeners + index,
                    (group->count - index) * sizeof(*group->listeners));
            --group->count;
        }
    }
    return true;
}

static bool event_id(AbDataTable *event, AbDataValue *output, AbError *error) {
    if (!valid_table(event))
        return ab_fail(error, "Event is not a valid plain table");
    AbDataValue id = field(event, "id");
    if (!valid_id(id))
        return ab_fail(error, "Event ID is absent or unsupported");
    *output = id;
    return true;
}

bool ab_event_manager_notify(AbEventManager *manager, AbDataTable *event,
                             AbError *error) {
    AbDataValue id = {0};
    if (!manager || !event_id(event, &id, error))
        return ab_fail(error, "Invalid event notification");
    EventGroup *group = find_group(manager, id);
    if (!group || !group->count)
        return true;
    if (manager->depth == manager->limits.notification_depth)
        return ab_fail(error, "Event notification depth exhausted");
    ++manager->depth;
    bool okay = false;
    for (size_t index = 0; index < group->count; ++index) {
        AbEventListener *listener = group->listeners[index];
        AbDataValue current_id = field(event, "id");
        if (current_id.kind != AB_DATA_STRING && current_id.kind != AB_DATA_NUMBER) {
            ab_fail(error, "Unsupported event ID diagnostic concatenation");
            goto finish;
        }
        if (!diagnostic(manager, AB_EVENT_DIAGNOSTIC_NOTIFY, current_id, listener,
                        event, error))
            goto finish;
        if (!listener || !listener->event_triggered) {
            ab_fail(error, "Event callback is missing at dispatch");
            goto finish;
        }
        if (!listener->event_triggered(listener->context, event, error))
            goto finish;
    }
    okay = true;
finish:
    --manager->depth;
    return okay;
}

bool ab_event_manager_queue(AbEventManager *manager, AbDataTable *event,
                            AbError *error) {
    AbDataValue id = {0};
    if (!manager || !event_id(event, &id, error))
        return ab_fail(error, "Invalid queued event");
    size_t *count =
        manager->buffering ? &manager->buffered_count : &manager->queued_count;
    if (*count == manager->limits.queued_events)
        return ab_fail(error, "Event queue capacity exhausted");
    AbDataTable **queue = manager->buffering ? manager->buffered : manager->queued;
    queue[(*count)++] = event;
    return true;
}

static bool set_delay(AbDataTable *event, float delay, AbError *error) {
    AbDataValue value = {.kind = AB_DATA_NUMBER};
    memcpy(&value.bits, &delay, sizeof(delay));
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = event}};
    return ab_object_property_set(&access, text("delay").string, value, error);
}

bool ab_event_manager_delay(AbEventManager *manager, AbDataTable *event, float delay,
                            AbError *error) {
    AbDataValue id = {0};
    if (!manager || !event_id(event, &id, error) || !isfinite(delay))
        return ab_fail(error, "Invalid delayed event");
    if (!set_delay(event, delay, error))
        return false;
    if (manager->delayed_count == manager->limits.delayed_events)
        return ab_fail(error, "Delayed event capacity exhausted");
    manager->delayed[manager->delayed_count++] = event;
    return true;
}

static bool notify_expired(AbEventManager *manager, size_t count, AbError *error) {
    for (size_t index = 0; index < count; ++index) {
        AbDataTable *event = manager->expired[index];
        if (!ab_event_manager_notify(manager, event, error))
            return false;
        size_t found = 0;
        while (found < manager->delayed_count && manager->delayed[found] != event)
            ++found;
        if (found == manager->delayed_count)
            return ab_fail(error,
                           "Expired event identity disappeared during notification");
        memmove(manager->delayed + found, manager->delayed + found + 1,
                (manager->delayed_count - found - 1) * sizeof(*manager->delayed));
        --manager->delayed_count;
    }
    return true;
}

bool ab_event_manager_tick(AbEventManager *manager, float seconds, AbError *error) {
    if (!manager || manager->ticking || !isfinite(seconds))
        return ab_fail(error, "Invalid or reentrant event tick");
    manager->ticking = true;
    manager->buffering = true;
    bool okay = false;
    size_t queued = manager->queued_count;
    for (size_t index = 0; index < queued; ++index)
        if (!ab_event_manager_notify(manager, manager->queued[index], error))
            goto finish;
    manager->buffering = false;
    if (manager->buffered_count)
        memcpy(manager->queued, manager->buffered,
               manager->buffered_count * sizeof(*manager->queued));
    manager->queued_count = manager->buffered_count;
    manager->buffered_count = 0;
    size_t expired = 0;
    for (size_t index = 0; index < manager->delayed_count; ++index) {
        AbDataTable *event = manager->delayed[index];
        AbDataValue value = field(event, "delay");
        float delay = 0;
        if (value.kind != AB_DATA_NUMBER) {
            ab_fail(error, "Delayed event has a nonnumeric delay");
            goto finish;
        }
        memcpy(&delay, &value.bits, sizeof(delay));
        delay = delay - seconds;
        if (!isfinite(delay)) {
            ab_fail(error, "Unsupported delayed event arithmetic");
            goto finish;
        }
        if (!set_delay(event, delay, error))
            goto finish;
        if (delay <= 0)
            manager->expired[expired++] = event;
    }
    okay = notify_expired(manager, expired, error);
finish:
    manager->ticking = false;
    return okay;
}
