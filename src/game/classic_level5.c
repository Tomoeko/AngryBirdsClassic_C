#include "ab/classic_level5.h"

#include <math.h>
#include <string.h>

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)});
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool coordinate(const AbDataTable *table, const char *name, float *output,
                       AbError *error) {
    AbDataValue value = field(table, name);
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric Level5 treasure position");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite Level5 treasure position");
}

bool ab_classic_level5_update(AbClassicLevel5 *state, AbClassicLevel5Input input,
                              bool (*achieve)(void *context, AbSpan name,
                                              AbError *error),
                              void *context, AbError *error) {
    static const char *const keys[] = {"O", "P", "E", "N"};
    size_t count = input.accelerometer ? 1 : 4;
    if (!state || !input.world || !input.pressed || state->key_index > count)
        return ab_fail(error, "Invalid Level5 script state/owners");
    AbDataValue chest = field(input.world, "ExtraTreasureChest_1");
    if (state->key_index < count &&
        truthy(field(input.pressed,
                     input.accelerometer ? "KEY_GAMING_B" : keys[state->key_index])) &&
        chest.kind != AB_DATA_NIL)
        ++state->key_index;
    bool keyboard = input.desktop || input.accelerometer;
    if (!((input.double_click && !keyboard) ||
          (state->key_index == count && keyboard)) ||
        chest.kind == AB_DATA_NIL)
        return true;
    if (chest.kind != AB_DATA_TABLE || !chest.table)
        return ab_fail(error, "Rebound Level5 treasure world object");
    float x, y;
    if (!coordinate(chest.table, "x", &x, error) ||
        !coordinate(chest.table, "y", &y, error))
        return false;
    float dx = input.cursor_physics.x - x, dy = input.cursor_physics.y - y;
    float squared = dx * dx;
    squared += dy * dy;
    float distance = (float)sqrt((double)squared);
    if (!isfinite(distance))
        return ab_fail(error, "Nonfinite Level5 treasure distance");
    if (distance >= 2.5f && !keyboard)
        return true;
    return achieve ? achieve(context, span("LevelGE_15"), error)
                   : ab_fail(error, "Missing actual Level5 golden-egg host");
}
