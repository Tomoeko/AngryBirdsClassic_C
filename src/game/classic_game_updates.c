#include "ab/classic_game_updates.h"

#include <stdlib.h>

struct AbClassicGameUpdates {
    AbStringMap *maps[2], *current, *iterating;
    size_t bytes;
    bool failed;
};

static bool valid(AbClassicGameUpdates *updates, bool table, AbError *error) {
    return updates && !updates->failed && (!table || updates->current)
               ? true
               : ab_fail(error, "Invalid, nil or failed game update registry");
}

bool ab_classic_game_updates_open(size_t nodes, size_t bytes,
                                  AbClassicGameUpdates **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing game update registry output");
    *output = NULL;
    if (!nodes || (nodes & (nodes - 1)) || bytes < sizeof(AbClassicGameUpdates))
        return ab_fail(error, "Invalid game update registry limits");
    AbClassicGameUpdates *updates = calloc(1, sizeof(*updates));
    if (!updates)
        return ab_fail(error, "Cannot allocate game update registry");
    updates->bytes = sizeof(*updates);
    for (size_t index = 0; index < 2; ++index) {
        if (!ab_string_map_open(0, nodes, bytes - updates->bytes, updates->maps + index,
                                error)) {
            ab_classic_game_updates_close(updates);
            return false;
        }
        updates->bytes += ab_string_map_bytes(updates->maps[index]);
    }
    *output = updates;
    return true;
}

void ab_classic_game_updates_close(AbClassicGameUpdates *updates) {
    if (updates) {
        ab_string_map_close(updates->maps[0]);
        ab_string_map_close(updates->maps[1]);
        free(updates);
    }
}

size_t ab_classic_game_updates_bytes(const AbClassicGameUpdates *updates) {
    return updates ? updates->bytes : 0;
}

bool ab_classic_game_updates_lookup(const AbClassicGameUpdates *updates, AbSpan key,
                                    const AbClassicGameUpdateFunction **output,
                                    AbError *error) {
    if (!updates || !output || (key.size && !key.data))
        return ab_fail(error, "Invalid game update lookup");
    uintptr_t value = 0;
    bool present = false;
    if (updates->current &&
        !ab_string_map_get(updates->current, key, &value, &present, error))
        return false;
    *output = present ? (const AbClassicGameUpdateFunction *)value : NULL;
    return true;
}

bool ab_classic_game_updates_clear(AbClassicGameUpdates *updates, AbError *error) {
    if (!valid(updates, false, error))
        return false;
    /* The private table is exposed only to the current pairs iterator. Two
     * maps preserve that old table even across repeated clears in one call. */
    AbStringMap *next =
        updates->maps[0] == updates->iterating ? updates->maps[1] : updates->maps[0];
    if (!ab_string_map_reset(next, 0, error))
        return false;
    updates->current = next;
    return true;
}

bool ab_classic_game_updates_add(AbClassicGameUpdates *updates, AbSpan key,
                                 const AbClassicGameUpdateFunction *function,
                                 AbError *error) {
    if (!valid(updates, true, error))
        return false;
    uintptr_t prior;
    bool present;
    if (!ab_string_map_get(updates->current, key, &prior, &present, error))
        return false;
    return present || ab_string_map_set(updates->current, key, (uintptr_t)function,
                                        function != NULL, error);
}

bool ab_classic_game_updates_remove(AbClassicGameUpdates *updates, AbSpan key,
                                    AbError *error) {
    return valid(updates, true, error) &&
           ab_string_map_set(updates->current, key, 0, false, error);
}

bool ab_classic_game_updates_call(AbClassicGameUpdates *updates, float seconds,
                                  float time, AbError *error) {
    if (!valid(updates, true, error) || updates->iterating)
        return ab_fail(error, "Invalid or reentrant game update registry call");
    updates->iterating = updates->current;
    bool okay = true;
    size_t cursor = 0;
    for (;;) {
        AbStringMapEntry entry;
        if (!ab_string_map_next(updates->iterating, &cursor, &entry, error)) {
            okay = false;
            break;
        }
        if (!entry.present)
            break;
        const AbClassicGameUpdateFunction *function =
            (const AbClassicGameUpdateFunction *)entry.value;
        if (!function || !function->update) {
            okay = ab_fail(error, "Missing actual registered game update function");
            break;
        }
        /* Capture this value before invocation: a callback may replace the
         * upvalue table or mutate the current key and later function records. */
        void *context = function->context;
        bool (*update)(void *, float, float, AbError *) = function->update;
        if (!update(context, seconds, time, error) ||
            !ab_string_map_resume(updates->iterating, entry.key, &cursor, error)) {
            okay = false;
            break;
        }
    }
    updates->iterating = NULL;
    updates->failed = !okay;
    return okay;
}
