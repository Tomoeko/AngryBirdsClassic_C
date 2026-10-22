#include "ab/classic_touches.h"
#include "ab/integer_order.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbClassicTouch value;
    AbDataEntry entries[4];
    AbDataTable table;
} TouchRecord;

struct AbClassicTouches {
    AbClassicTouchesConfig config;
    AbClassicLevelView level;
    AbClassicTouchesView state;
    TouchRecord *current, *previous;
    int32_t *keys;
    size_t *selected;
    AbIntegerOrderWorkspace order;
    size_t bytes;
    bool calling, failed;
};

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static size_t find(const TouchRecord *records, size_t count, int32_t id) {
    size_t index = 0;
    while (index < count && records[index].value.id != id)
        ++index;
    return index;
}

static void record(TouchRecord *output, AbClassicTouch value, bool id_present) {
    output->value = value;
    output->entries[0] = (AbDataEntry){text("x"), number(value.position.x)};
    output->entries[1] = (AbDataEntry){text("y"), number(value.position.y)};
    output->entries[2] = (AbDataEntry){text("consumed"), value.consumed};
    output->entries[3] = (AbDataEntry){text("touchId"), number((float)value.id)};
    output->table = (AbDataTable){output->entries, id_present ? 4 : 3, 4, NULL};
}

static bool valid_input(AbClassicTouches *owner, const AbClassicTouch *input,
                        size_t count, AbError *error) {
    if (count > owner->config.capacity || (count && !input))
        return ab_fail(error, "Invalid ordinary touch input capacity");
    for (size_t index = 0; index < count; ++index) {
        AbDataValue consumed = input[index].consumed;
        if (consumed.kind < AB_DATA_NIL || consumed.kind > AB_DATA_STRING ||
            (consumed.kind == AB_DATA_STRING && consumed.string.size &&
             !consumed.string.data))
            return ab_fail(error, "Unsupported ordinary touch consumed value");
        owner->keys[index] = input[index].id;
    }
    AbIntegerOrderLayout layout;
    return ab_integer_order_growing(owner->keys, count, &owner->order, &layout, error);
}

static bool script_present(AbClassicTouches *owner, size_t object, bool *present,
                           AbError *error) {
    const AbDataValue *value = ab_data_get(
        &owner->level.properties->objects[object].physical, text("gameObjectScript"));
    *present = value && truthy(*value);
    return !*present ||
           (value->kind == AB_DATA_TABLE &&
            value->table == &owner->level.properties->objects[object].script) ||
           ab_fail(error, "Rebound ordinary touch GameObject script");
}

static bool dispatch(AbClassicTouches *owner, size_t object, TouchRecord *records,
                     size_t count, const TouchRecord *excluded, size_t excluded_count,
                     AbClassicTouchKind kind, float seconds, AbError *error) {
    size_t selected = 0;
    for (size_t index = 0; index < count; ++index) {
        if (excluded &&
            find(excluded, excluded_count, records[index].value.id) != excluded_count)
            continue;
        owner->selected[selected] = index;
        owner->keys[selected++] = records[index].value.id;
    }
    AbIntegerOrderLayout layout;
    if (kind != AB_CLASSIC_TOUCH_UPDATE &&
        !ab_integer_order_growing(owner->keys, selected, &owner->order, &layout, error))
        return false;
    for (size_t cursor = 0; cursor < selected; ++cursor) {
        size_t slot =
            kind == AB_CLASSIC_TOUCH_UPDATE ? cursor : owner->order.indices[cursor];
        if (!ab_classic_behaviors_touch(owner->config.behaviors, object, kind,
                                        &records[owner->selected[slot]].table, seconds,
                                        error))
            return false;
    }
    return true;
}

static bool handle(AbClassicTouches *owner, const AbClassicTouch *input, size_t count,
                   float seconds, AbDataValue *consumed, AbError *error) {
    if (!valid_input(owner, input, count, error))
        return false;
    size_t primary = 0;
    while (primary < count && input[primary].id != owner->state.primary)
        ++primary;
    if (!owner->state.primary_present || primary == count) {
        owner->state.primary_present = count != 0;
        if (count)
            owner->state.primary = input[0].id;
    }
    AbCameraSystemView camera = ab_camera_system_view(owner->config.camera);
    for (size_t index = 0; index < count; ++index) {
        AbClassicTouch value = input[index];
        if (!camera.viewport ||
            !ab_coordinate_point(&camera.viewport->rendering, AB_SPACE_SCREEN,
                                 AB_SPACE_WORLD, value.position, &value.position))
            return ab_fail(error, "Invalid ordinary touch world transform");
        record(owner->current + index, value, true);
    }
    for (size_t cursor = 0; cursor < owner->level.bodies->count; ++cursor) {
        size_t object = owner->level.object_order[cursor];
        if (owner->level.removed_objects[object])
            continue;
        bool present = false;
        if (!script_present(owner, object, &present, error))
            return false;
        if (!present)
            continue;
        if (!dispatch(owner, object, owner->current, count, owner->previous,
                      owner->state.previous_count, AB_CLASSIC_TOUCH_START, seconds,
                      error) ||
            !dispatch(owner, object, owner->current, count, NULL, 0,
                      AB_CLASSIC_TOUCH_UPDATE, seconds, error) ||
            !dispatch(owner, object, owner->previous, owner->state.previous_count,
                      owner->current, count, AB_CLASSIC_TOUCH_END, seconds, error))
            return false;
    }
    for (size_t index = 0; index < count; ++index)
        owner->keys[index] = owner->current[index].value.id;
    AbIntegerOrderLayout layout;
    if (!ab_integer_order_growing(owner->keys, count, &owner->order, &layout, error))
        return false;
    for (size_t index = 0; index < count; ++index)
        record(owner->previous + index,
               owner->current[owner->order.indices[index]].value, true);
    owner->state.previous_count = count;
    *consumed = (AbDataValue){0};
    if (owner->state.primary_present) {
        primary = find(owner->current, count, owner->state.primary);
        if (primary < count)
            *consumed = owner->current[primary].value.consumed;
    }
    return true;
}

bool ab_classic_touches_open(AbClassicTouchesConfig config, AbClassicTouches **output,
                             AbError *error) {
    if (!output)
        return ab_fail(error, "Missing ordinary touch owner output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    size_t slots = 1;
    while (slots < config.capacity && slots <= SIZE_MAX / 2)
        slots *= 2;
    size_t stride = sizeof(TouchRecord) * 2 + sizeof(int32_t) + sizeof(size_t) * 3 +
                    sizeof(AbIntegerOrderNode);
    if (!level.bodies || !config.behaviors || !config.camera ||
        slots < config.capacity || config.storage_bytes < sizeof(AbClassicTouches) ||
        slots > (config.storage_bytes - sizeof(AbClassicTouches)) / stride)
        return ab_fail(error, "Invalid ordinary touch bindings or byte limit");
    AbClassicTouches *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate ordinary touch owner");
    owner->config = config;
    owner->level = level;
    owner->bytes = sizeof(*owner) + slots * stride;
    owner->current = calloc(slots, sizeof(TouchRecord));
    owner->previous = calloc(slots, sizeof(TouchRecord));
    owner->keys = calloc(slots, sizeof(int32_t));
    owner->selected = calloc(slots, sizeof(size_t));
    owner->order =
        (AbIntegerOrderWorkspace){.indices = calloc(slots, sizeof(size_t)),
                                  .index_capacity = slots,
                                  .array_slots = calloc(slots, sizeof(size_t)),
                                  .array_capacity = slots,
                                  .nodes = calloc(slots, sizeof(AbIntegerOrderNode)),
                                  .node_capacity = slots};
    if (!owner->current || !owner->previous || !owner->keys || !owner->selected ||
        !owner->order.indices || !owner->order.array_slots || !owner->order.nodes) {
        ab_classic_touches_close(owner);
        return ab_fail(error, "Cannot allocate ordinary touch snapshots");
    }
    *output = owner;
    return true;
}

void ab_classic_touches_close(AbClassicTouches *owner) {
    if (owner) {
        free(owner->current);
        free(owner->previous);
        free(owner->keys);
        free(owner->selected);
        free(owner->order.indices);
        free(owner->order.array_slots);
        free(owner->order.nodes);
        free(owner);
    }
}

size_t ab_classic_touches_bytes(const AbClassicTouches *owner) {
    return owner ? owner->bytes : 0;
}

AbClassicTouchesView ab_classic_touches_view(const AbClassicTouches *owner) {
    return owner ? owner->state : (AbClassicTouchesView){0};
}

static bool enter(AbClassicTouches *owner, AbError *error) {
    if (!owner || owner->calling || owner->failed || owner->level.world->locked ||
        owner->level.world->failed)
        return ab_fail(error, "Invalid, failed or reentrant ordinary touches");
    owner->calling = true;
    return true;
}

bool ab_classic_touches_handle(AbClassicTouches *owner, const AbClassicTouch *input,
                               size_t count, float seconds, AbDataValue *consumed,
                               AbError *error) {
    if (!consumed || !enter(owner, error))
        return ab_fail(error, "Invalid ordinary touch result binding");
    bool okay = handle(owner, input, count, seconds, consumed, error);
    owner->calling = false;
    owner->failed = !okay;
    return okay;
}

bool ab_classic_touches_cancel(AbClassicTouches *owner, const AbClassicTouch *input,
                               size_t count, AbError *error) {
    if (!enter(owner, error))
        return false;
    bool okay = valid_input(owner, input, count, error);
    for (size_t index = 0; okay && index < count; ++index)
        record(owner->current + index, input[index], false);
    for (size_t cursor = 0; okay && cursor < owner->level.bodies->count; ++cursor) {
        size_t object = owner->level.object_order[cursor];
        bool present = false;
        if (owner->level.removed_objects[object])
            continue;
        okay = script_present(owner, object, &present, error);
        for (size_t index = 0; okay && present && index < count; ++index)
            okay = ab_classic_behaviors_touch(owner->config.behaviors, object,
                                              AB_CLASSIC_TOUCH_CANCEL,
                                              &owner->current[index].table, 0, error);
    }
    owner->calling = false;
    owner->failed = !okay;
    return okay;
}
