#include "ab/classic_balloons.h"

#include <stdlib.h>
#include <string.h>

struct AbClassicBalloons {
    AbClassicBalloonConfig config;
    AbClassicLevelView level;
    AbClassicGameUpdateFunction function;
    AbStringMap *members;
    AbDataValue *values;
    size_t bytes;
    bool updating, failed;
};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)};
}

static AbDataValue field(const AbDataTable *table, AbDataValue key) {
    const AbDataValue *value = ab_data_get(table, key);
    return value ? *value : (AbDataValue){0};
}

static bool balloon_definition(AbDataValue value, bool *selected, AbError *error) {
    if (value.kind != AB_DATA_TABLE || !value.table)
        return ab_fail(error, "Missing initial balloon object definition");
    AbDataValue material = field(value.table, text("materialName"));
    *selected = material.kind == AB_DATA_STRING && material.string.size == 7 &&
                material.string.data && !memcmp(material.string.data, "balloon", 7);
    return true;
}

bool ab_classic_balloons_created(AbClassicBalloons *owner, size_t object,
                                 AbError *error) {
    if (!owner || owner->updating || owner->failed ||
        object >= owner->level.bodies->count)
        return ab_fail(error, "Invalid canonical balloon creator binding");
    return ab_string_map_set(owner->members, owner->level.bodies->objects[object].name,
                             (uintptr_t)(owner->values + object), true, error);
}

static bool update(AbClassicBalloons *owner, AbError *error) {
    size_t cursor = 0;
    for (;;) {
        AbStringMapEntry entry;
        if (!ab_string_map_next(owner->members, &cursor, &entry, error))
            return false;
        if (!entry.present)
            return true;
        AbDataValue name = {.kind = AB_DATA_STRING, .string = entry.key};
        AbDataValue live = field(&owner->level.properties->world, name);
        if (live.kind == AB_DATA_NIL) {
            if (!ab_string_map_set(owner->members, entry.key, 0, false, error))
                return false;
            continue;
        }
        const AbDataValue *value = (const AbDataValue *)entry.value;
        if (!value || value->kind != AB_DATA_TABLE || !value->table)
            return ab_fail(error, "Invalid stored canonical balloon value");
        AbDataValue position = field(value->table, text("y"));
        if (position.kind != AB_DATA_NUMBER)
            return ab_fail(error, "Nonnumeric canonical balloon height");
        float y = 0;
        memcpy(&y, &position.bits, sizeof(y));
        if (y < -200 && !ab_string_map_set(owner->config.dead_blocks, entry.key,
                                           entry.value, true, error))
            return false;
    }
}

bool ab_classic_balloons_update(AbClassicBalloons *owner, AbError *error) {
    if (!owner || owner->updating || owner->failed)
        return ab_fail(error, "Invalid, failed or reentrant balloon update");
    owner->updating = true;
    bool okay = update(owner, error);
    owner->updating = false;
    owner->failed = !okay;
    return okay;
}

static bool registered_update(void *context, float seconds, float time,
                              AbError *error) {
    (void)seconds;
    (void)time;
    return ab_classic_balloons_update(context, error);
}

bool ab_classic_balloons_open(AbClassicBalloonConfig config, AbClassicBalloons **output,
                              AbError *error) {
    if (!output)
        return ab_fail(error, "Missing canonical balloon owner output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    if (!level.bodies || !level.properties || !config.updates || !config.dead_blocks ||
        config.storage_bytes < sizeof(AbClassicBalloons) ||
        level.bodies->count >
            (config.storage_bytes - sizeof(AbClassicBalloons)) / sizeof(AbDataValue))
        return ab_fail(error, "Invalid canonical balloon bindings or byte limit");
    const AbClassicGameUpdateFunction *prior = NULL;
    if (!ab_classic_game_updates_lookup(config.updates, span("removeBalloonsTooHigh"),
                                        &prior, error) ||
        prior)
        return prior ? ab_fail(error, "Balloon init requires a cleared update registry")
                     : false;
    AbClassicBalloons *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate canonical balloon owner");
    owner->config = config;
    owner->level = level;
    owner->bytes = sizeof(*owner) + level.bodies->count * sizeof(AbDataValue);
    owner->values = calloc(level.bodies->count, sizeof(*owner->values));
    if (!owner->values ||
        !ab_string_map_open(0, config.nodes, config.storage_bytes - owner->bytes,
                            &owner->members, error)) {
        ab_classic_balloons_close(owner);
        return ab_fail(error, "Cannot allocate canonical balloon registry");
    }
    owner->bytes += ab_string_map_bytes(owner->members);
    for (size_t object = 0; object < level.bodies->count; ++object)
        owner->values[object] =
            (AbDataValue){.kind = AB_DATA_TABLE,
                          .table = &level.properties->objects[object].physical};
    AbDataValue block_table = level.properties->lookup.block_table;
    AbDataValue definitions = block_table.kind == AB_DATA_TABLE
                                  ? field(block_table.table, text("blocks"))
                                  : (AbDataValue){0};
    if (definitions.kind != AB_DATA_TABLE) {
        ab_classic_balloons_close(owner);
        return ab_fail(error, "Missing initial balloon catalog");
    }
    for (size_t object = 0; object + 1 < level.bodies->count; ++object) {
        AbDataValue name = {.kind = AB_DATA_STRING,
                            .string = level.bodies->objects[object].definition_name};
        bool selected = false;
        if (!balloon_definition(field(definitions.table, name), &selected, error) ||
            (selected && !ab_classic_balloons_created(owner, object, error))) {
            ab_classic_balloons_close(owner);
            return false;
        }
    }
    owner->function = (AbClassicGameUpdateFunction){owner, registered_update};
    if (!ab_classic_game_updates_add(config.updates, span("removeBalloonsTooHigh"),
                                     &owner->function, error)) {
        ab_classic_balloons_close(owner);
        return false;
    }
    *output = owner;
    return true;
}

void ab_classic_balloons_close(AbClassicBalloons *owner) {
    if (owner) {
        ab_string_map_close(owner->members);
        free(owner->values);
        free(owner);
    }
}

size_t ab_classic_balloons_bytes(const AbClassicBalloons *owner) {
    return owner ? owner->bytes : 0;
}

const AbStringMap *ab_classic_balloons_members(const AbClassicBalloons *owner) {
    return owner ? owner->members : NULL;
}
