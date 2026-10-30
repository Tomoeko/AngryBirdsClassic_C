#include "ab/block_removal_cursor.h"

#include <string.h>

static AbDataValue key(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static bool field(AbDataValue table, AbDataValue name, AbDataValue *output,
                  AbError *error) {
    if (table.kind != AB_DATA_TABLE || !table.table ||
        table.table->count > table.table->capacity ||
        (table.table->count && !table.table->entries)) {
        return ab_fail(error, "Block removal indexes a non-table value");
    }
    const AbDataValue *found = ab_data_get(table.table, name);
    *output = found ? *found : (AbDataValue){0};
    return true;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

bool ab_block_removal_open(const AbBlockRemovalState *state,
                           AbBlockRemovalCursor *output, AbError *error) {
    if (!state || !output) {
        return ab_fail(error, "Missing block removal traversal input");
    }
    AbDataValue world = {0};
    if (!field(state->objects, key("world"), &world, error)) {
        return false;
    }
    if (world.kind == AB_DATA_NIL) {
        *output = (AbBlockRemovalCursor){.finished = true};
        return true;
    }
    if (!state->dead_blocks) {
        return ab_fail(error, "Missing deadBlocks iteration table");
    }
    *output = (AbBlockRemovalCursor){.iteration = state->dead_blocks};
    return true;
}

static bool erase(AbBlockRemovalState *state, AbSpan name, AbError *error) {
    if (!state->dead_blocks) {
        return ab_fail(error, "Missing current deadBlocks deletion table");
    }
    return ab_string_map_set(state->dead_blocks, name, 0, false, error);
}

static bool is_rocket(AbDataValue object, AbDataValue strength, bool *output,
                      AbError *error) {
    if (strength.kind != AB_DATA_NUMBER) {
        return ab_fail(error, "Block removal strength comparison requires a number");
    }
    float number;
    memcpy(&number, &strength.bits, sizeof(number));
    *output = false;
    if (!(number <= 0.0f)) {
        return true;
    }
    AbDataValue rocket = {0}, timer = {0};
    if (!field(object, key("rocket"), &rocket, error)) {
        return false;
    }
    if (rocket.kind == AB_DATA_NIL) {
        return true;
    }
    if (!field(object, key("deathTimerFull"), &timer, error)) {
        return false;
    }
    *output = timer.kind == AB_DATA_NIL;
    return true;
}

bool ab_block_removal_next(AbBlockRemovalState *state, AbBlockRemovalCursor *cursor,
                           const AbBlockRemovalHost *host, AbBlockRemovalEntry *output,
                           bool *found, AbError *error) {
    if (!cursor || !output || !found) {
        return ab_fail(error, "Missing block removal traversal output");
    }
    if (cursor->finished) {
        *found = false;
        return true;
    }
    if (!state || !cursor->iteration) {
        return ab_fail(error, "Invalid block removal cursor state");
    }
    for (;;) {
        /* Explosions enqueue additional deaths inside the ordinary removal
         * pipeline. Rehash can move the retained key or shrink the hash part.
         * Lua pairs resumes next(table, key), not a saved physical slot. */
        if (cursor->resume_pending) {
            if (!ab_string_map_resume(cursor->iteration, cursor->resume_key,
                                      &cursor->cursor, error))
                return false;
            cursor->resume_pending = false;
        }
        AbStringMapEntry entry = {0};
        if (!ab_string_map_next(cursor->iteration, &cursor->cursor, &entry, error)) {
            return false;
        }
        if (!entry.present) {
            cursor->finished = true;
            *found = false;
            return true;
        }
        cursor->resume_key = entry.key;
        cursor->resume_pending = true;
        AbDataValue object =
            entry.value ? *(const AbDataValue *)entry.value : (AbDataValue){0};
        AbDataValue world = {0}, current = {0};
        if (!field(state->objects, key("world"), &world, error) ||
            !field(world, (AbDataValue){.kind = AB_DATA_STRING, .string = entry.key},
                   &current, error)) {
            return false;
        }
        if (!truthy(current)) {
            if (!erase(state, entry.key, error)) {
                return false;
            }
            continue;
        }
        if (object.kind == AB_DATA_NIL) {
            continue;
        }
        AbDataValue strength = {0};
        if (!field(object, key("strength"), &strength, error)) {
            return false;
        }
        if (strength.kind == AB_DATA_NIL) {
            continue;
        }
        bool rocket = false;
        if (!is_rocket(object, strength, &rocket, error)) {
            return false;
        }
        if (!rocket) {
            *output = (AbBlockRemovalEntry){entry.key, object};
            *found = true;
            return true;
        }
        AbDataValue name = {0};
        if (!field(object, key("name"), &name, error)) {
            return false;
        }
        if (!host || !host->rocket) {
            return ab_fail(error, "Missing shootRocket host");
        }
        if (!host->rocket(host->context, name, error) ||
            !erase(state, entry.key, error)) {
            return false;
        }
    }
}
