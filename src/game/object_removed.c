#include "ab/object_removed.h"
#include "ab/object_property_access.h"

#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static AbDataValue text(AbSpan value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = value};
}

static AbDataValue key(const char *value) {
    return text((AbSpan){(const uint8_t *)value, strlen(value)});
}

static bool field(AbDataValue table, AbDataValue name, AbDataValue *output,
                  AbError *error) {
    if (table.kind != AB_DATA_TABLE || !table.table ||
        table.table->count > table.table->capacity ||
        (table.table->count && !table.table->entries)) {
        return ab_fail(error, "Object-removed notification indexes a non-table");
    }
    const AbDataValue *found = ab_data_get(table.table, name);
    *output = found ? *found : (AbDataValue){0};
    return true;
}

static bool erase(AbObjectRemovedState *state, const char *map, AbSpan name,
                  AbError *error) {
    AbDataValue table = {0};
    if (!field(state->objects, key(map), &table, error)) {
        return false;
    }
    AbObjectPropertyAccess access = {.physics_object = table};
    return ab_object_property_set(&access, name, (AbDataValue){0}, error);
}

bool ab_object_removed(AbObjectRemovedState *state, AbSpan name,
                       AbObjectRemovedHost *host, AbError *error) {
    if (!state || (name.size && !name.data)) {
        return ab_fail(error, "Invalid object-removed notification input");
    }
    AbDataValue world = {0}, object = {0}, controllable = {0};
    if (!field(state->objects, key("world"), &world, error) ||
        !field(world, text(name), &object, error)) {
        return false;
    }
    if (!truthy(object)) {
        return true;
    }
    if (!field(object, key("controllable"), &controllable, error)) {
        return false;
    }
    if (truthy(controllable)) {
        AbDataValue bait = {0}, eagle = {0};
        if (!field(object, key("isMightyBait"), &bait, error) ||
            (!truthy(bait) && !field(object, key("isMightyEagle"), &eagle, error))) {
            return false;
        }
        if (!truthy(bait) && !truthy(eagle)) {
            return true;
        }
        if (!host || !host->exited) {
            return ab_fail(error, "Missing object-removed exitedFunction host");
        }
        return host->exited(host->context, object, state->seconds, error);
    }
    if (!host || !host->notify) {
        return ab_fail(error, "Missing object-removed notification upvalue");
    }
    if (!host->notify(host->context, object, error) ||
        !erase(state, "world", name, error) || !erase(state, "dynamic", name, error)) {
        return false;
    }
    if (!state->level_goals ||
        !ab_string_map_set(state->level_goals, name, 0, false, error)) {
        return ab_fail(error, "Cannot clear current object-removed goal table");
    }
    if (!host->check_goals) {
        return ab_fail(error, "Missing object-removed goal check host");
    }
    AbDataValue result = {0};
    if (!host->check_goals(host->context, &result, error)) {
        return false;
    }
    if (truthy(result)) {
        state->last_score_time = state->time;
        state->old_score = state->score;
    }
    return true;
}
