#include "data_internal.h"

#include <stdlib.h>

const AbDataTableLayout *ab_lua_table_layout(const AbLuaData *data,
                                             const AbDataTable *table) {
    if (!data || !table) {
        return NULL;
    }
    for (const AbDataTableLayout *layout = data->layouts; layout;
         layout = layout->next) {
        if (layout->table == table) {
            return layout;
        }
    }
    return NULL;
}

void ab_data_layout_nil_assignment(AbLuaData *data, const AbDataTable *table) {
    for (AbDataTableLayout *layout = data->layouts; layout; layout = layout->next) {
        if (layout->table == table) {
            layout->no_nil_assignments = false;
            return;
        }
    }
}

static bool decode_hint(unsigned encoded, size_t *output) {
    if (encoded < 8) {
        *output = encoded;
        return true;
    }
    if (encoded > 255) {
        return false;
    }
    unsigned exponent = (encoded >> 3) - 1;
    uint64_t count = (uint64_t)((encoded & 7) + 8) << exponent;
    if (count > UINT32_MAX) {
        return false;
    }
    *output = (size_t)count;
    return true;
}

bool ab_data_record_layout(AbLuaData *data, const AbDataTable *table,
                           unsigned array_hint, unsigned hash_hint, AbError *error) {
    size_t array, hash;
    if (!decode_hint(array_hint, &array) || !decode_hint(hash_hint, &hash)) {
        return ab_fail(error, "Unsupported static table constructor hint");
    }
    size_t capacity = hash ? 1 : 0;
    while (capacity < hash) {
        if (capacity > UINT32_MAX / 2) {
            return ab_fail(error, "Static table constructor hash size exceeds target");
        }
        capacity *= 2;
    }
    if (sizeof(AbDataTableLayout) > data->memory_limit - data->allocated_bytes) {
        return ab_fail(error, "Static table layout exceeds memory limit");
    }
    AbDataTableLayout *layout = malloc(sizeof(*layout));
    if (!layout) {
        return ab_fail(error, "Cannot allocate static table layout");
    }
    *layout = (AbDataTableLayout){table, array, capacity, true, data->layouts};
    data->layouts = layout;
    data->allocated_bytes += sizeof(*layout);
    return true;
}
