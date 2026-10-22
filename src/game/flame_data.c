#include "ab/flame_data.h"
#include "component_data_internal.h"

#include <string.h>

static bool default_number(const AbDataValue *value, float *output) {
    *output = 1;
    return !value || value->kind == AB_DATA_NIL ||
           (value->kind == AB_DATA_BOOLEAN && !value->bits) || number(value, output);
}

static bool offsets(const AbDataTable *table, AbFlameStorage storage,
                    AbFlameDefinition *definition) {
    if (!valid_table(table)) {
        return false;
    }
    for (size_t index = 0; index < table->count; ++index) {
        const AbDataEntry *entry = table->entries + index;
        if (entry->key.kind != AB_DATA_STRING ||
            (entry->key.string.size && !entry->key.string.data)) {
            return false;
        }
        if (entry->value.kind == AB_DATA_NIL ||
            (entry->value.kind == AB_DATA_BOOLEAN && !entry->value.bits)) {
            continue;
        }
        AbVec2 point;
        if (entry->value.kind != AB_DATA_TABLE || !valid_table(entry->value.table) ||
            !number(field(entry->value.table, "x"), &point.x) ||
            !number(field(entry->value.table, "y"), &point.y)) {
            return false;
        }
        AbSpan name = entry->key.string;
        if (name.size == 7 && memcmp(name.data, "default", 7) == 0) {
            if (definition->default_present) {
                return false;
            }
            definition->default_present = true;
            definition->default_offset = point;
            continue;
        }
        if (definition->offset_count == storage.offset_capacity || !storage.offsets) {
            return false;
        }
        for (size_t previous = 0; previous < definition->offset_count; ++previous) {
            AbSpan other = storage.offsets[previous].sprite;
            if (name.size == other.size &&
                (!name.size || memcmp(name.data, other.data, name.size) == 0)) {
                return false;
            }
        }
        storage.offsets[definition->offset_count++] = (AbFlameOffset){name, point};
    }
    return true;
}

bool ab_flame_data(const AbDataTable *component, AbFlameStorage storage,
                   AbFlameConfiguration *output, AbError *error) {
    if (!valid_table(component) || !output) {
        return ab_fail(error, "Invalid flame component data");
    }
    AbFlameConfiguration configuration = {
        .definition = {.sprites = storage.sprites, .offsets = storage.offsets}};
    const AbDataValue *sprites = field(component, "sprites");
    const AbDataValue *offset = field(component, "offset");
    if (!default_number(field(component, "speed"), &configuration.timer_speed) ||
        !default_number(field(component, "scale"), &configuration.scale) || !sprites ||
        sprites->kind != AB_DATA_TABLE ||
        !sprite_array(sprites->table, storage.sprites, storage.sprite_capacity) ||
        !offset || offset->kind != AB_DATA_TABLE ||
        !offsets(offset->table, storage, &configuration.definition)) {
        return ab_fail(error, "Unsupported flame component fields or storage");
    }
    configuration.definition.count = sprites->table->count;
    *output = configuration;
    return true;
}
