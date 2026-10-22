#include "ab/object_definition_lookup.h"

#include <string.h>

static bool valid_key(AbDataValue key) {
    return (unsigned)key.kind <= AB_DATA_TABLE &&
           (key.kind != AB_DATA_STRING || !key.string.size || key.string.data) &&
           (key.kind != AB_DATA_TABLE || key.table);
}

static bool equal_key(AbDataValue first, AbDataValue second) {
    if (first.kind != second.kind)
        return false;
    switch (first.kind) {
        case AB_DATA_NIL:
            return false;
        case AB_DATA_BOOLEAN:
            return (first.bits != 0) == (second.bits != 0);
        case AB_DATA_NUMBER: {
            float a, b;
            memcpy(&a, &first.bits, sizeof(a));
            memcpy(&b, &second.bits, sizeof(b));
            return a == b;
        }
        case AB_DATA_STRING:
            return first.string.size == second.string.size &&
                   (!first.string.size ||
                    !memcmp(first.string.data, second.string.data, first.string.size));
        case AB_DATA_TABLE:
            return first.table == second.table;
        case AB_DATA_OBJECT:
            return first.object == second.object;
    }
    return false;
}

static bool index_value(AbDataValue source, AbDataValue key, AbDataValue *output,
                        AbError *error) {
    if (source.kind != AB_DATA_TABLE || !source.table ||
        source.table->count > source.table->capacity ||
        (source.table->count && !source.table->entries) || !valid_key(key)) {
        return ab_fail(error, "Object definition lookup indexes invalid table data");
    }
    for (size_t index = 0; index < source.table->count; ++index) {
        const AbDataEntry *entry = source.table->entries + index;
        if (!valid_key(entry->key)) {
            return ab_fail(error, "Object definition lookup has an invalid stored key");
        }
        if (equal_key(entry->key, key)) {
            *output = entry->value;
            return true;
        }
    }
    *output = (AbDataValue){0};
    return true;
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static bool world_object(AbObjectDefinitionLookup *lookup, AbDataValue name,
                         AbDataValue *output, AbError *error) {
    AbDataValue world = {0};
    return index_value(lookup->objects, text("world"), &world, error) &&
           index_value(world, name, output, error);
}

bool ab_object_world_lookup(AbObjectDefinitionLookup *lookup, AbDataValue key,
                            AbDataValue *output, AbError *error) {
    if (!lookup || !output)
        return ab_fail(error, "Invalid world object lookup request");
    return world_object(lookup, key, output, error);
}

bool ab_object_definition_lookup(AbObjectDefinitionLookup *lookup, AbSpan name,
                                 AbDataValue *output, AbError *error) {
    if (!lookup || !output || (name.size && !name.data)) {
        return ab_fail(error, "Invalid object definition lookup request");
    }
    AbDataValue identifier = {.kind = AB_DATA_STRING, .string = name};
    AbDataValue object = {0};
    if (!world_object(lookup, identifier, &object, error)) {
        return false;
    }
    if (object.kind == AB_DATA_NIL) {
        if (!lookup->missing) {
            return ab_fail(error, "Missing object-lookup diagnostic service");
        }
        if (!lookup->missing(lookup->context, name, error)) {
            return false;
        }
        *output = (AbDataValue){0};
        return true;
    }
    AbDataValue blocks = {0}, definition = {0}, result = {0};
    if (!index_value(lookup->block_table, text("blocks"), &blocks, error) ||
        !world_object(lookup, identifier, &object, error) ||
        !index_value(object, text("definition"), &definition, error) ||
        !index_value(blocks, definition, &result, error)) {
        return false;
    }
    *output = result;
    return true;
}
