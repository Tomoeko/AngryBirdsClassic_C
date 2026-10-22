#include "ab/ambient_animation_data.h"
#include "component_data_internal.h"

static bool read_variant(const AbDataTable *table, AbSpan *sprites, size_t capacity,
                         AbAmbientAnimationVariant *output) {
    if (!valid_table(table)) {
        return false;
    }
    AbAmbientAnimationVariant variant = {.sprites = sprites};
    const AbDataValue *frames = field(table, "sprites");
    if (!number(field(table, "speed"), &variant.speed) || !frames ||
        frames->kind != AB_DATA_TABLE ||
        !sprite_array(frames->table, sprites, capacity)) {
        return false;
    }
    variant.count = frames->table->count;
    const AbDataValue *delay = field(table, "delay");
    if (delay && delay->kind != AB_DATA_NIL &&
        (delay->kind != AB_DATA_BOOLEAN || delay->bits)) {
        if (!number(delay, &variant.delay)) {
            return false;
        }
        variant.delay_present = true;
    } else if (!number(field(table, "minDelay"), &variant.min_delay) ||
               !number(field(table, "maxDelay"), &variant.max_delay)) {
        return false;
    }
    const AbDataValue *timer = field(table, "timer");
    if (timer && timer->kind != AB_DATA_NIL) {
        if (!number(timer, &variant.timer)) {
            return false;
        }
        variant.timer_present = true;
    }
    *output = variant;
    return true;
}

bool ab_ambient_animation_data(const AbDataTable *component, const size_t *entry_order,
                               AbAmbientAnimationStorage storage,
                               AbAmbientAnimationConfiguration *output,
                               AbError *error) {
    if (!valid_table(component) || !output || component->count > UINT32_C(16777216) ||
        component->count > storage.variant_capacity ||
        (component->count && (!entry_order || !storage.variants))) {
        return ab_fail(error, "Invalid ambient animation component storage");
    }
    size_t used = 0;
    for (size_t index = 0; index < component->count; ++index) {
        size_t slot = entry_order[index];
        if (slot >= component->count) {
            return ab_fail(error, "Invalid ambient animation entry order");
        }
        for (size_t previous = 0; previous < index; ++previous) {
            if (entry_order[previous] == slot) {
                return ab_fail(error, "Duplicate ambient animation entry order");
            }
        }
        const AbDataValue *value = &component->entries[slot].value;
        AbSpan *sprites = storage.sprites ? storage.sprites + used : NULL;
        if (value->kind != AB_DATA_TABLE ||
            !read_variant(value->table, sprites, storage.sprite_capacity - used,
                          storage.variants + index)) {
            return ab_fail(error, "Unsupported ambient animation descriptor");
        }
        used += storage.variants[index].count;
    }
    *output =
        (AbAmbientAnimationConfiguration){storage.variants, component->count, used};
    return true;
}
