#ifndef AB_COMPONENT_DATA_INTERNAL_H
#define AB_COMPONENT_DATA_INTERNAL_H

#include "ab/lua_data.h"

#include <math.h>
#include <string.h>

static inline bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static inline const AbDataValue *field(const AbDataTable *table, const char *name) {
    return ab_data_get(table,
                       (AbDataValue){.kind = AB_DATA_STRING,
                                     .string = {(const uint8_t *)name, strlen(name)}});
}

static inline bool number(const AbDataValue *value, float *output) {
    if (!value || value->kind != AB_DATA_NUMBER) {
        return false;
    }
    memcpy(output, &value->bits, sizeof(*output));
    return isfinite(*output);
}

static inline bool sprite_array(const AbDataTable *table, AbSpan *sprites,
                                size_t capacity) {
    if (!valid_table(table) || table->count > capacity ||
        table->count > UINT32_C(16777216) || (table->count && !sprites)) {
        return false;
    }
    for (size_t index = 0; index < table->count; ++index) {
        sprites[index] = (AbSpan){.size = SIZE_MAX};
    }
    for (size_t index = 0; index < table->count; ++index) {
        const AbDataEntry *entry = table->entries + index;
        float key;
        if (!number(&entry->key, &key) || key < 1 || key > (float)table->count ||
            key != (float)(size_t)key || entry->value.kind != AB_DATA_STRING ||
            entry->value.string.size == SIZE_MAX ||
            (entry->value.string.size && !entry->value.string.data)) {
            return false;
        }
        size_t slot = (size_t)key - 1;
        if (sprites[slot].size != SIZE_MAX) {
            return false;
        }
        sprites[slot] = entry->value.string;
    }
    return true;
}

#endif
