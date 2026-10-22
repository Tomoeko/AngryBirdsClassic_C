#include "ab/classic_removals.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const AbDataTable *source;
    AbDataTable table;
} RemovalDefinition;

struct AbClassicRemovals {
    AbClassicRemovalConfig config;
    AbClassicLevelView level;
    RemovalDefinition *definitions;
    AbDataEntry *fields;
    AbDataEntry block_entry;
    AbDataTable block_table;
    AbBlockRemovalScores scores;
    float published_total;
    size_t published_count;
    AbBlockRemovalState state;
    AbBlockRemovalCleanupState cleanup;
    AbObjectRemovalNotifyHost *notifying;
    size_t count, bytes;
    bool calling;
};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(AbSpan name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = name};
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue field(const AbDataTable *value, const char *name) {
    const AbDataValue *found = ab_data_get(value, text(span(name)));
    return found ? *found : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool equal(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || (first.data && second.data &&
                            !memcmp(first.data, second.data, first.size)));
}

static void refresh_scores(AbClassicRemovals *owner) {
    AbBlockCollisionScores *shared = owner->config.scores;
    AbBlockRemovalScores *destruction = owner->config.destruction_scores;
    destruction->total = (AbScoreNumber){shared->total, true};
    owner->scores = *destruction;
    owner->published_total = shared->total;
    owner->published_count = destruction->count;
}

static void publish_scores(AbClassicRemovals *owner) {
    AbBlockCollisionScores *shared = owner->config.scores;
    if (memcmp(&owner->scores.total.value, &owner->published_total, sizeof(float)))
        shared->total = owner->scores.total.value;
    if (owner->scores.count != owner->published_count)
        owner->config.destruction_scores->count = owner->scores.count;
    refresh_scores(owner);
}

static bool finish(AbClassicRemovals *owner, bool okay) {
    refresh_scores(owner);
    owner->cleanup.level_name = *owner->config.level_name;
    if (owner->notifying)
        owner->notifying->level_name = *owner->config.level_name;
    return okay;
}

static bool object_index(AbClassicRemovals *owner, AbDataValue object, size_t *output,
                         AbError *error) {
    for (size_t index = 0; object.kind == AB_DATA_TABLE && index < owner->count;
         ++index) {
        if (object.table == &owner->level.properties->objects[index].physical) {
            *output = index;
            return true;
        }
    }
    return ab_fail(error, "Foreign classic removal object");
}

static bool script_index(AbClassicRemovals *owner, AbDataValue object, size_t *output,
                         AbError *error) {
    size_t index = SIZE_MAX;
    if (!object_index(owner, object, &index, error))
        return false;
    AbDataValue script = field(object.table, "gameObjectScript");
    if (script.kind != AB_DATA_TABLE ||
        script.table != &owner->level.properties->objects[index].script)
        return ab_fail(error, "Rebound classic removal script receiver");
    *output = index;
    return true;
}

static bool destroy(void *context, AbDataValue object, AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    size_t index = SIZE_MAX;
    return script_index(owner, object, &index, error) &&
           finish(owner,
                  ab_classic_behaviors_destroy(owner->config.behaviors, index, error));
}

static bool resolved(AbClassicRemovals *owner, size_t index, AbObjectDefinition *output,
                     AbError *error) {
    AbDataValue name =
        field(&owner->level.properties->objects[index].physical, "definition");
    return name.kind == AB_DATA_STRING
               ? ab_object_definition_open(owner->level.catalog, name.string, NULL,
                                           output, error)
               : ab_fail(error, "Invalid classic removal definition name");
}

static bool normalize(AbClassicRemovals *owner, const AbObjectDefinition *definition,
                      AbDataValue *output, AbError *error) {
    RemovalDefinition *entry = NULL;
    for (size_t index = 0; index < owner->count; ++index) {
        if (owner->definitions[index].source == definition->source) {
            *output = table(&owner->definitions[index].table);
            return true;
        }
        if (!entry && !owner->definitions[index].source)
            entry = owner->definitions + index;
    }
    if (!entry || definition->source->count > entry->table.capacity)
        return ab_fail(error, "Classic removal definition storage exhausted");
    entry->table.count = definition->source->count;
    memcpy(entry->table.entries, definition->source->entries,
           entry->table.count * sizeof(*entry->table.entries));
    const AbDataTable *material = definition->material;
    for (size_t index = 0; material && index < material->count; ++index) {
        AbDataValue key = material->entries[index].key;
        const AbDataValue *prior = ab_data_get(&entry->table, key);
        if (key.kind != AB_DATA_STRING || (prior && prior->kind != AB_DATA_NIL))
            continue;
        AbDataValue value = ab_object_property(definition, key.string);
        if (value.kind == AB_DATA_NIL)
            continue;
        AbObjectPropertyAccess access = {.physics_object = table(&entry->table)};
        if (!ab_object_property_set(&access, key.string, value, error))
            return false;
    }
    entry->source = definition->source;
    *output = table(&entry->table);
    return true;
}

static bool definition(void *context, AbSpan name, AbDataValue *output,
                       AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    AbDataValue raw;
    if (!ab_object_definition_lookup(&owner->level.properties->lookup, name, &raw,
                                     error))
        return false;
    for (size_t index = 0; index < owner->count; ++index) {
        if (!equal(owner->level.bodies->objects[index].name, name))
            continue;
        AbObjectDefinition value;
        if (!resolved(owner, index, &value, error))
            return false;
        if (raw.kind != AB_DATA_TABLE || raw.table != value.source)
            return ab_fail(error, "Rebound classic removal definition lookup");
        return normalize(owner, &value, output, error);
    }
    return ab_fail(error, "Unknown classic removal definition object");
}

static bool score(void *context, AbDataValue object, AbScoreNumber *output,
                  AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    size_t index = SIZE_MAX;
    AbObjectDefinition value;
    if (!object_index(owner, object, &index, error) ||
        !resolved(owner, index, &value, error))
        return false;
    AbDataValue merged = ab_object_property(&value, span("destroyedScoreInc"));
    AbDataValue base = field(value.source, "destroyedScoreInc");
    AbScoreNumber override = {.present = base.kind != AB_DATA_NIL};
    if (override.present) {
        if (base.kind != AB_DATA_NUMBER)
            return ab_fail(error, "Nonnumeric classic base breaking score");
        memcpy(&override.value, &base.bits, sizeof(override.value));
    }
    AbBreakScoreInput input = {.block_increment = owner->config.block_increment,
                               .goal_increment = owner->config.goal_increment,
                               .base_override = override,
                               .merged_override_present = merged.kind != AB_DATA_NIL,
                               .level_goal_truthy =
                                   truthy(field(object.table, "levelGoal"))};
    return ab_break_score(&input, output) ||
           ab_fail(error, "Invalid classic removal breaking score");
}

static bool audio_name(void *context, AbDataValue name, AbDataValue *output,
                       AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    return ab_audio_bank_name_value(owner->config.audio, name, output, error);
}

static bool audio(void *context, AbAudioPlay request, AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    uint32_t handle;
    return ab_audio_bank_play(owner->config.audio, request, &handle, error);
}

static bool floating(void *context, const AbBlockRemovalFloatingScore *request,
                     AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    return ab_floating_scores_add_removal(owner->config.floating, request, error);
}

static bool property(void *context, AbDataValue object, AbSpan key, AbDataValue *output,
                     AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    size_t index = SIZE_MAX;
    return script_index(owner, object, &index, error) &&
           ab_level_property_get(owner->level.properties, index, key, output, error);
}

static bool random_integer(void *context, float low, float high, AbDataValue *output,
                           AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    float value;
    if (ab_script_random(owner->config.random, 2, (int32_t)low, (int32_t)high,
                         &value) != AB_SCRIPT_RANDOM_OK)
        return ab_fail(error, "Classic removal random interval failed");
    *output = number(value);
    return true;
}

static bool destroyed_sound(void *context, AbDataValue object, AbDataValue definition,
                            AbDataValue *output, AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    size_t index = SIZE_MAX;
    if (!object_index(owner, object, &index, error))
        return false;
    return owner->config.destroyed_sound
               ? finish(owner,
                        owner->config.destroyed_sound(owner->config.context, index,
                                                      definition, output, error))
               : ab_fail(error, "Missing actual classic destroyed-sound policy");
}

static bool update_destroyed(void *context, AbDataValue object, AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    size_t index = SIZE_MAX;
    if (!object_index(owner, object, &index, error))
        return false;
    return owner->config.update_destroyed
               ? finish(owner, owner->config.update_destroyed(owner->config.context,
                                                              index, error))
               : ab_fail(error, "Missing actual classic destruction statistics");
}

static bool notice_binding(void *context, AbDataValue key, AbUiPointerValue *output,
                           AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    AbObjectRemovalNotifyHost *host = owner->config.notifications;
    if (!host->binding)
        return ab_fail(error, "Missing actual removal environment lookup");
    return finish(owner, host->binding(host->context, key, output, error));
}

static bool notice_level(void *context, AbDataValue name, AbObjectRemovalLevel **output,
                         AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    AbObjectRemovalNotifyHost *host = owner->config.notifications;
    if (!host->level)
        return ab_fail(error, "Missing actual removal level registry lookup");
    return finish(owner, host->level(host->context, name, output, error));
}

static bool notice_call(void *context, AbObjectRemovalNotice notice,
                        AbUiPointerValue function, AbObjectRemovalObject *object,
                        AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    AbObjectRemovalNotifyHost *host = owner->config.notifications;
    if (!host->call)
        return ab_fail(error, "Missing actual removal notification method");
    return finish(owner, host->call(host->context, notice, function, object, error));
}

static bool notify(void *context, AbDataValue object, AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    size_t index = SIZE_MAX;
    if (!object_index(owner, object, &index, error))
        return false;
    AbObjectRemovalNotifyHost *host = owner->config.notifications;
    if (!host)
        return ab_fail(error, "Missing actual classic removal notification registry");
    AbObjectRemovalNotifyHost bridge = {owner,
                                        table(&owner->block_table),
                                        *owner->config.level_name,
                                        notice_binding,
                                        notice_level,
                                        notice_call};
    AbObjectRemovalObject receiver = {field(object.table, "definition"), object.table};
    owner->notifying = &bridge;
    bool okay = ab_object_removal_notify(&receiver, &bridge, error);
    owner->notifying = NULL;
    return finish(owner, okay);
}

static bool functions(void *context, AbDataValue object, AbUiPointerValue *output,
                      AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    if (field(object.table, "removeFunctions").kind == AB_DATA_NIL) {
        *output = (AbUiPointerValue){0};
        return true;
    }
    size_t index = SIZE_MAX;
    if (!object_index(owner, object, &index, error))
        return false;
    return owner->config.functions
               ? finish(owner, owner->config.functions(owner->config.context, index,
                                                       output, error))
               : ab_fail(error, "Missing actual classic removal-function list");
}

static bool call(void *context, AbUiPointerValue function, AbDataValue object,
                 AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    size_t index = SIZE_MAX;
    if (!object_index(owner, object, &index, error))
        return false;
    return owner->config.call
               ? finish(owner, owner->config.call(owner->config.context, function,
                                                  index, error))
               : ab_fail(error, "Missing actual classic removal-function service");
}

static bool new_particles(void *context, AbSpan name, AbDataValue descriptor,
                          bool flag1, bool flag2, AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    return ab_classic_particles_removal(owner->config.particles, name, descriptor,
                                        flag1, flag2, error);
}

static bool old_particles(void *context, AbSpan name, AbDataValue particles,
                          AbDataValue amount, bool flag1, bool flag2, AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    return ab_classic_particles_legacy(owner->config.particles, name, particles, amount,
                                       flag1, flag2, error);
}

static bool remove_object(void *context, AbSpan name, AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    return finish(owner,
                  ab_classic_object_removal_call(owner->config.physical, name, error));
}

static bool remove_pig(void *context, AbSpan name, AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    return owner->config.remove_pig
               ? finish(owner,
                        owner->config.remove_pig(owner->config.context, name, error))
               : ab_fail(error, "Missing actual classic removePig service");
}

static bool rocket(void *context, AbDataValue name, AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    return owner->config.rocket
               ? finish(owner, owner->config.rocket(owner->config.context, name, error))
               : ab_fail(error, "Missing actual classic rocket service");
}

static bool explosion(void *context, AbDataValue object, AbDataValue definition,
                      AbDataValue sound, AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    return owner->config.explosion
               ? finish(owner, owner->config.explosion(owner->config.context, object,
                                                       definition, sound, error))
               : ab_fail(error, "Missing actual classic explosion service");
}

static bool gift(void *context, AbDataValue level, AbDataValue x, AbDataValue y,
                 AbError *error) {
    AbClassicRemovals *owner = context;
    publish_scores(owner);
    return owner->config.collect_gift
               ? finish(owner, owner->config.collect_gift(owner->config.context, level,
                                                          x, y, error))
               : ab_fail(error, "Missing actual classic gift service");
}

bool ab_classic_removals_open(AbClassicRemovalConfig config, AbClassicRemovals **output,
                              AbError *error) {
    if (!output)
        return ab_fail(error, "Missing classic removal output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    size_t count = level.bodies ? level.bodies->count : 0;
    if (!count || !level.properties || !level.catalog || !config.level_name ||
        !config.dead_blocks || !config.level_goals || !config.scores ||
        !config.destruction_scores || !config.definition_fields ||
        config.storage_bytes < sizeof(AbClassicRemovals))
        return ab_fail(error, "Invalid classic removal source or limits");
    size_t remaining = config.storage_bytes - sizeof(AbClassicRemovals);
    if (count > remaining / sizeof(RemovalDefinition))
        return ab_fail(error, "Classic removal definition rows exceed byte bound");
    remaining -= count * sizeof(RemovalDefinition);
    if (config.definition_fields > remaining / sizeof(AbDataEntry) / count)
        return ab_fail(error, "Classic removal definition fields exceed byte bound");
    AbClassicRemovals *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate classic removals");
    owner->config = config;
    owner->level = level;
    owner->count = count;
    owner->bytes = sizeof(*owner) + count * sizeof(RemovalDefinition) +
                   count * config.definition_fields * sizeof(AbDataEntry);
    owner->definitions = calloc(count, sizeof(*owner->definitions));
    owner->fields = calloc(count * config.definition_fields, sizeof(*owner->fields));
    if (!owner->definitions || !owner->fields) {
        ab_classic_removals_close(owner);
        return ab_fail(error, "Cannot allocate classic removal definitions");
    }
    for (size_t index = 0; index < count; ++index)
        owner->definitions[index].table =
            (AbDataTable){owner->fields + index * config.definition_fields, 0,
                          config.definition_fields, NULL};
    owner->block_entry =
        (AbDataEntry){text(span("blocks")), field(level.catalog, "blocks")};
    owner->block_table = (AbDataTable){&owner->block_entry, 1, 1, NULL};
    owner->state =
        (AbBlockRemovalState){table(&level.properties->registry), config.dead_blocks};
    *output = owner;
    return true;
}

void ab_classic_removals_close(AbClassicRemovals *owner) {
    if (owner) {
        free(owner->fields);
        free(owner->definitions);
        free(owner);
    }
}

size_t ab_classic_removals_bytes(const AbClassicRemovals *owner) {
    return owner ? owner->bytes : 0;
}

bool ab_classic_removals_run(AbClassicRemovals *owner, AbError *error) {
    if (!owner || owner->calling)
        return ab_fail(error, "Invalid or reentrant classic removal pass");
    owner->calling = true;
    refresh_scores(owner);
    AbBlockRemovalHost traversal = {owner, rocket};
    AbBlockRemovalScoreHost scoring = {
        owner,         destroy, definition, score,
        audio_name,    audio,   floating,   {owner->config.floating_scaling, true},
        &owner->scores};
    AbBlockRemovalEffectsHost effects = {
        owner,     &owner->state, property, destroyed_sound, random_integer,
        explosion, audio_name,    audio,    update_destroyed};
    AbBlockRemovalParticlesHost particles = {
        owner, notify, functions, call, property, new_particles, old_particles};
    AbBlockRemovalDetachHost detach = {owner, table(&owner->block_table),
                                       remove_object};
    owner->cleanup = (AbBlockRemovalCleanupState){
        &owner->state, owner->config.level_goals, *owner->config.level_name};
    AbBlockRemovalCleanupHost cleanup = {owner, remove_pig, gift};
    AbBlockRemovalPassHost host = {&traversal,      &scoring, &effects,
                                   &particles,      &detach,  owner->config.spawns,
                                   &owner->cleanup, &cleanup};
    bool okay = ab_block_removal_pass(&owner->state, &host, error);
    publish_scores(owner);
    owner->calling = false;
    return okay;
}

static bool clear_map(AbDataTable *value, AbSpan name, AbError *error) {
    AbObjectPropertyAccess access = {.physics_object = table(value)};
    return ab_object_property_set(&access, name, (AbDataValue){0}, error);
}

static bool standalone_pig(AbClassicRemovals *owner, AbSpan name, AbError *error) {
    AbDataValue object = {0};
    if (!ab_object_world_lookup(&owner->level.properties->lookup, text(name), &object,
                                error))
        return false;
    if (truthy(object)) {
        AbScoreNumber increment = {0};
        if (!score(owner, object, &increment, error))
            return false;
        if (!increment.present)
            return ab_fail(error, "Absent standalone pig breaking score");
        AbBlockCollisionScores *shared = owner->config.scores;
        shared->total = shared->total + increment.value;
        AbBlockRemovalScores *destruction = owner->config.destruction_scores;
        if (destruction->count >= destruction->capacity || !destruction->entries)
            return ab_fail(error, "Standalone pig score append exhausted");
        destruction->entries[destruction->count++] = increment.value;
        refresh_scores(owner);
        if (!ab_object_world_lookup(&owner->level.properties->lookup, text(name),
                                    &object, error) ||
            !notify(owner, object, error) || !remove_object(owner, name, error) ||
            !clear_map(&owner->level.properties->world, name, error) ||
            !clear_map(&owner->level.properties->dynamic, name, error) ||
            !ab_string_map_set(owner->config.level_goals, name, 0, false, error))
            return false;
    }
    return remove_pig(owner, name, error);
}

bool ab_classic_removals_remove_pig(void *context, AbSpan name, AbError *error) {
    AbClassicRemovals *owner = context;
    if (!owner || owner->calling)
        return ab_fail(error, "Invalid or reentrant standalone pig removal");
    owner->calling = true;
    refresh_scores(owner);
    bool okay = standalone_pig(owner, name, error);
    publish_scores(owner);
    owner->calling = false;
    return okay;
}
