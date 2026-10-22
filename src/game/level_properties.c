#include "ab/level_properties.h"

#include "ab/game_collision.h"

#include <string.h>

static AbDataValue text(AbSpan value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = value};
}

static AbDataValue key(const char *value) {
    return text((AbSpan){(const uint8_t *)value, strlen(value)});
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool valid_table(const AbDataTable *value) {
    return value && value->count <= value->capacity &&
           (!value->count || value->entries);
}

static AbDataValue field(const AbDataTable *source, const char *name) {
    const AbDataValue *value = ab_data_get(source, key(name));
    return value ? *value : (AbDataValue){0};
}

static bool valid_object(const AbLevelProperties *properties, size_t object) {
    return properties && properties->count <= properties->capacity &&
           object < properties->count && properties->objects &&
           properties->world_entries;
}

static void initialize_object(AbLevelPropertyObject *object,
                              const AbLevelObject *source) {
    *object = (AbLevelPropertyObject){0};
    object->physical = (AbDataTable){object->physical_entries, 6,
                                     AB_LEVEL_PHYSICAL_PROPERTY_CAPACITY, NULL};
    object->script = (AbDataTable){object->script_entries, 3, 3, NULL};
    object->launch_velocity = (AbDataTable){object->launch_entries, 2, 2, NULL};
    object->launch_entries[0] = (AbDataEntry){key("x"), number(0)};
    object->launch_entries[1] = (AbDataEntry){key("y"), number(0)};
    object->physical_entries[0] = (AbDataEntry){key("name"), text(source->name)};
    object->physical_entries[1] =
        (AbDataEntry){key("definition"), text(source->definition_name)};
    object->physical_entries[2] =
        (AbDataEntry){key("width"), number(source->physics.dimensions.width)};
    object->physical_entries[3] =
        (AbDataEntry){key("height"), number(source->physics.dimensions.height)};
    object->physical_entries[4] =
        (AbDataEntry){key("radius"), number(source->physics.dimensions.radius)};
    object->physical_entries[5] =
        (AbDataEntry){key("gameObjectScript"), table(&object->script)};
    object->script_entries[0] = (AbDataEntry){key("name"), text(source->name)};
    object->script_entries[1] =
        (AbDataEntry){key("physicsObject"), table(&object->physical)};
    object->script_entries[2] =
        (AbDataEntry){key("behaviors"), table(&object->behaviors)};
}

bool ab_level_properties_initialize(AbLevelProperties *properties,
                                    const AbLevelBodies *bodies,
                                    const AbDataTable *catalog, AbError *error) {
    if (!properties || !bodies || bodies->count > bodies->capacity ||
        bodies->count > properties->capacity ||
        (bodies->count &&
         (!bodies->objects || !properties->objects || !properties->world_entries)) ||
        !valid_table(catalog)) {
        return ab_fail(error, "Invalid level property storage or source");
    }
    properties->count = 0;
    for (size_t index = 0; index < bodies->count; ++index) {
        const AbLevelObject *source = bodies->objects + index;
        if (!source->name.data || !source->name.size || !source->definition_name.data ||
            !source->definition_name.size) {
            return ab_fail(error, "Invalid initial level property name");
        }
        initialize_object(properties->objects + index, source);
        properties->world_entries[index] = (AbDataEntry){
            text(source->name), table(&properties->objects[index].physical)};
        ++properties->count;
    }
    properties->world = (AbDataTable){properties->world_entries, properties->count,
                                      properties->capacity, NULL};
    properties->dynamic = (AbDataTable){0};
    properties->registry_entries[0] =
        (AbDataEntry){key("world"), table(&properties->world)};
    properties->registry_entries[1] = (AbDataEntry){0};
    properties->registry = (AbDataTable){properties->registry_entries, 1, 2, NULL};
    properties->lookup.objects = table(&properties->registry);
    properties->lookup.block_table = table((AbDataTable *)catalog);
    return true;
}

bool ab_level_properties_bind_dynamic(AbLevelProperties *properties,
                                      const AbLevelBodies *bodies, AbDataEntry *entries,
                                      size_t capacity, AbError *error) {
    if (!properties || !bodies || bodies->count != properties->count ||
        bodies->count > bodies->capacity || bodies->count > capacity ||
        (bodies->count && (!entries || !bodies->objects || !properties->objects)) ||
        properties->registry.entries != properties->registry_entries ||
        properties->registry.count != 1 || properties->registry.capacity != 2)
        return ab_fail(error, "Invalid initial dynamic property-table binding");
    size_t count = 0;
    for (size_t index = 0; index < bodies->count; ++index) {
        const AbLevelObject *object = bodies->objects + index;
        if (object->name.size == 6 && object->name.data &&
            !memcmp(object->name.data, "ground", 6))
            continue;
        float collider = 0;
        if (object->physics.collider.kind == AB_DATA_NUMBER)
            memcpy(&collider, &object->physics.collider.bits, sizeof(collider));
        if (object->physics.collider.kind == AB_DATA_NUMBER &&
            collider == (float)AB_COLLIDER_STATIC)
            continue;
        entries[count++] = (AbDataEntry){text(object->name),
                                         table(&properties->objects[index].physical)};
    }
    properties->dynamic = (AbDataTable){entries, count, capacity, NULL};
    properties->registry_entries[1] =
        (AbDataEntry){key("dynamic"), table(&properties->dynamic)};
    properties->registry.count = 2;
    return true;
}

bool ab_level_properties_bind_script(AbLevelProperties *properties, size_t index,
                                     AbError *error) {
    if (!valid_object(properties, index))
        return ab_fail(error, "Invalid dynamic GameObject property binding");
    AbLevelPropertyObject *object = properties->objects + index;
    if (!valid_table(&object->physical) || object->script.entries ||
        object->script.count || object->script.capacity || object->behaviors.entries ||
        object->behaviors.count || object->behaviors.capacity ||
        field(&object->physical, "gameObjectScript").kind != AB_DATA_NIL)
        return ab_fail(error, "Dynamic GameObject already has a script binding");
    AbDataValue name = field(&object->physical, "name");
    if (name.kind == AB_DATA_NIL || (name.kind == AB_DATA_BOOLEAN && !name.bits))
        name = key("GameObject_Unnamed");
    object->script_entries[0] = (AbDataEntry){key("name"), name};
    object->script_entries[1] =
        (AbDataEntry){key("behaviors"), table(&object->behaviors)};
    object->script_entries[2] =
        (AbDataEntry){key("physicsObject"), table(&object->physical)};
    object->script = (AbDataTable){object->script_entries, 3, 3, NULL};
    object->launch_entries[0] = (AbDataEntry){key("x"), number(0)};
    object->launch_entries[1] = (AbDataEntry){key("y"), number(0)};
    object->launch_velocity = (AbDataTable){object->launch_entries, 2, 2, NULL};
    AbObjectPropertyAccess access = {.physics_object = table(&object->physical)};
    return ab_object_property_set(&access, key("gameObjectScript").string,
                                  table(&object->script), error);
}

static bool get_definition(void *context, AbDataValue name, AbDataValue *output,
                           AbError *error) {
    if (name.kind != AB_DATA_STRING) {
        return ab_fail(error, "Unsupported non-string level script name");
    }
    AbLevelProperties *properties = context;
    return ab_object_definition_lookup(&properties->lookup, name.string, output, error);
}

static bool object_access(AbLevelProperties *properties, size_t index,
                          AbObjectPropertyAccess *output, AbError *error) {
    if (!valid_object(properties, index) ||
        !valid_table(&properties->objects[index].script)) {
        return ab_fail(error, "Invalid level script property binding");
    }
    const AbDataTable *script = &properties->objects[index].script;
    *output =
        (AbObjectPropertyAccess){field(script, "name"), field(script, "physicsObject")};
    return true;
}

bool ab_level_property_get(AbLevelProperties *properties, size_t object,
                           AbSpan key_name, AbDataValue *output, AbError *error) {
    AbObjectPropertyAccess access;
    if (!object_access(properties, object, &access, error)) {
        return false;
    }
    /* ObjectFunctions reads materials only after two successful definition
     * queries. Those plain-table queries have no callback on their success
     * path, so retaining the field now observes the same binding. */
    AbDataValue catalog = properties->lookup.block_table;
    AbDataValue materials = catalog.kind == AB_DATA_TABLE && valid_table(catalog.table)
                                ? field(catalog.table, "materials")
                                : (AbDataValue){0};
    AbObjectPropertyAccessHost host = {properties, get_definition, materials};
    return ab_object_property_get(&access, key_name, &host, output, error);
}

bool ab_level_property_set(AbLevelProperties *properties, size_t object,
                           AbSpan key_name, AbDataValue value, AbError *error) {
    AbObjectPropertyAccess access;
    return object_access(properties, object, &access, error) &&
           ab_object_property_set(&access, key_name, value, error);
}

bool ab_level_properties_publish_position(AbLevelProperties *properties, size_t index,
                                          AbVec2 position, AbError *error) {
    if (!valid_object(properties, index))
        return ab_fail(error, "Invalid level position property binding");
    AbObjectPropertyAccess access = {.physics_object =
                                         table(&properties->objects[index].physical)};
    return ab_object_property_set(&access, key("x").string, number(position.x),
                                  error) &&
           ab_object_property_set(&access, key("y").string, number(position.y), error);
}

bool ab_level_properties_publish_angle(AbLevelProperties *properties, size_t index,
                                       float angle, AbError *error) {
    if (!valid_object(properties, index))
        return ab_fail(error, "Invalid level angle property binding");
    AbObjectPropertyAccess access = {.physics_object =
                                         table(&properties->objects[index].physical)};
    return ab_object_property_set(&access, key("angle").string, number(angle), error);
}

bool ab_level_properties_publish_motion(AbLevelProperties *properties, size_t index,
                                        const AbObjectProperties *motion,
                                        AbError *error) {
    if (!motion || !valid_object(properties, index))
        return ab_fail(error, "Invalid level motion property binding");
    AbObjectPropertyAccess access = {.physics_object =
                                         table(&properties->objects[index].physical)};
    const AbDataEntry fields[] = {
        {key("x"), number(motion->pose.position.x)},
        {key("y"), number(motion->pose.position.y)},
        {key("xVel"), number(motion->velocity.x)},
        {key("yVel"), number(motion->velocity.y)},
        {key("angle"), number(motion->pose.angle)},
        {key("sleeping"), {.kind = AB_DATA_BOOLEAN, .bits = motion->sleeping}},
        {key("outsideBoundaries"),
         {.kind = AB_DATA_BOOLEAN, .bits = motion->outside_boundaries}}};
    for (size_t field = 0; field < sizeof(fields) / sizeof(*fields); ++field) {
        if (!ab_object_property_set(&access, fields[field].key.string,
                                    fields[field].value, error))
            return false;
    }
    return true;
}

bool ab_level_properties_publish_launch(AbLevelProperties *properties, size_t index,
                                        AbVec2 velocity, AbError *error) {
    if (!valid_object(properties, index)) {
        return ab_fail(error, "Invalid level launch property binding");
    }
    AbLevelPropertyObject *object = properties->objects + index;
    if (!valid_table(&object->physical) || object->launch_published ||
        field(&object->physical, "launchVelocity").kind != AB_DATA_NIL) {
        return ab_fail(error, "Level launch vector already exists or is invalid");
    }
    AbObjectPropertyAccess access = {.physics_object = table(&object->physical)};
    if (!ab_object_property_set(&access, key("launchVelocity").string,
                                table(&object->launch_velocity), error)) {
        return false;
    }
    object->launch_entries[0].value = number(velocity.x);
    object->launch_entries[1].value = number(velocity.y);
    object->launch_published = true;
    return true;
}

bool ab_level_properties_forget(AbLevelProperties *properties, size_t object,
                                AbError *error) {
    if (!valid_object(properties, object)) {
        return ab_fail(error, "Invalid level script removal binding");
    }
    properties->world_entries[object].value = (AbDataValue){0};
    return true;
}
