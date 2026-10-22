#include "ab/object_property_access.h"

#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool table_valid(AbDataValue value) {
    return value.kind == AB_DATA_TABLE && value.table &&
           value.table->count <= value.table->capacity &&
           (!value.table->count || value.table->entries);
}

static bool field(AbDataValue table, AbDataValue key, AbDataValue *output,
                  AbError *error) {
    if (!table_valid(table)) {
        return ab_fail(error, "Object property access indexes a non-table value");
    }
    const AbDataValue *value = ab_data_get(table.table, key);
    *output = value ? *value : (AbDataValue){0};
    return true;
}

static bool definition(AbObjectPropertyAccess *object,
                       const AbObjectPropertyAccessHost *host, AbDataValue *output,
                       AbError *error) {
    if (!host || !host->definition) {
        return ab_fail(error, "Missing object property definition host");
    }
    *output = (AbDataValue){0};
    return host->definition(host->context, object->name, output, error);
}

bool ab_object_property_get(AbObjectPropertyAccess *object, AbSpan key,
                            const AbObjectPropertyAccessHost *host, AbDataValue *output,
                            AbError *error) {
    if (!output || (key.size && !key.data)) {
        return ab_fail(error, "Invalid object property getter arguments");
    }
    AbDataValue result = {0};
    if (!object || !truthy(object->physics_object)) {
        *output = result;
        return true;
    }
    AbDataValue current = {0};
    if (!definition(object, host, &current, error)) {
        return false;
    }
    if (!truthy(current)) {
        *output = result;
        return true;
    }
    AbDataValue property = {.kind = AB_DATA_STRING, .string = key};
    if (!field(object->physics_object, property, &result, error)) {
        return false;
    }
    if (truthy(result)) {
        *output = result;
        return true;
    }
    if (!definition(object, host, &current, error)) {
        return false;
    }
    if (truthy(current)) {
        if (!field(current, property, &result, error)) {
            return false;
        }
        if (truthy(result)) {
            *output = result;
            return true;
        }
    }
    AbDataValue materials = host->materials;
    if (!definition(object, host, &current, error)) {
        return false;
    }
    AbDataValue material_name = {0}, material = {0};
    AbDataValue name_key = {.kind = AB_DATA_STRING,
                            .string = {(const uint8_t *)"materialName", 12}};
    if (!field(current, name_key, &material_name, error) ||
        !field(materials, material_name, &material, error)) {
        return false;
    }
    if (truthy(material)) {
        if (!field(material, property, &result, error)) {
            return false;
        }
        if (truthy(result)) {
            *output = result;
            return true;
        }
    }
    *output = (AbDataValue){0};
    return true;
}

bool ab_object_property_set(AbObjectPropertyAccess *object, AbSpan key,
                            AbDataValue value, AbError *error) {
    if (!object || (key.size && !key.data) || !table_valid(object->physics_object)) {
        return ab_fail(error, "Invalid object property setter arguments");
    }
    AbDataTable *table = object->physics_object.table;
    for (size_t index = 0; index < table->count; ++index) {
        AbDataValue existing = table->entries[index].key;
        if (existing.kind == AB_DATA_STRING && existing.string.size == key.size &&
            (!key.size || memcmp(existing.string.data, key.data, key.size) == 0)) {
            table->entries[index].value = value;
            return true;
        }
    }
    if (table->count == table->capacity || !table->entries) {
        return ab_fail(error, "Object property setter exceeds physical entry capacity");
    }
    table->entries[table->count++] =
        (AbDataEntry){{.kind = AB_DATA_STRING, .string = key}, value};
    return true;
}
