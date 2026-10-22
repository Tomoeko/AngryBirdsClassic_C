#include "ab/classic_destruction_statistics.h"

#include "ab/object_property_access.h"

#include <stdlib.h>
#include <string.h>

struct AbClassicDestructionStatistics {
    AbClassicDestructionStatisticsConfig config;
    AbClassicLevelView level;
    AbDataTable *events;
    AbDataEntry *fields;
    size_t count, bytes;
    bool calling;
};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool named(AbDataValue value, const char *name) {
    AbSpan expected = span(name);
    return value.kind == AB_DATA_STRING && value.string.size == expected.size &&
           (!expected.size ||
            (value.string.data &&
             !memcmp(value.string.data, expected.data, expected.size)));
}

static AbDataTable *event(AbClassicDestructionStatistics *owner, const char *id,
                          AbError *error) {
    if (owner->count == owner->config.notifications) {
        ab_fail(error, "Destruction statistics notification storage exhausted");
        return NULL;
    }
    AbDataTable *result = owner->events + owner->count++;
    result->count = 1;
    result->entries[0] = (AbDataEntry){text("id"), text(id)};
    return result;
}

static bool assign(AbDataTable *table, AbSpan key, AbDataValue value, AbError *error) {
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = table}};
    return ab_object_property_set(&access, key, value, error);
}

static bool sum(AbClassicDestructionStatistics *owner, const char *name, float *output,
                AbError *error) {
    return ab_settings_statistics_device_sum(owner->config.settings, span(name), output,
                                             error);
}

static bool material(AbClassicDestructionStatistics *owner, size_t object,
                     AbDataValue name, const AbDataTable **extra, AbError *error) {
    const char *const materials[] = {"wood", "light", "rock", "piglette"};
    const char *const events[] = {"EID_WOODEN_BLOCK_DESTROYED",
                                  "EID_GLASS_BLOCK_DESTROYED",
                                  "EID_STONE_BLOCK_DESTROYED", "EID_PIG_POPPED"};
    const char *const counters[] = {"woodBlocksDestroyed", "iceBlocksDestroyed",
                                    "rockBlocksDestroyed", "pigsDestroyed"};
    size_t index = 0;
    while (index < 4 && !named(name, materials[index]))
        ++index;
    if (index == 4)
        return owner->config.per_project
                   ? owner->config.per_project(owner->config.context, object, extra,
                                               error)
                   : ab_fail(error,
                             "Missing destruction per-project achievement service");
    AbDataTable *notification = event(owner, events[index], error);
    if (!notification || !assign(notification, span("amount"), number(1), error) ||
        !ab_event_manager_notify(owner->config.events, notification, error))
        return false;
    return ab_settings_statistics_increment(owner->config.settings,
                                            span(counters[index]), error);
}

static bool update(AbClassicDestructionStatistics *owner, size_t object,
                   AbError *error) {
    if (truthy(*owner->config.eagle_bait_launched))
        return true;
    AbDataValue model = *owner->config.device_model;
    if (!named(model, "iphone") && !named(model, "ipad") &&
        !truthy(*owner->config.gamer_service))
        return true;
    if (object >= owner->level.properties->count)
        return ab_fail(error, "Unknown destruction statistics object");
    const AbDataValue *name = ab_data_get(
        &owner->level.properties->objects[object].physical, text("materialName"));
    const AbDataTable *extra = NULL;
    if (name && name->kind != AB_DATA_NIL) {
        if (!material(owner, object, *name, &extra, error))
            return false;
        *owner->config.total_blocks = 0;
        const char *const counters[] = {"woodBlocksDestroyed", "iceBlocksDestroyed",
                                        "rockBlocksDestroyed"};
        for (size_t index = 0; index < 3; ++index) {
            float value;
            if (!sum(owner, counters[index], &value, error))
                return false;
            *owner->config.total_blocks = *owner->config.total_blocks + value;
        }
    }
    AbDataTable *notification = event(owner, "EID_BLOCKS_DESTROYED", error);
    if (!notification || !assign(notification, span("totalBlocks"),
                                 number(*owner->config.total_blocks), error))
        return false;
    const char *const counters[] = {"pigsDestroyed", "rockBlocksDestroyed",
                                    "iceBlocksDestroyed", "woodBlocksDestroyed"};
    for (size_t index = 0; index < 4; ++index) {
        float value;
        if (!sum(owner, counters[index], &value, error) ||
            !assign(notification, span(counters[index]), number(value), error))
            return false;
    }
    if (extra) {
        if (extra->count > extra->capacity || (extra->count && !extra->entries))
            return ab_fail(error, "Malformed destruction per-project fields");
        /* Plain immutable string fields have no callbacks while copied. */
        for (size_t index = 0; index < extra->count; ++index) {
            AbDataEntry entry = extra->entries[index];
            if (entry.key.kind != AB_DATA_STRING)
                return ab_fail(error, "Unsupported destruction extra-field key");
            if (!assign(notification, entry.key.string, entry.value, error))
                return false;
        }
    }
    return ab_event_manager_notify(owner->config.events, notification, error);
}

bool ab_classic_destruction_statistics_open(AbClassicDestructionStatisticsConfig config,
                                            AbClassicDestructionStatistics **output,
                                            AbError *error) {
    if (!output)
        return ab_fail(error, "Missing classic destruction statistics output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    if (!level.properties || !config.settings || !config.events ||
        !config.eagle_bait_launched || !config.device_model || !config.gamer_service ||
        !config.total_blocks || config.event_fields < 6 ||
        config.storage_bytes < sizeof(AbClassicDestructionStatistics))
        return ab_fail(error, "Invalid classic destruction statistics bindings");
    size_t remaining = config.storage_bytes - sizeof(AbClassicDestructionStatistics);
    if (config.notifications > remaining / sizeof(AbDataTable))
        return ab_fail(error, "Destruction event tables exceed storage bound");
    remaining -= config.notifications * sizeof(AbDataTable);
    if (config.notifications &&
        config.event_fields > remaining / sizeof(AbDataEntry) / config.notifications)
        return ab_fail(error, "Destruction event fields exceed storage bound");
    AbClassicDestructionStatistics *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate classic destruction statistics");
    owner->config = config;
    owner->level = level;
    owner->bytes = sizeof(*owner) +
                   config.notifications * (sizeof(AbDataTable) +
                                           config.event_fields * sizeof(AbDataEntry));
    if (config.notifications) {
        owner->events = calloc(config.notifications, sizeof(*owner->events));
        owner->fields =
            calloc(config.notifications * config.event_fields, sizeof(*owner->fields));
        if (!owner->events || !owner->fields) {
            ab_classic_destruction_statistics_close(owner);
            return ab_fail(error, "Cannot allocate destruction statistics events");
        }
        for (size_t index = 0; index < config.notifications; ++index)
            owner->events[index] =
                (AbDataTable){owner->fields + index * config.event_fields, 0,
                              config.event_fields, NULL};
    }
    *output = owner;
    return true;
}

void ab_classic_destruction_statistics_close(AbClassicDestructionStatistics *owner) {
    if (owner) {
        free(owner->fields);
        free(owner->events);
        free(owner);
    }
}

size_t
ab_classic_destruction_statistics_bytes(const AbClassicDestructionStatistics *owner) {
    return owner ? owner->bytes : 0;
}

bool ab_classic_destruction_statistics_call(void *context, size_t object,
                                            AbError *error) {
    AbClassicDestructionStatistics *owner = context;
    if (!owner || owner->calling)
        return ab_fail(error, "Invalid or reentrant destruction statistics update");
    owner->calling = true;
    bool okay = update(owner, object, error);
    owner->calling = false;
    return okay;
}
