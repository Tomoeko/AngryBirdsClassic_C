#include "ab/block_removal_spawn.h"
#include "ab/script_math.h"
#include "ab/ui_value_text.h"
#include "script_float_internal.h"

#include <math.h>
#include <string.h>

typedef struct {
    float columns, cell_width, cell_height, start_x, start_y;
    uint32_t amount;
} SpawnGrid;

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static bool field(AbDataValue table, AbDataValue key, AbDataValue *output,
                  AbError *error) {
    if (table.kind != AB_DATA_TABLE || !table.table ||
        table.table->count > table.table->capacity ||
        (table.table->count && !table.table->entries)) {
        return ab_fail(error, "Removal spawn indexes a non-table value");
    }
    const AbDataValue *found = ab_data_get(table.table, key);
    *output = found ? *found : (AbDataValue){0};
    return true;
}

static bool number(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER) {
        return ab_fail(error, "Removal spawn requires a numeric value");
    }
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite removal spawn source number");
}

static bool numeric_field(AbDataValue table, const char *name, float *output,
                          AbError *error) {
    AbDataValue value = {0};
    return field(table, text(name), &value, error) && number(value, output, error);
}

static float add(float first, float second) {
    return script_float_binary(first, second, AB_SCRIPT_FLOAT_ADD);
}

static float multiply(float first, float second) {
    return script_float_binary(first, second, AB_SCRIPT_FLOAT_MULTIPLY);
}

static float divide(float first, float second) {
    return script_float_binary(first, second, AB_SCRIPT_FLOAT_DIVIDE);
}

static float negative(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return script_float_word(word ^ UINT32_C(0x80000000));
}

static bool loop_amount(AbDataValue table, uint32_t *output, AbError *error) {
    float amount = 0;
    if (!numeric_field(table, "amount", &amount, error)) {
        return false;
    }
    if (amount > 16777215.0f) {
        return ab_fail(error, "Removal spawn loop exceeds exact integer range");
    }
    *output = amount < 1 ? 0u : (uint32_t)amount;
    return true;
}

static bool target_field(AbDataValue spawn, AbBlockRemovalSpawnHost *host,
                         const char *name, AbDataValue *output, AbError *error) {
    AbDataValue blocks = {0}, definition = {0}, target = {0};
    return field(host->block_table, text("blocks"), &blocks, error) &&
           field(spawn, text("definition"), &definition, error) &&
           field(blocks, definition, &target, error) &&
           field(target, text(name), output, error);
}

static bool grid_layout(AbDataValue object, AbDataValue spawn,
                        AbBlockRemovalSpawnHost *host, SpawnGrid *output,
                        AbError *error) {
    AbDataValue target_width = {0}, unused_height = {0};
    if (!target_field(spawn, host, "width", &target_width, error) ||
        !target_field(spawn, host, "height", &unused_height, error)) {
        return false;
    }
    float width = 0, height = 0, target = 0, amount = 0;
    if (!numeric_field(object, "width", &width, error) ||
        !number(target_width, &target, error) ||
        !numeric_field(spawn, "amount", &amount, error)) {
        return false;
    }
    float columns = (float)floor((double)divide(width, target));
    columns = columns > 1 ? columns : 1;
    float rows = (float)ceil((double)divide(amount, columns));
    rows = rows > 1 ? rows : 1;
    if (!numeric_field(object, "width", &width, error) ||
        !numeric_field(object, "height", &height, error)) {
        return false;
    }
    SpawnGrid grid = {.columns = columns,
                      .cell_width = divide(width, columns),
                      .cell_height = divide(height, rows)};
    grid.start_x = add(negative(multiply(divide(columns, 2), grid.cell_width)),
                       multiply(grid.cell_width, 0.5f));
    grid.start_y = add(negative(multiply(divide(rows, 2), grid.cell_height)),
                       multiply(grid.cell_height, 0.5f));
    if (!loop_amount(spawn, &grid.amount, error)) {
        return false;
    }
    *output = grid;
    return true;
}

static bool generated_name(AbDataValue base, const char *separator, uint32_t index,
                           AbBlockRemovalSpawnHost *host, AbSpan *output,
                           AbError *error) {
    uint8_t base_storage[32], index_storage[32];
    AbSpan first = base.string, last = {0};
    if (base.kind == AB_DATA_NUMBER) {
        if (!ab_ui_value_text((AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                                 .number_bits = base.bits},
                              base_storage, sizeof(base_storage), &first)) {
            return ab_fail(error, "Cannot format removal spawn base name");
        }
    } else if (base.kind != AB_DATA_STRING || (first.size && !first.data)) {
        return ab_fail(error, "Removal spawn name concatenates a non-string value");
    }
    float numeric_index = (float)index;
    uint32_t word;
    memcpy(&word, &numeric_index, sizeof(word));
    if (!ab_ui_value_text(
            (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER, .number_bits = word},
            index_storage, sizeof(index_storage), &last)) {
        return ab_fail(error, "Cannot format removal spawn index");
    }
    size_t middle = strlen(separator);
    if (!host->name_storage || first.size > host->name_capacity ||
        middle > host->name_capacity - first.size ||
        last.size > host->name_capacity - first.size - middle) {
        return ab_fail(error, "Removal spawn name exceeds caller storage");
    }
    if (first.size) {
        memcpy(host->name_storage, first.data, first.size);
    }
    memcpy(host->name_storage + first.size, separator, middle);
    memcpy(host->name_storage + first.size + middle, last.data, last.size);
    *output = (AbSpan){host->name_storage, first.size + middle + last.size};
    return true;
}

static bool normalized_angle(AbDataValue object, AbBlockRemovalSpawnHost *host,
                             float *output, AbError *error) {
    float angle = 0;
    if (!numeric_field(object, "angle", &angle, error)) {
        return false;
    }
    if (!isfinite(host->math_pi) || host->math_pi <= 0 || host->math_pi > 512) {
        return ab_fail(error, "Unsupported removal spawn pi");
    }
    float tau = multiply(host->math_pi, 2);
    angle = (float)fmod((double)angle, (double)tau);
    if (angle < 0) {
        angle = add(angle, multiply(host->math_pi, 2));
    }
    *output = angle;
    return true;
}

static bool grid_spawns(AbDataValue object, AbDataValue spawn,
                        AbBlockRemovalSpawnHost *host, size_t *created_count,
                        AbError *error) {
    SpawnGrid grid = {0};
    if (!grid_layout(object, spawn, host, &grid, error)) {
        return false;
    }
    float column = 0, row = 0;
    for (uint32_t index = 1; index <= grid.amount; ++index) {
        if (*created_count >= host->maximum_instances) {
            return ab_fail(error, "Removal spawn instance limit reached");
        }
        float angle = 0, cosine = 0, sine = 0;
        if (!normalized_angle(object, host, &angle, error) ||
            !ab_script_cosine(angle, &cosine) || !ab_script_sine(angle, &sine)) {
            return ab_fail(error, "Unsupported removal spawn rotation");
        }
        float local_x = add(grid.start_x, multiply(column, grid.cell_width));
        float local_y = add(grid.start_y, multiply(row, grid.cell_height));
        float x =
            script_float_binary(multiply(cosine, local_x), multiply(sine, local_y),
                                AB_SCRIPT_FLOAT_SUBTRACT);
        float y = add(multiply(sine, local_x), multiply(cosine, local_y));
        AbDataValue definition = {0}, name = {0};
        AbSpan generated = {0};
        float object_x = 0, object_y = 0;
        if (!field(spawn, text("definition"), &definition, error) ||
            !field(object, text("name"), &name, error) ||
            !generated_name(name, "_", index, host, &generated, error) ||
            !numeric_field(object, "x", &object_x, error) ||
            !numeric_field(object, "y", &object_y, error)) {
            return false;
        }
        if (!host->create) {
            return ab_fail(error, "Missing removal spawn createObject host");
        }
        AbDataValue created = {0};
        if (!host->create(host->context, host->block_table, definition, generated,
                          add(object_x, x), add(object_y, y), &created, error)) {
            return false;
        }
        ++*created_count;
        column = add(column, 1);
        if (grid.columns <= column) {
            row = add(row, 1);
            column = 0;
        }
        if (!host->rotate) {
            return ab_fail(error, "Missing removal spawn setRotation host");
        }
        if (!host->rotate(host->context, created, angle, error)) {
            return false;
        }
    }
    return true;
}

static bool dense_length(AbDataValue types, size_t *output, AbError *error) {
    if (types.kind == AB_DATA_STRING) {
        *output = types.string.size;
        return true;
    }
    AbDataValue ignored = {0};
    if (!field(types, (AbDataValue){0}, &ignored, error)) {
        return false;
    }
    size_t length = 0;
    for (size_t index = 0; index < types.table->count; ++index) {
        AbDataEntry entry = types.table->entries[index];
        if (entry.key.kind != AB_DATA_NUMBER || entry.value.kind == AB_DATA_NIL) {
            continue;
        }
        float key;
        memcpy(&key, &entry.key.bits, sizeof(key));
        if (key >= 1 && key <= 16777215 && key == floorf(key) && (size_t)key > length) {
            length = (size_t)key;
        }
    }
    for (size_t index = 1; index <= length; ++index) {
        float key = (float)index;
        uint32_t word;
        memcpy(&word, &key, sizeof(word));
        AbDataValue value = {0};
        if (!field(types, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = word}, &value,
                   error) ||
            value.kind == AB_DATA_NIL) {
            return ab_fail(error, "Sparse removal spawn type array is unsupported");
        }
    }
    *output = length;
    return true;
}

static bool death_spawns(const AbBlockRemovalEntry *entry,
                         const AbBlockRemovalDetached *detached,
                         AbBlockRemovalSpawnHost *host, size_t *created_count,
                         AbError *error) {
    uint32_t amount = 0;
    if (!loop_amount(detached->death_spawn, &amount, error)) {
        return false;
    }
    for (uint32_t index = 1; index <= amount; ++index) {
        if (*created_count >= host->maximum_instances) {
            return ab_fail(error, "Removal death-spawn instance limit reached");
        }
        AbDataValue types = {0}, current_types = {0}, choice = {0}, definition = {0};
        size_t count = 0;
        if (!field(detached->death_spawn, text("types"), &types, error) ||
            !field(detached->death_spawn, text("types"), &current_types, error) ||
            !dense_length(current_types, &count, error)) {
            return false;
        }
        if (count > 16777215 || !host->random) {
            return ab_fail(error, "Missing or oversized death-spawn random choice");
        }
        if (!host->random(host->context, 2, 1, (float)count, &choice, error) ||
            !field(types, choice, &definition, error)) {
            return false;
        }
        AbSpan generated = {0};
        float x = 0, y = 0, scale = 0;
        if (!generated_name((AbDataValue){.kind = AB_DATA_STRING, .string = entry->key},
                            "_generated_", index, host, &generated, error) ||
            !number(detached->x, &x, error) ||
            !number(host->scale_factor, &scale, error)) {
            return false;
        }
        x = multiply(x, scale);
        if (!number(detached->y, &y, error) ||
            !number(host->scale_factor, &scale, error)) {
            return false;
        }
        y = multiply(y, scale);
        if (!host->create) {
            return ab_fail(error, "Missing removal death-spawn createObject host");
        }
        AbDataValue created = {0};
        if (!host->create(host->context, host->block_table, definition, generated, x, y,
                          &created, error)) {
            return false;
        }
        ++*created_count;
        bool (*rotate)(void *, AbDataValue, float, AbError *) = host->rotate;
        if (!host->random) {
            return ab_fail(error, "Missing removal death-spawn random rotation");
        }
        AbDataValue random = {0};
        float angle = 0;
        if (!host->random(host->context, 0, 0, 0, &random, error) ||
            !number(random, &angle, error)) {
            return false;
        }
        if (!isfinite(host->math_pi) || host->math_pi <= 0 || host->math_pi > 512) {
            return ab_fail(error, "Unsupported removal death-spawn pi");
        }
        angle = multiply(multiply(angle, 2), host->math_pi);
        if (!rotate) {
            return ab_fail(error, "Missing captured death-spawn setRotation host");
        }
        if (!rotate(host->context, created, angle, error)) {
            return false;
        }
    }
    return true;
}

bool ab_block_removal_spawn(const AbBlockRemovalEntry *entry,
                            const AbBlockRemovalDetached *detached,
                            AbBlockRemovalSpawnHost *host, AbError *error) {
    if (!entry || !detached || !host || (entry->key.size && !entry->key.data)) {
        return ab_fail(error, "Invalid removal spawn input");
    }
    const AbBlockRemovalEntry captured = *entry;
    const AbBlockRemovalDetached cached = *detached;
    size_t created = 0;
    if (cached.spawn.kind != AB_DATA_NIL &&
        !grid_spawns(captured.object, cached.spawn, host, &created, error)) {
        return false;
    }
    bool death =
        cached.death_spawn.kind != AB_DATA_NIL &&
        (cached.death_spawn.kind != AB_DATA_BOOLEAN || cached.death_spawn.bits);
    return !death || death_spawns(&captured, &cached, host, &created, error);
}
