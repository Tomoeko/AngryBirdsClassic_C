#include "ab/theme_rect.h"

#include <limits.h>
#include <math.h>
#include <string.h>

static AbDataValue field(const AbDataTable *table, const char *key) {
    const AbDataValue *value = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)key, strlen(key)}});
    return value ? *value : (AbDataValue){0};
}

static bool numeric(const AbDataTable *table, const char *key, float fallback,
                    float *output, AbError *error) {
    AbDataValue value = field(table, key);
    if (value.kind == AB_DATA_STRING)
        return ab_fail(error, "Numeric-string rectangle theme fields are unsupported");
    if (value.kind == AB_DATA_NUMBER)
        memcpy(output, &value.bits, sizeof(*output));
    else
        *output = fallback;
    return isfinite(*output) || ab_fail(error, "Nonfinite rectangle theme field");
}

bool ab_theme_rect_read(const AbDataTable *layer, AbThemeRect *output, AbError *error) {
    if (!layer || layer->count > layer->capacity || (layer->count && !layer->entries))
        return ab_fail(error, "Invalid rectangle theme layer data");
    AbDataValue value = field(layer, "rect");
    if (!output || value.kind != AB_DATA_TABLE || !value.table ||
        value.table->count > value.table->capacity ||
        (value.table->count && !value.table->entries))
        return ab_fail(error, "Rectangle theme layer requires its rect table");
    const AbDataTable *table = value.table;
    const char *const colors[] = {"r", "g", "b", "a"};
    const uint32_t word = UINT32_C(0x3b808081);
    float reciprocal = 0;
    memcpy(&reciprocal, &word, sizeof(reciprocal));
    AbThemeRect rect = {0};
    for (size_t index = 0; index < 4; ++index) {
        if (!numeric(table, colors[index], 0, rect.color + index, error))
            return false;
        rect.color[index] = rect.color[index] * reciprocal;
    }
    if (!numeric(table, "relLeft", 0, &rect.left, error) ||
        !numeric(table, "relBottom", 0, &rect.bottom, error) ||
        !numeric(table, "relWidth", 1, &rect.width, error) ||
        !numeric(table, "relHeight", 1, &rect.height, error))
        return false;
    *output = rect;
    return true;
}

bool ab_theme_rect_plan(const AbThemeRect *rect, uint32_t width, uint32_t height,
                        AbGameRectanglePlan *output, bool *active, AbError *error) {
    if (!rect || !output || !active || !width || !height || width > INT32_MAX ||
        height > INT32_MAX || !isfinite(rect->color[3]))
        return ab_fail(error, "Invalid rectangle theme draw bindings");
    if (!(rect->color[3] > 0)) {
        *active = false;
        return true;
    }
    float left = (float)width * rect->left;
    float top = (float)height * rect->bottom;
    AbGameRectangle rectangle = {.left = left,
                                 .top = top,
                                 .right = left + (float)width * rect->width,
                                 .bottom = top + (float)height * rect->height};
    memcpy(rectangle.color, rect->color, sizeof(rectangle.color));
    if (!ab_game_rectangle_plan(&rectangle, output, error))
        return false;
    *active = true;
    return true;
}
