#include "ab/level_clock.h"
#include "ab/script_math.h"

#include <math.h>
#include <string.h>

typedef struct {
    AbDataValue *x, *y;
    float horizontal, vertical;
} Position;

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static AbDataValue *field(AbDataTable *table, const char *name) {
    size_t length = strlen(name);
    if (!valid_table(table))
        return NULL;
    for (size_t index = 0; index < table->count; ++index) {
        AbDataEntry *entry = table->entries + index;
        if (entry->key.kind == AB_DATA_STRING && entry->key.string.size == length &&
            (!length || (entry->key.string.data &&
                         memcmp(entry->key.string.data, name, length) == 0)))
            return &entry->value;
    }
    return NULL;
}

static bool truthy(const AbDataValue *value) {
    return value && value->kind != AB_DATA_NIL &&
           (value->kind != AB_DATA_BOOLEAN || value->bits != 0);
}

static bool position(AbDataValue *object, Position *output, AbError *error) {
    if (!object || object->kind != AB_DATA_TABLE || !valid_table(object->table))
        return ab_fail(error, "Clock level indexes a non-table object");
    Position value = {.x = field(object->table, "x"), .y = field(object->table, "y")};
    if (!value.x || !value.y || value.x->kind != AB_DATA_NUMBER ||
        value.y->kind != AB_DATA_NUMBER)
        return ab_fail(error, "Clock level requires numeric authored positions");
    memcpy(&value.horizontal, &value.x->bits, sizeof(float));
    memcpy(&value.vertical, &value.y->bits, sizeof(float));
    if (!isfinite(value.horizontal) || !isfinite(value.vertical))
        return ab_fail(error, "Clock level position is nonfinite");
    *output = value;
    return true;
}

static bool move(Position *value, const Position *center, float angle, AbError *error) {
    float x = value->horizontal - center->horizontal;
    float y = value->vertical - center->vertical;
    float squared_x = (float)pow((double)x, 2.0);
    float squared_y = (float)pow((double)y, 2.0);
    float squared = squared_x + squared_y;
    float radius = (float)sqrt((double)squared);
    float cosine, sine;
    if (!isfinite(radius) || !ab_script_cosine(angle, &cosine) ||
        !ab_script_sine(angle, &sine))
        return ab_fail(error, "Clock level exceeds supported numeric geometry");
    x = cosine * radius;
    x = center->horizontal + x;
    y = sine * radius;
    y = center->vertical + y;
    if (!isfinite(x) || !isfinite(y))
        return ab_fail(error, "Clock level rotated position is nonfinite");
    memcpy(&value->x->bits, &x, sizeof(x));
    memcpy(&value->y->bits, &y, sizeof(y));
    return true;
}

bool ab_level_clock_prepare(const AbLuaData *level, const AbCalendarTime *time,
                            AbError *error) {
    if (!level || !valid_table(level->globals) || !time || !isfinite(time->minutes) ||
        time->minutes < 0 || time->minutes > 59 || !isfinite(time->seconds) ||
        time->seconds < 0 || time->seconds > 60)
        return ab_fail(error, "Invalid clock level or native calendar input");
    AbDataValue *world = field(level->globals, "world");
    if (!world || world->kind != AB_DATA_TABLE || !valid_table(world->table))
        return ab_fail(error, "Clock level has no loaded world");
    const char *const names[] = {"StaticBlockTheme17_06_1", "WoodBlock1_1",
                                 "WoodBlock1_2", "LightBlock1_1"};
    AbDataValue *objects[4];
    bool present = true;
    for (size_t index = 0; index < 4; ++index) {
        objects[index] = field(world->table, names[index]);
        present = present && truthy(objects[index]);
    }
    if (!present)
        return true;
    Position center, first, second;
    if (!position(objects[0], &center, error) || !position(objects[1], &first, error) ||
        !position(objects[2], &second, error))
        return false;
    float fraction = time->minutes / 60.0f;
    float seconds = time->seconds / 3600.0f;
    fraction = fraction + seconds;
    float angle = fraction * 0x1.921fb6p+1f;
    angle = angle * 2.0f;
    float quarter = 0x1.921fb6p+1f * 0.5f;
    angle = angle - quarter;
    if (!move(&first, &center, angle, error))
        return false;
    *second.x = *first.x;
    *second.y = *first.y;
    Position light;
    if (!position(objects[3], &light, error))
        return false;
    /* The source calculates a seconds angle but passes the minute angle to
     * both trig calls for this object. Keep that visible source behavior. */
    return move(&light, &center, angle, error);
}
