#include "ab/theme_layer_data.h"

#include <math.h>
#include <string.h>

static const AbDataValue *tuple(const AbDataTable *table, float index) {
    uint32_t bits;
    memcpy(&bits, &index, sizeof(bits));
    return ab_data_get(table, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits});
}

static bool numeric_default(const AbDataValue *value, float fallback, float *output) {
    if (!value || value->kind == AB_DATA_NIL || value->kind == AB_DATA_BOOLEAN ||
        value->kind == AB_DATA_TABLE) {
        *output = fallback;
        return true;
    }
    if (value->kind != AB_DATA_NUMBER) {
        return false;
    }
    memcpy(output, &value->bits, sizeof(*output));
    return isfinite(*output);
}

bool ab_theme_layer_definition_metrics(const AbDataTable *table,
                                       const AbThemeSpriteMetrics *metrics,
                                       AbThemeLayerDefinition *output, AbError *error) {
    if (!table || !output || table->count > table->capacity ||
        (table->count && !table->entries)) {
        return ab_fail(error, "Invalid static theme layer table");
    }
    const AbDataValue *name = tuple(table, 2);
    if (!name || name->kind != AB_DATA_STRING ||
        (name->string.size && !name->string.data)) {
        return ab_fail(error, "Theme layer requires a string sprite name");
    }
    AbThemeLayerDefinition definition = {.sprite = name->string,
                                         .has_base_sprite = metrics && metrics->present,
                                         .base = {.color = {1, 1, 1, 1}}};
    if (definition.has_base_sprite) {
        definition.base.width = (float)metrics->width;
        definition.height = (float)metrics->height;
        definition.base.pivot_x = (float)metrics->pivot_x;
        definition.pivot_y = (float)metrics->pivot_y;
    }
    const AbDataValue *speed =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING,
                                         .string = {(const uint8_t *)"v", 1}});
    if (!numeric_default(tuple(table, 3), 1, &definition.parallax) ||
        !numeric_default(tuple(table, 4), 1, &definition.base.scale) ||
        !numeric_default(tuple(table, 6), 0, &definition.base.x_offset) ||
        !numeric_default(tuple(table, 7), 0, &definition.base.vertical_offset) ||
        !numeric_default(speed, 0, &definition.base.speed)) {
        return ab_fail(error, "Unsupported numeric theme layer field");
    }
    const AbDataValue *repeat = tuple(table, 5);
    if (repeat && (unsigned)repeat->kind > AB_DATA_TABLE) {
        return ab_fail(error, "Invalid theme layer repeat field");
    }
    definition.base.repeats =
        !repeat || repeat->kind != AB_DATA_BOOLEAN || repeat->bits != 0;
    definition.base.phase = definition.base.x_offset;
    *output = definition;
    return true;
}

bool ab_theme_layer_definition(const AbDataTable *table, const AbSpriteRegion *region,
                               AbThemeLayerDefinition *output, AbError *error) {
    AbThemeSpriteMetrics metrics = {0};
    if (region) {
        metrics = (AbThemeSpriteMetrics){true, region->width, region->height,
                                         (int32_t)((float)region->pivot_x + 0.5f),
                                         (int32_t)((float)region->pivot_y + 0.5f)};
    }
    return ab_theme_layer_definition_metrics(table, &metrics, output, error);
}
