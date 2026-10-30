#include "ab/delayed_callbacks.h"

#include "ab/integer_map.h"

#include <math.h>
#include <stdlib.h>

typedef struct {
    const AbDelayedFunction *function;
    AbDataValue parameters;
    float timeout;
    bool current, snapshot;
} DelayedRecord;

struct AbDelayedCallbacks {
    AbClassicGameUpdates *updates;
    AbClassicGameUpdateFunction update;
    AbIntegerMap *current, *snapshot;
    DelayedRecord *records;
    size_t capacity, bytes;
    int32_t next_id;
    bool initialized, updating, failed;
};

static AbSpan update_key(void) {
    static const uint8_t key[] = "updateDelayedCallbacks";
    return (AbSpan){key, sizeof(key) - 1};
}

static bool update_callback(void *context, float seconds, float time, AbError *error) {
    (void)time;
    return ab_delayed_callbacks_update(context, seconds, error);
}

bool ab_delayed_callbacks_open(AbClassicGameUpdates *game_updates, size_t capacity,
                               size_t bytes, AbDelayedCallbacks **output,
                               AbError *error) {
    if (!output)
        return ab_fail(error, "Missing delayed callback output");
    *output = NULL;
    if (!game_updates || !capacity || (capacity & (capacity - 1)) ||
        bytes < sizeof(AbDelayedCallbacks) ||
        capacity > (bytes - sizeof(AbDelayedCallbacks)) / (2 * sizeof(DelayedRecord)))
        return ab_fail(error, "Invalid delayed callback bounds");
    AbDelayedCallbacks *callbacks = calloc(1, sizeof(*callbacks));
    if (!callbacks)
        return ab_fail(error, "Cannot allocate delayed callbacks");
    callbacks->capacity = capacity;
    callbacks->bytes = sizeof(*callbacks) + capacity * 2 * sizeof(DelayedRecord);
    callbacks->updates = game_updates;
    callbacks->update = (AbClassicGameUpdateFunction){callbacks, update_callback};
    callbacks->records = calloc(capacity * 2, sizeof(*callbacks->records));
    if (!callbacks->records || !ab_integer_map_open(capacity, bytes - callbacks->bytes,
                                                    &callbacks->current, error))
        goto failed;
    callbacks->bytes += ab_integer_map_bytes(callbacks->current);
    if (!ab_integer_map_open(capacity, bytes - callbacks->bytes, &callbacks->snapshot,
                             error))
        goto failed;
    callbacks->bytes += ab_integer_map_bytes(callbacks->snapshot);
    *output = callbacks;
    return true;
failed:
    ab_delayed_callbacks_close(callbacks);
    return ab_fail(error, "Cannot allocate bounded delayed callback tables");
}

void ab_delayed_callbacks_close(AbDelayedCallbacks *callbacks) {
    if (callbacks) {
        ab_integer_map_close(callbacks->current);
        ab_integer_map_close(callbacks->snapshot);
        free(callbacks->records);
        free(callbacks);
    }
}

size_t ab_delayed_callbacks_bytes(const AbDelayedCallbacks *callbacks) {
    return callbacks ? callbacks->bytes : 0;
}

static bool valid(AbDelayedCallbacks *callbacks, AbError *error) {
    return callbacks && callbacks->initialized && !callbacks->failed
               ? true
               : ab_fail(error, "Invalid, nil or failed delayed callbacks");
}

bool ab_delayed_callbacks_initialize(AbDelayedCallbacks *callbacks, AbError *error) {
    if (!callbacks || callbacks->failed ||
        !ab_integer_map_reset(callbacks->current, error))
        return ab_fail(error, "Invalid delayed callback initialization");
    for (size_t index = 0; index < callbacks->capacity * 2; ++index)
        callbacks->records[index].current = false;
    callbacks->next_id = 1;
    callbacks->initialized = true;
    return true;
}

bool ab_delayed_callbacks_add(AbDelayedCallbacks *callbacks, float timeout,
                              const AbDelayedFunction *function, AbDataValue parameters,
                              AbError *error) {
    if (!valid(callbacks, error))
        return false;
    if (!isfinite(timeout) || !function || !function->call ||
        callbacks->next_id >= 16777216)
        return ab_fail(error, "Invalid or exhausted delayed callback addition");
    if (!ab_classic_game_updates_add(callbacks->updates, update_key(),
                                     &callbacks->update, error))
        return false;
    AbIntegerMapLayout layout;
    if (!ab_integer_map_layout(callbacks->current, &layout) ||
        layout.count == callbacks->capacity)
        return ab_fail(error, "Concurrent delayed callback capacity exhausted");
    size_t index = 0;
    while (index < callbacks->capacity * 2 &&
           (callbacks->records[index].current || callbacks->records[index].snapshot))
        ++index;
    if (index == callbacks->capacity * 2)
        return ab_fail(error, "Delayed callback snapshot storage exhausted");
    DelayedRecord *record = callbacks->records + index;
    if (!ab_integer_map_set(callbacks->current, callbacks->next_id, (uintptr_t)record,
                            true, error))
        return false;
    *record = (DelayedRecord){function, parameters, timeout, true, false};
    callbacks->next_id++;
    return true;
}

static bool snapshot_callbacks(AbDelayedCallbacks *callbacks, AbError *error) {
    if (!ab_integer_map_reset(callbacks->snapshot, error))
        return false;
    size_t cursor = 0;
    AbIntegerMapEntry entry;
    while (ab_integer_map_next(callbacks->current, &cursor, &entry, error)) {
        if (!entry.present)
            return true;
        if (!ab_integer_map_set(callbacks->snapshot, entry.key, entry.value, true,
                                error))
            return false;
        ((DelayedRecord *)entry.value)->snapshot = true;
    }
    return false;
}

static bool update_snapshot(AbDelayedCallbacks *callbacks, float seconds,
                            AbError *error) {
    size_t cursor = 0;
    AbIntegerMapEntry entry;
    while (ab_integer_map_next(callbacks->snapshot, &cursor, &entry, error)) {
        if (!entry.present)
            return true;
        DelayedRecord *record = (DelayedRecord *)entry.value;
        record->timeout -= seconds;
        if (!isfinite(record->timeout))
            return ab_fail(error, "Delayed callback timeout arithmetic overflow");
        if (record->timeout <= 0) {
            const AbDelayedFunction function = *record->function;
            if (!function.call(function.context, record->parameters, error) ||
                !ab_integer_map_set(callbacks->snapshot, entry.key, 0, false, error))
                return false;
            uintptr_t value;
            bool present;
            if (!ab_integer_map_get(callbacks->current, entry.key, &value, &present,
                                    error))
                return false;
            if (present)
                ((DelayedRecord *)value)->current = false;
            if (!ab_integer_map_set(callbacks->current, entry.key, 0, false, error))
                return false;
        }
        record->snapshot = false;
    }
    return false;
}

bool ab_delayed_callbacks_update(AbDelayedCallbacks *callbacks, float seconds,
                                 AbError *error) {
    if (!valid(callbacks, error) || callbacks->updating || !isfinite(seconds))
        return ab_fail(error, "Invalid or reentrant delayed callback update");
    callbacks->updating = true;
    bool okay = snapshot_callbacks(callbacks, error) &&
                update_snapshot(callbacks, seconds, error);
    callbacks->updating = false;
    callbacks->failed = !okay;
    return okay;
}
