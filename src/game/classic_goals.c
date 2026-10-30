#include "ab/classic_goals.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataEntry entry;
    AbDataTable table;
} GoalNotification;

struct AbClassicGoals {
    AbClassicGoalConfig config;
    AbClassicLevelView level;
    AbStringMap *map;
    AbDataValue *values;
    GoalNotification *notifications;
    size_t notification_count, bytes, capacity;
    bool calling;
};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool goal(AbDataValue value, bool *output, AbError *error) {
    if (value.kind != AB_DATA_TABLE || !value.table ||
        value.table->count > value.table->capacity ||
        (value.table->count && !value.table->entries))
        return ab_fail(error, "Level-goal check indexes a non-table");
    const AbDataValue *flag = ab_data_get(value.table, text("levelGoal"));
    *output = flag && truthy(*flag);
    return true;
}

bool ab_classic_goals_open(AbClassicGoalConfig config, AbClassicGoals **output,
                           AbError *error) {
    if (!output)
        return ab_fail(error, "Missing classic goals output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    size_t count = level.bodies ? level.bodies->count : 0;
    size_t capacity = config.object_capacity ? config.object_capacity : count;
    if (!count || !level.properties || !config.disable_check ||
        !config.notification_sent || !config.time || !config.score ||
        !config.last_score_time || !config.old_score ||
        config.storage_bytes < sizeof(AbClassicGoals) || capacity < count ||
        capacity > level.bodies->capacity)
        return ab_fail(error, "Invalid classic goals source/globals");
    size_t remaining = config.storage_bytes - sizeof(AbClassicGoals);
    if (capacity > remaining / sizeof(AbDataValue))
        return ab_fail(error, "Classic goal values exceed storage bound");
    remaining -= capacity * sizeof(AbDataValue);
    if (config.notifications > remaining / sizeof(GoalNotification))
        return ab_fail(error, "Classic goal notifications exceed storage bound");
    remaining -= config.notifications * sizeof(GoalNotification);
    AbClassicGoals *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate classic goal owner");
    owner->config = config;
    owner->level = level;
    owner->bytes = config.storage_bytes - remaining;
    owner->capacity = capacity;
    owner->values = calloc(capacity, sizeof(*owner->values));
    if (config.notifications)
        owner->notifications =
            calloc(config.notifications, sizeof(*owner->notifications));
    size_t maximum = 1;
    while (maximum < capacity && maximum <= SIZE_MAX / 2)
        maximum *= 2;
    if (!owner->values || (config.notifications && !owner->notifications) ||
        maximum < capacity ||
        !ab_string_map_open(0, maximum, remaining, &owner->map, error))
        goto release_owner;
    owner->bytes += ab_string_map_bytes(owner->map);
    for (size_t index = 0; index < count; ++index) {
        owner->values[index] = (AbDataValue){
            .kind = AB_DATA_TABLE, .table = &level.properties->objects[index].physical};
        bool include = false;
        if (!goal(owner->values[index], &include, error))
            goto release_owner;
        if (include &&
            !ab_string_map_set(owner->map, level.bodies->objects[index].name,
                               (uintptr_t)(owner->values + index), true, error))
            goto release_owner;
    }
    *output = owner;
    return true;

release_owner:
    if (error && !error->message[0])
        ab_fail(error, "Cannot prepare classic goals");
    ab_classic_goals_close(owner);
    return false;
}

void ab_classic_goals_close(AbClassicGoals *owner) {
    if (owner) {
        ab_string_map_close(owner->map);
        free(owner->notifications);
        free(owner->values);
        free(owner);
    }
}

size_t ab_classic_goals_bytes(const AbClassicGoals *owner) {
    return owner ? owner->bytes : 0;
}

AbStringMap *ab_classic_goals_map(AbClassicGoals *owner) {
    return owner ? owner->map : NULL;
}

bool ab_classic_goals_set(AbClassicGoals *owner, size_t object, bool present,
                          AbError *error) {
    if (!owner || owner->calling || object >= owner->capacity ||
        object >= owner->level.bodies->count || owner->level.world->failed ||
        owner->level.world->locked || owner->level.removed_objects[object])
        return ab_fail(error, "Invalid dynamic canonical goal assignment");
    owner->values[object] =
        (AbDataValue){.kind = AB_DATA_TABLE,
                      .table = &owner->level.properties->objects[object].physical};
    return ab_string_map_set(owner->map, owner->level.bodies->objects[object].name,
                             (uintptr_t)(owner->values + object), present, error);
}

static bool check(AbClassicGoals *owner, bool *cleared, AbError *error) {
    if (truthy(*owner->config.disable_check)) {
        *cleared = false;
        return true;
    }
    size_t cursor = 0;
    for (;;) {
        AbStringMapEntry entry;
        if (!ab_string_map_next(owner->map, &cursor, &entry, error))
            return false;
        if (!entry.present)
            break;
        if (!entry.value)
            return ab_fail(error, "Missing canonical level-goal value");
        bool include = false;
        if (!goal(*(const AbDataValue *)entry.value, &include, error))
            return false;
        if (include) {
            *cleared = false;
            return true;
        }
    }
    if (!truthy(*owner->config.notification_sent)) {
        if (owner->notification_count == owner->config.notifications)
            return ab_fail(error, "Classic goal notifications exhausted");
        GoalNotification *event = owner->notifications + owner->notification_count++;
        event->entry = (AbDataEntry){text("id"), text("EID_LEVEL_GOALS_CLEARED")};
        event->table = (AbDataTable){&event->entry, 1, 1, NULL};
        if (!ab_event_manager_notify(owner->config.events, &event->table, error))
            return false;
        *owner->config.notification_sent =
            (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    }
    *cleared = true;
    return true;
}

bool ab_classic_goals_check(AbClassicGoals *owner, bool *cleared, AbError *error) {
    if (!owner || !cleared || owner->calling)
        return ab_fail(error, "Invalid or reentrant classic goal check");
    owner->calling = true;
    bool okay = check(owner, cleared, error);
    owner->calling = false;
    return okay;
}

bool ab_classic_goals_after_removal(void *context, AbSpan name, AbError *error) {
    AbClassicGoals *owner = context;
    if (!owner || owner->calling)
        return ab_fail(error, "Invalid or reentrant classic removePig continuation");
    AbDataValue current;
    if (!ab_object_world_lookup(&owner->level.properties->lookup,
                                (AbDataValue){.kind = AB_DATA_STRING, .string = name},
                                &current, error))
        return false;
    if (truthy(current))
        return ab_fail(error,
                       "Classic removePig still requires its live-object prefix");
    owner->calling = true;
    bool cleared = false;
    bool okay = check(owner, &cleared, error);
    if (okay) {
        if (cleared)
            *owner->config.last_score_time = *owner->config.time;
        *owner->config.old_score = *owner->config.score;
    }
    owner->calling = false;
    return okay;
}
