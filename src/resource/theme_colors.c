#include "ab/theme_colors.h"

#include <math.h>
#include <string.h>

static bool table_valid(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static const AbDataValue *property(const AbDataTable *table, const char *name) {
    return ab_data_get(table,
                       (AbDataValue){.kind = AB_DATA_STRING,
                                     .string = {(const uint8_t *)name, strlen(name)}});
}

static bool channels(const AbDataValue *value, float output[3]) {
    if (!value || value->kind != AB_DATA_TABLE || !table_valid(value->table)) {
        return false;
    }
    const char *const names[] = {"r", "g", "b"};
    for (size_t index = 0; index < 3; ++index) {
        const AbDataValue *channel = property(value->table, names[index]);
        if (!channel || channel->kind != AB_DATA_NUMBER) {
            return false;
        }
        memcpy(output + index, &channel->bits, sizeof(*output));
        if (!isfinite(output[index])) {
            return false;
        }
    }
    return true;
}

bool ab_theme_colors(const AbDataTable *theme, AbThemeColors *output, AbError *error) {
    if (!output || !table_valid(theme)) {
        return ab_fail(error, "Invalid static theme color table");
    }
    AbThemeColors colors;
    if (!channels(property(theme, "color"), colors.background) ||
        !channels(property(theme, "groundColor"), colors.ground)) {
        return ab_fail(error, "Unsupported mandatory theme RGB channels");
    }
    const uint32_t reciprocal_bits = UINT32_C(0x3b808081);
    float reciprocal;
    memcpy(&reciprocal, &reciprocal_bits, sizeof(reciprocal));
    for (size_t index = 0; index < 3; ++index) {
        colors.ground[index] *= reciprocal;
    }
    const AbDataValue *tint = property(theme, "backgroundTintColor");
    if (tint && (unsigned)tint->kind > AB_DATA_TABLE) {
        return ab_fail(error, "Invalid theme background tint kind");
    }
    if (tint && tint->kind == AB_DATA_TABLE) {
        if (!channels(tint, colors.tint)) {
            return ab_fail(error, "Unsupported theme background tint channels");
        }
    } else {
        for (size_t index = 0; index < 3; ++index) {
            colors.tint[index] = 255;
        }
    }
    *output = colors;
    return true;
}
