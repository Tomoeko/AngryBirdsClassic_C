#include "ab/classic_block_scripts.h"

#include <stdlib.h>
#include <string.h>

enum { SCRIPT_CLASSES = 3 };

typedef struct {
    AbBlockScript script;
    AbClassicBlockScripts *owner;
    const char *component;
} BlockClass;

struct AbClassicBlockScripts {
    AbClassicBlockScriptConfig config;
    AbClassicLevelView level;
    BlockClass classes[SCRIPT_CLASSES];
    AbBlockScript *registered[SCRIPT_CLASSES], *initialized[SCRIPT_CLASSES];
    AbBlockScriptManager manager;
    AbBlockEventObject *objects;
    AbBlockEventScript **attachments;
    AbTrackedBlockScriptObject *tracked;
    AbBlockScriptUpdate update;
    AbPhysicsScriptStep physics;
    size_t count, capacity;
    bool failed;
};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(AbSpan name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = name};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(span(name)));
    return value ? *value : (AbDataValue){0};
}

static bool can_apply(AbBlockScript *script, const AbDataTable *definition,
                      bool *applicable, AbError *error) {
    BlockClass *class = script->events.context;
    if (!definition || !applicable)
        return ab_fail(error, "Missing actual block-script definition/output");
    AbDataValue components = field(definition, "components");
    *applicable = false;
    if (components.kind != AB_DATA_TABLE)
        return true;
    if (!components.table)
        return ab_fail(error, "Invalid actual block components table");
    AbDataValue value = field(components.table, class->component);
    if (value.kind == AB_DATA_TABLE && !value.table)
        return ab_fail(error, "Invalid actual block-script component table");
    *applicable = value.kind == AB_DATA_TABLE;
    return true;
}

static bool apply_active(AbBlockScript *script, AbBlockEventObject *object,
                         const AbDataTable *definition, AbError *error) {
    BlockClass *class = script->events.context;
    (void)object;
    (void)definition;
    class->owner->failed = true;
    return ab_fail(error, "Active %s requires its concrete block-script class owner",
                   class->component);
}

static bool update_unapplied(AbBlockScript *script, float seconds, float time,
                             AbError *error) {
    BlockClass *class = script->events.context;
    AbClassicBlockScripts *owner = class->owner;
    (void)seconds;
    (void)time;
    if (owner->failed)
        return ab_fail(error, "Stopped default block-script class owner");
    if (!*owner->config.physics_enabled)
        return true;
    /* Both source update methods reach an empty pairs traversal here. No class
     * instance was applied; a reached active application stops this owner. */
    return true;
}

static bool dispose_unapplied(AbBlockScript *script, AbError *error) {
    BlockClass *class = script->events.context;
    return !class->owner->failed ||
           ab_fail(error, "Stopped default block-script disposal owner");
}

static bool definition(void *context, AbBlockEventObject *object,
                       const AbDataTable **output, AbError *error) {
    AbClassicBlockScripts *owner = context;
    AbDataValue value;
    if (!object || !output || owner->failed ||
        !ab_object_definition_lookup(&owner->level.properties->lookup, object->name,
                                     &value, error))
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Missing live block-script object definition");
    if (value.kind != AB_DATA_TABLE || !value.table)
        return ab_fail(error, "Block-script object has no actual definition table");
    *output = value.table;
    return true;
}

static bool world_truthy(void *context, AbSpan name, bool *present, AbError *error) {
    AbClassicBlockScripts *owner = context;
    if (owner->failed || !present)
        return ab_fail(error, "Invalid or stopped block-script world lookup");
    const AbDataValue *value = ab_data_get(&owner->level.properties->world, text(name));
    *present = value && value->kind != AB_DATA_NIL &&
               (value->kind != AB_DATA_BOOLEAN || value->bits);
    return true;
}

bool ab_classic_block_scripts_open(AbClassicBlockScriptConfig config,
                                   AbClassicBlockScripts **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing default block-script output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    size_t capacity = config.object_capacity ? config.object_capacity
                                             : (level.bodies ? level.bodies->count : 0);
    const size_t stride = sizeof(AbBlockEventObject) +
                          SCRIPT_CLASSES * sizeof(AbBlockEventScript *) +
                          sizeof(AbTrackedBlockScriptObject);
    if (!level.bodies || !level.properties || !config.physics_enabled ||
        config.storage_bytes < sizeof(AbClassicBlockScripts) ||
        capacity < level.bodies->count || capacity > level.bodies->capacity ||
        capacity > (config.storage_bytes - sizeof(AbClassicBlockScripts)) / stride)
        return ab_fail(error, "Invalid default block-script owners or storage bound");
    AbClassicBlockScripts *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate default block-script owner");
    owner->config = config;
    owner->level = level;
    size_t count = level.bodies->count;
    owner->count = count;
    owner->capacity = capacity;
    owner->objects = calloc(capacity, sizeof(*owner->objects));
    owner->attachments = calloc(capacity, SCRIPT_CLASSES * sizeof(*owner->attachments));
    owner->tracked = calloc(capacity, sizeof(*owner->tracked));
    if (count && (!owner->objects || !owner->attachments || !owner->tracked)) {
        ab_classic_block_scripts_close(owner);
        return ab_fail(error, "Cannot allocate actual block-script attachment rows");
    }
    owner->manager =
        (AbBlockScriptManager){.registered = {owner->registered, 0, SCRIPT_CLASSES},
                               .initialized = {owner->initialized, 0, SCRIPT_CLASSES},
                               .tracked = owner->tracked,
                               .capacity = capacity};
    AbBlockEventHost host = {owner, definition, world_truthy};
    owner->update = (AbBlockScriptUpdate){.host = host, .event = span("onUpdateBlock")};
    owner->physics = (AbPhysicsScriptStep){.host = host,
                                           .event = span("onUpdateBlockPhysicsStep"),
                                           .context = config.context,
                                           .level_step = config.level_step};
    const char *const components[] = {"flame", "ambientAnimations", "ambientParticles"};
    for (size_t index = 0; index < SCRIPT_CLASSES; ++index) {
        BlockClass *class = owner->classes + index;
        class->owner = owner;
        class->component = components[index];
        class->script = (AbBlockScript){.events.context = class,
                                        .can_apply = can_apply,
                                        .apply = apply_active,
                                        .update = index ? update_unapplied : NULL,
                                        .dispose = index ? dispose_unapplied : NULL};
        if (!ab_block_script_register(&owner->manager, &class->script, error)) {
            ab_classic_block_scripts_close(owner);
            return false;
        }
    }
    for (size_t index = 0; index < count; ++index) {
        const AbLevelObject *object = level.bodies->objects + index;
        owner->objects[index] =
            (AbBlockEventObject){.name = object->name,
                                 .definition = object->definition_name,
                                 .scripts = owner->attachments + index * SCRIPT_CLASSES,
                                 .capacity = SCRIPT_CLASSES};
        /* Museum constructs ground directly through GameObject; only the
         * ordinary ObjectFunctions factory applies BlockScriptManager. */
        if (object->name.size == 6 && !memcmp(object->name.data, "ground", 6))
            continue;
        if (!ab_block_scripts_apply(&owner->manager, owner->objects + index, host,
                                    error)) {
            ab_classic_block_scripts_close(owner);
            return false;
        }
    }
    *output = owner;
    return true;
}

void ab_classic_block_scripts_close(AbClassicBlockScripts *scripts) {
    if (!scripts)
        return;
    free(scripts->tracked);
    free(scripts->attachments);
    free(scripts->objects);
    free(scripts);
}

AbClassicBlockScriptView ab_classic_block_scripts_view(AbClassicBlockScripts *scripts) {
    return scripts ? (AbClassicBlockScriptView){.manager = &scripts->manager,
                                                .objects = scripts->objects,
                                                .count = scripts->count,
                                                .update = &scripts->update,
                                                .physics = &scripts->physics,
                                                .host = scripts->update.host}
                   : (AbClassicBlockScriptView){0};
}

bool ab_classic_block_scripts_register_circle(AbClassicBlockScripts *scripts,
                                              size_t object, AbError *error) {
    if (!scripts || scripts->failed || object != scripts->count ||
        object >= scripts->capacity || object >= scripts->level.bodies->count)
        return ab_fail(error, "Invalid direct-circle block-script row");
    const AbLevelObject *source = scripts->level.bodies->objects + object;
    scripts->objects[object] =
        (AbBlockEventObject){.name = source->name,
                             .definition = source->definition_name,
                             .scripts = scripts->attachments + object * SCRIPT_CLASSES,
                             .capacity = SCRIPT_CLASSES};
    ++scripts->count;
    return true;
}
