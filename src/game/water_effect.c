#include "ab/water_effect.h"

#include <math.h>
#include <string.h>

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    AbDataValue key = {.kind = AB_DATA_STRING,
                       .string = {(const uint8_t *)name, strlen(name)}};
    const AbDataValue *value = ab_data_get(table, key);
    return value ? *value : (AbDataValue){0};
}

static bool number(AbDataValue value, float *output) {
    if (value.kind == AB_DATA_STRING || (unsigned)value.kind > AB_DATA_TABLE) {
        return false;
    }
    *output = 0;
    if (value.kind == AB_DATA_NUMBER) {
        memcpy(output, &value.bits, sizeof(*output));
        return isfinite(*output);
    }
    return true;
}

bool ab_water_effect(const AbDataTable *level, AbWaterEffect *output, AbError *error) {
    if (!valid_table(level) || !output) {
        return ab_fail(error, "Invalid initial water effect request");
    }
    AbWaterEffect effect = {0};
    AbDataValue legacy = field(level, "waterLevel");
    if (legacy.kind == AB_DATA_NUMBER) {
        effect.enabled = true;
        if (!number(legacy, &effect.level)) {
            return ab_fail(error, "Invalid legacy water level");
        }
    } else {
        if (legacy.kind == AB_DATA_STRING || (unsigned)legacy.kind > AB_DATA_TABLE) {
            return ab_fail(error, "Unsupported legacy water-level coercion");
        }
        AbDataValue water = field(level, "water");
        if (water.kind == AB_DATA_TABLE) {
            if (!valid_table(water.table)) {
                return ab_fail(error, "Invalid water table");
            }
            effect.enabled = true;
            AbDataValue freezing = field(water.table, "freezing");
            AbDataValue kill = field(water.table, "killLevel");
            effect.freezing = freezing.kind == AB_DATA_BOOLEAN && freezing.bits != 0;
            effect.kill_level_present = kill.kind == AB_DATA_NUMBER;
            if (!number(field(water.table, "waterLevel"), &effect.level) ||
                !number(kill, &effect.kill_level)) {
                return ab_fail(error, "Unsupported water-table numeric fields");
            }
        }
    }
    *output = effect;
    return true;
}
