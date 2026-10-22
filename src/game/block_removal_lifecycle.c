#include "ab/block_removal_lifecycle.h"
#include "ab/object_property_access.h"

#include <string.h>

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static bool field(AbDataValue table, AbDataValue key, AbDataValue *output,
                  AbError *error) {
    if (table.kind != AB_DATA_TABLE || !table.table ||
        table.table->count > table.table->capacity ||
        (table.table->count && !table.table->entries)) {
        return ab_fail(error, "Removal lifecycle indexes a non-table value");
    }
    const AbDataValue *found = ab_data_get(table.table, key);
    *output = found ? *found : (AbDataValue){0};
    return true;
}

static bool raw_spawn(AbDataValue object, const AbBlockRemovalDetachHost *host,
                      const char *name, AbDataValue *output, AbError *error) {
    AbDataValue blocks = {0}, key = {0}, raw = {0};
    return field(host->block_table, text("blocks"), &blocks, error) &&
           field(object, text("definition"), &key, error) &&
           field(blocks, key, &raw, error) && field(raw, text(name), output, error);
}

bool ab_block_removal_detach(const AbBlockRemovalEntry *entry,
                             AbBlockRemovalDetachHost *host,
                             AbBlockRemovalDetached *output, AbError *error) {
    if (!entry || !host || !output || (entry->key.size && !entry->key.data)) {
        return ab_fail(error, "Invalid removal detach input");
    }
    const AbBlockRemovalEntry captured = *entry;
    AbBlockRemovalDetached result = {0};
    if (!raw_spawn(captured.object, host, "spawn", &result.spawn, error) ||
        !raw_spawn(captured.object, host, "deathSpawn", &result.death_spawn, error) ||
        !field(captured.object, text("x"), &result.x, error) ||
        !field(captured.object, text("y"), &result.y, error)) {
        return false;
    }
    if (!host->remove) {
        return ab_fail(error, "Missing native removeObject host");
    }
    if (!host->remove(host->context, captured.key, error)) {
        return false;
    }
    *output = result;
    return true;
}

static bool clear_object_map(AbBlockRemovalState *state, const char *name, AbSpan key,
                             AbError *error) {
    AbDataValue table = {0};
    if (!state || !field(state->objects, text(name), &table, error)) {
        return false;
    }
    AbObjectPropertyAccess view = {.physics_object = table};
    return ab_object_property_set(&view, key, (AbDataValue){0}, error);
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

bool ab_block_removal_cleanup(const AbBlockRemovalEntry *entry,
                              AbBlockRemovalCleanupState *state,
                              AbBlockRemovalCleanupHost *host, AbError *error) {
    if (!entry || !state || !host || (entry->key.size && !entry->key.data)) {
        return ab_fail(error, "Invalid removal cleanup input");
    }
    const AbBlockRemovalEntry captured = *entry;
    if (!state->level_goals) {
        return ab_fail(error, "Missing current removal levelGoals table");
    }
    if (!ab_string_map_set(state->level_goals, captured.key, 0, false, error) ||
        !clear_object_map(state->removal, "world", captured.key, error) ||
        !clear_object_map(state->removal, "dynamic", captured.key, error)) {
        return false;
    }
    if (!state->removal->dead_blocks) {
        return ab_fail(error, "Missing current cleanup deadBlocks table");
    }
    if (!ab_string_map_set(state->removal->dead_blocks, captured.key, 0, false,
                           error)) {
        return false;
    }
    AbDataValue goal = {0};
    if (!field(captured.object, text("levelGoal"), &goal, error)) {
        return false;
    }
    if (truthy(goal)) {
        if (!host->remove_pig) {
            return ab_fail(error, "Missing removal removePig host");
        }
        if (!host->remove_pig(host->context, captured.key, error)) {
            return false;
        }
    }
    AbDataValue gift = {0};
    if (!field(captured.object, text("isGiftBox"), &gift, error)) {
        return false;
    }
    if (!truthy(gift)) {
        return true;
    }
    AbDataValue level = state->level_name, x = {0}, y = {0};
    if (!field(captured.object, text("x"), &x, error) ||
        !field(captured.object, text("y"), &y, error)) {
        return false;
    }
    if (!host->collect_gift) {
        return ab_fail(error, "Missing gift-box collection host");
    }
    return host->collect_gift(host->context, level, x, y, error);
}
