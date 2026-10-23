#include "ab/classic_behaviors.h"

#include <stdlib.h>
#include <string.h>

enum { MUSEUM_CLASSES = 2, MUSEUM_METHODS = 4 };

typedef struct {
    AbMuseumBirdBehavior instance;
    AbGameBehavior dispatch;
    AbGameBehaviorMethod methods[MUSEUM_METHODS];
    AbMuseumBirdHost *host;
} ClassicBehavior;

typedef struct {
    AbClassicBehaviors *owner;
    AbStringMap *groups;
    AbGameBehaviorGroup new_groups[MUSEUM_CLASSES];
    AbGameBehavior *items[MUSEUM_CLASSES];
    ClassicBehavior instances[MUSEUM_CLASSES];
    size_t instance_count, group_count;
    AbDataEntry group_entries[MUSEUM_CLASSES], item_entries[MUSEUM_CLASSES];
    AbDataTable group_tables[MUSEUM_CLASSES];
    bool published;
} ClassicBehaviorObject;

struct AbClassicBehaviors {
    AbClassicLevelView level;
    AbClassicBehaviorConfig config;
    AbMuseumBirdHost host;
    ClassicBehaviorObject *objects;
    size_t count, capacity, bytes;
    bool constructing;
};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)};
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static bool valid_table(const AbDataTable *value) {
    return value && value->count <= value->capacity &&
           (!value->count || value->entries);
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static AbDataValue field(const AbDataTable *value, const char *name) {
    const AbDataValue *found = ab_data_get(value, text(name));
    return found ? *found : (AbDataValue){0};
}

static bool service(void *context, const AbMuseumBirdCall *call, AbError *error) {
    AbClassicBehaviors *owner = context;
    if (!owner->config.call)
        return ab_fail(error, "Missing classic behavior service");
    bool okay = owner->config.call(owner->config.context, call, error);
    owner->host.flying_bird = *owner->config.flying_bird;
    return okay;
}

static bool get_property(void *context, AbDataValue script, AbSpan key,
                         AbDataValue *output, AbError *error) {
    AbClassicBehaviors *owner = context;
    for (size_t index = 0; index < owner->count; ++index) {
        if (script.kind == AB_DATA_TABLE &&
            script.table == &owner->level.properties->objects[index].script)
            return ab_level_property_get(owner->level.properties, index, key, output,
                                         error);
    }
    return ab_fail(error, "Foreign classic behavior script receiver");
}

static bool launch(void *context, const AbUiPointerValue *arguments, size_t count,
                   AbError *error) {
    (void)arguments;
    (void)count;
    ClassicBehavior *behavior = context;
    return ab_museum_bird_behavior_launch(&behavior->instance, behavior->host, error);
}

static bool specialty(void *context, const AbUiPointerValue *arguments, size_t count,
                      AbError *error) {
    (void)arguments;
    (void)count;
    ClassicBehavior *behavior = context;
    return ab_museum_bird_behavior_specialty(&behavior->instance, behavior->host,
                                             error);
}

static bool destroy(void *context, const AbUiPointerValue *arguments, size_t count,
                    AbError *error) {
    (void)arguments;
    (void)count;
    ClassicBehavior *behavior = context;
    return ab_museum_bird_behavior_destroy(&behavior->instance, behavior->host, error);
}

static bool collision(void *context, const AbUiPointerValue *arguments, size_t count,
                      AbError *error) {
    if (count < 2 || arguments[1].kind != AB_UI_POINTER_OBJECT || !arguments[1].object)
        return ab_fail(error, "Missing classic behavior collision object");
    ClassicBehavior *behavior = context;
    return ab_museum_bird_behavior_collision(&behavior->instance,
                                             table((AbDataTable *)arguments[1].object),
                                             behavior->host, error);
}

static bool publish(AbClassicBehaviors *owner, size_t index, AbError *error);

static bool create(void *context, AbSpan name, AbDataValue receiver,
                   AbDataValue properties, AbGameBehavior **output, AbError *error) {
    ClassicBehaviorObject *object = context;
    size_t index = (size_t)(object - object->owner->objects);
    if (!publish(object->owner, index, error))
        return false;
    if (object->instance_count == MUSEUM_CLASSES)
        return ab_fail(error, "Classic behavior instance storage exhausted");
    ClassicBehavior *behavior = object->instances + object->instance_count;
    if (!ab_museum_bird_behavior_initialize(name, receiver, properties,
                                            &behavior->instance, error))
        return false;
    behavior->host = &object->owner->host;
    if (!ab_museum_bird_behavior_init(&behavior->instance, behavior->host, error))
        return false;
    behavior->methods[0] =
        (AbGameBehaviorMethod){span("onLaunch"), true, behavior, launch};
    behavior->methods[1] =
        (AbGameBehaviorMethod){span("onTriggerSpecialty"), true, behavior, specialty};
    size_t method_count = 2;
    if (behavior->instance.kind != AB_MUSEUM_BIRD_CAMERA) {
        behavior->methods[2] =
            (AbGameBehaviorMethod){span("onBirdCollision"), true, behavior, collision};
        behavior->methods[3] =
            (AbGameBehaviorMethod){span("onDestroy"), true, behavior, destroy};
        method_count = 4;
    }
    behavior->dispatch = (AbGameBehavior){behavior->methods, method_count};
    if (behavior->instance.kind == AB_MUSEUM_BIRD_BLUES_CAMERA)
        behavior->dispatch.count = 0;
    ++object->instance_count;
    *output = &behavior->dispatch;
    return true;
}

static bool add_behavior(ClassicBehaviorObject *object, AbSpan name,
                         AbDataValue properties, AbError *error) {
    size_t index = (size_t)(object - object->owner->objects);
    AbGameBehaviorRegistration registration = {
        .groups = object->groups,
        .receiver = table(&object->owner->level.properties->objects[index].script),
        .new_groups = object->new_groups,
        .group_count = object->group_count,
        .group_capacity = MUSEUM_CLASSES,
        .context = object,
        .create = create};
    bool okay = ab_game_object_add_behavior(&registration, name, properties, error);
    object->group_count = registration.group_count;
    return okay;
}

static bool configure(ClassicBehaviorObject *object, AbDataValue configurations,
                      const AbLuaData *catalog, AbError *error) {
    if (!truthy(configurations))
        return true;
    if (configurations.kind != AB_DATA_TABLE || !valid_table(configurations.table) ||
        configurations.table->count > MUSEUM_CLASSES)
        return ab_fail(error, "Unsupported classic behavior configuration");
    const AbDataTableLayout *layout =
        ab_lua_table_layout(catalog, configurations.table);
    if (!layout || layout->array_hint || !layout->no_nil_assignments)
        return ab_fail(error, "Missing ordinary behavior constructor layout");
    size_t indices[MUSEUM_CLASSES];
    AbStringOrderNode nodes[MUSEUM_CLASSES];
    AbStringOrderWorkspace order = {indices, MUSEUM_CLASSES, nodes, MUSEUM_CLASSES};
    if (!ab_data_string_order(configurations.table, layout->hash_capacity, &order,
                              error))
        return false;
    for (size_t cursor = 0; cursor < configurations.table->count; ++cursor) {
        const AbDataEntry *entry = configurations.table->entries + indices[cursor];
        if (!add_behavior(object, entry->key.string, entry->value, error))
            return false;
    }
    return true;
}

static bool initialize(AbClassicBehaviors *owner, size_t index,
                       const AbLuaData *catalog, AbError *error) {
    ClassicBehaviorObject *object = owner->objects + index;
    /* MuseumMode creates ground separately without a configuration argument.
     * The body owner always appends that exact row last. */
    if (index + 1 == owner->count)
        return true;
    AbDataValue definition = {0};
    if (!ab_object_definition_lookup(&owner->level.properties->lookup,
                                     owner->level.bodies->objects[index].name,
                                     &definition, error))
        return false;
    if (definition.kind != AB_DATA_TABLE || !valid_table(definition.table))
        return ab_fail(error, "Missing classic behavior object definition");
    return configure(object, field(definition.table, "behaviors"), catalog, error);
}

static bool publish(AbClassicBehaviors *owner, size_t index, AbError *error) {
    ClassicBehaviorObject *object = owner->objects + index;
    size_t count = 0, cursor = 0;
    AbStringMapEntry entry;
    for (;;) {
        if (!ab_string_map_next(object->groups, &cursor, &entry, error))
            return false;
        if (!entry.present)
            break;
        AbGameBehaviorGroup *group = (AbGameBehaviorGroup *)entry.value;
        size_t slot = 0;
        while (slot < MUSEUM_CLASSES && group != object->new_groups + slot)
            ++slot;
        if (slot == MUSEUM_CLASSES)
            return ab_fail(error, "Foreign classic behavior group");
        size_t length = group->items[0] ? 1u : 0u;
        if (length) {
            size_t instance = 0;
            while (instance < object->instance_count &&
                   &object->instances[instance].dispatch != group->items[0])
                ++instance;
            if (instance == object->instance_count)
                return ab_fail(error, "Foreign classic behavior instance");
            object->item_entries[slot] =
                (AbDataEntry){{.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)},
                              table(&object->instances[instance].instance.fields)};
        }
        object->group_tables[slot] =
            (AbDataTable){object->item_entries + slot, length, 1, NULL};
        object->group_entries[count++] =
            (AbDataEntry){{.kind = AB_DATA_STRING, .string = entry.key},
                          table(object->group_tables + slot)};
    }
    owner->level.properties->objects[index].behaviors =
        (AbDataTable){object->group_entries, count, MUSEUM_CLASSES, NULL};
    object->published = true;
    return true;
}

void ab_classic_behaviors_close(AbClassicBehaviors *owner) {
    if (!owner)
        return;
    for (size_t index = 0; index < owner->capacity; ++index) {
        ClassicBehaviorObject *object = owner->objects + index;
        if (object->published)
            owner->level.properties->objects[index].behaviors = (AbDataTable){0};
        ab_string_map_close(object->groups);
    }
    free(owner->objects);
    free(owner);
}

bool ab_classic_behaviors_open(AbClassicLevel *level, AbClassicBehaviorConfig config,
                               AbClassicBehaviors **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing classic behavior owner output");
    *output = NULL;
    AbClassicLevelView view = ab_classic_level_view(level);
    size_t capacity = config.object_capacity ? config.object_capacity
                      : view.bodies          ? view.bodies->count
                                             : 0;
    if (!level || !view.bodies || !view.properties || !config.flying_bird ||
        config.storage_bytes < sizeof(AbClassicBehaviors) ||
        view.bodies->count != view.properties->count || !view.bodies->count ||
        capacity < view.bodies->count || capacity > view.bodies->capacity ||
        capacity > (config.storage_bytes - sizeof(AbClassicBehaviors)) /
                       sizeof(ClassicBehaviorObject))
        return ab_fail(error, "Invalid classic behavior owner bounds");
    const AbLuaData *catalog =
        config.catalog_data ? config.catalog_data : view.catalog_data;
    if (catalog && catalog->globals != view.catalog)
        return ab_fail(error, "Classic behavior catalog layouts do not match");
    for (size_t index = 0; index < view.properties->count; ++index) {
        AbDataTable *groups = &view.properties->objects[index].behaviors;
        if (groups->entries || groups->count || groups->capacity || groups->next_owned)
            return ab_fail(error, "Classic script behaviors are already bound");
    }
    AbClassicBehaviors *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate classic behavior owner");
    owner->level = view;
    owner->config = config;
    owner->count = view.bodies->count;
    owner->capacity = capacity;
    owner->bytes = sizeof(*owner) + capacity * sizeof(*owner->objects);
    owner->objects = calloc(capacity, sizeof(*owner->objects));
    if (!owner->objects) {
        free(owner);
        return ab_fail(error, "Cannot allocate classic behavior objects");
    }
    owner->host = (AbMuseumBirdHost){owner, *config.flying_bird, service, get_property,
                                     config.blues};
    for (size_t index = 0; index < capacity; ++index) {
        ClassicBehaviorObject *object = owner->objects + index;
        object->owner = owner;
        for (size_t group = 0; group < MUSEUM_CLASSES; ++group)
            object->new_groups[group] = (AbGameBehaviorGroup){object->items + group, 1};
        if (!ab_string_map_open(0, MUSEUM_CLASSES, config.storage_bytes - owner->bytes,
                                &object->groups, error)) {
            ab_classic_behaviors_close(owner);
            return false;
        }
        owner->bytes += ab_string_map_bytes(object->groups);
    }
    for (size_t index = 0; index < owner->count; ++index) {
        if (!initialize(owner, index, catalog, error)) {
            ab_classic_behaviors_close(owner);
            return false;
        }
    }
    for (size_t index = 0; index < owner->count; ++index) {
        if (!publish(owner, index, error)) {
            ab_classic_behaviors_close(owner);
            return false;
        }
    }
    *output = owner;
    return true;
}

bool ab_classic_behaviors_create_object(AbClassicBehaviors *owner, size_t object,
                                        AbDataValue descriptor,
                                        const AbLuaData *descriptor_data,
                                        AbError *error) {
    if (!owner || owner->constructing || object != owner->count ||
        object >= owner->capacity || object >= owner->level.bodies->count ||
        owner->level.removed_objects[object] || owner->objects[object].instance_count ||
        owner->objects[object].published)
        return ab_fail(error, "Invalid new GameObject behavior binding");
    owner->constructing = true;
    bool okay = ab_level_properties_bind_script(owner->level.properties, object, error);
    if (okay) {
        ++owner->count;
        okay = configure(owner->objects + object, descriptor, descriptor_data, error) &&
               publish(owner, object, error);
    }
    owner->constructing = false;
    return okay;
}

bool ab_classic_behaviors_recreate_object(AbClassicBehaviors *owner, size_t object,
                                          AbDataValue descriptor, AbError *error) {
    if (!owner || owner->constructing || object >= owner->count ||
        owner->level.removed_objects[object])
        return ab_fail(error, "Invalid replacement GameObject behavior binding");
    ClassicBehaviorObject *row = owner->objects + object;
    AbStringMap *groups = row->groups;
    if (!ab_string_map_reset(groups, 0, error))
        return false;
    *row = (ClassicBehaviorObject){.owner = owner, .groups = groups};
    for (size_t index = 0; index < MUSEUM_CLASSES; ++index)
        row->new_groups[index] = (AbGameBehaviorGroup){row->items + index, 1};
    AbLevelPropertyObject *properties = owner->level.properties->objects + object;
    properties->script = (AbDataTable){0};
    properties->behaviors = (AbDataTable){0};
    AbObjectPropertyAccess access = {.physics_object = table(&properties->physical)};
    owner->constructing = true;
    const AbLuaData *catalog = owner->config.catalog_data ? owner->config.catalog_data
                                                          : owner->level.catalog_data;
    bool okay =
        ab_object_property_set(&access, span("gameObjectScript"), (AbDataValue){0},
                               error) &&
        ab_level_properties_bind_script(owner->level.properties, object, error) &&
        configure(row, descriptor, catalog, error) && publish(owner, object, error);
    owner->constructing = false;
    return okay;
}

size_t ab_classic_behaviors_bytes(const AbClassicBehaviors *owner) {
    return owner ? owner->bytes : 0;
}

bool ab_classic_behaviors_add(AbClassicBehaviors *owner, size_t object, AbSpan name,
                              AbDataValue overrides, AbError *error) {
    if (!owner || owner->constructing || object >= owner->count ||
        !owner->objects[object].published || owner->level.removed_objects[object] ||
        !name.data || !name.size)
        return ab_fail(error, "Invalid classic addBehavior receiver");
    owner->constructing = true;
    owner->host.flying_bird = *owner->config.flying_bird;
    bool okay = add_behavior(owner->objects + object, name, overrides, error);
    /* The source inserts a missing empty group before construction. Retain that
     * public prefix even if initialization or the bounded append later fails. */
    bool published = publish(owner, object, error);
    owner->constructing = false;
    return okay && published;
}

const AbStringMap *ab_classic_behaviors_groups(const AbClassicBehaviors *owner,
                                               size_t object) {
    return owner && object < owner->count ? owner->objects[object].groups : NULL;
}

static bool delegate(AbClassicBehaviors *owner, size_t object, const char *event,
                     const AbUiPointerValue *arguments, size_t count, AbError *error) {
    if (!owner || object >= owner->count)
        return ab_fail(error, "Invalid classic behavior event object");
    owner->host.flying_bird = *owner->config.flying_bird;
    return ab_game_object_delegate(owner->objects[object].groups, span(event),
                                   arguments, count, error);
}

bool ab_classic_behaviors_launch(AbClassicBehaviors *owner, size_t object,
                                 AbVec2 velocity, AbError *error) {
    AbDataEntry entries[2] = {{text("x"), {.kind = AB_DATA_NUMBER}},
                              {text("y"), {.kind = AB_DATA_NUMBER}}};
    memcpy(&entries[0].value.bits, &velocity.x, sizeof(velocity.x));
    memcpy(&entries[1].value.bits, &velocity.y, sizeof(velocity.y));
    AbDataTable argument = {entries, 2, 2, NULL};
    AbUiPointerValue value = {.kind = AB_UI_POINTER_OBJECT,
                              .object = (uintptr_t)&argument};
    return delegate(owner, object, "onLaunch", &value, 1, error);
}

static AbUiPointerValue numeric(float value) {
    AbUiPointerValue result = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&result.number_bits, &value, sizeof(value));
    return result;
}

bool ab_classic_behaviors_collision(AbClassicBehaviors *owner, size_t object,
                                    size_t other, const AbBlockCollisionNotice *notice,
                                    AbError *error) {
    return ab_classic_behaviors_bird_collision(owner, object, object, other, notice,
                                               error);
}

bool ab_classic_behaviors_bird_collision(AbClassicBehaviors *owner, size_t receiver,
                                         size_t first, size_t second,
                                         const AbBlockCollisionNotice *notice,
                                         AbError *error) {
    if (!owner || receiver >= owner->count || first >= owner->level.bodies->count ||
        second >= owner->level.bodies->count || !notice)
        return ab_fail(error, "Invalid classic behavior collision binding");
    AbLevelPropertyObject *properties = owner->level.properties->objects;
    AbUiPointerValue arguments[8] = {
        {.kind = AB_UI_POINTER_OBJECT,
         .object = (uintptr_t)&properties[first].physical},
        {.kind = AB_UI_POINTER_OBJECT,
         .object = (uintptr_t)&properties[second].physical},
        numeric(notice->force),
        numeric(notice->damage),
        notice->normal_present ? numeric(notice->normal.x) : (AbUiPointerValue){0},
        notice->normal_present ? numeric(notice->normal.y) : (AbUiPointerValue){0},
        notice->point_present ? numeric(notice->point.x) : (AbUiPointerValue){0},
        notice->point_present ? numeric(notice->point.y) : (AbUiPointerValue){0}};
    return delegate(owner, receiver, "onBirdCollision", arguments, 8, error);
}

bool ab_classic_behaviors_block_collision(AbClassicBehaviors *owner, size_t object,
                                          const AbBlockCollisionNotice *notice,
                                          AbError *error) {
    if (!owner || object >= owner->count)
        return ab_fail(error, "Invalid classic block behavior object");
    owner->host.flying_bird = *owner->config.flying_bird;
    return ab_game_object_block_collision(owner->objects[object].groups, notice, error);
}

bool ab_classic_behaviors_specialty(AbClassicBehaviors *owner, size_t object,
                                    AbError *error) {
    return delegate(owner, object, "onTriggerSpecialty", NULL, 0, error);
}

bool ab_classic_behaviors_destroy(AbClassicBehaviors *owner, size_t object,
                                  AbError *error) {
    return delegate(owner, object, "onDestroy", NULL, 0, error);
}

bool ab_classic_behaviors_debug_draw(AbClassicBehaviors *owner, size_t object,
                                     AbError *error) {
    return delegate(owner, object, "debugDraw", NULL, 0, error);
}

bool ab_classic_behaviors_touch(AbClassicBehaviors *owner, size_t object,
                                AbClassicTouchKind kind, AbDataTable *touch,
                                float seconds, AbError *error) {
    static const char *const names[] = {"onTouchStart", "onTouchUpdate", "onTouchEnd",
                                        "onTouchCancel"};
    if ((unsigned)kind >= sizeof(names) / sizeof(*names) || !touch)
        return ab_fail(error, "Invalid classic behavior touch dispatch");
    AbUiPointerValue arguments[2] = {
        {.kind = AB_UI_POINTER_OBJECT, .object = (uintptr_t)touch}, numeric(seconds)};
    return delegate(owner, object, names[kind], arguments,
                    kind == AB_CLASSIC_TOUCH_UPDATE ? 2 : 1, error);
}
