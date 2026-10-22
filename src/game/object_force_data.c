#include "ab/object_force_data.h"

#include <math.h>
#include <string.h>

static AbDataValue property(const AbObjectDefinition *definition, const char *name) {
    return ab_object_property(definition,
                              (AbSpan){(const uint8_t *)name, strlen(name)});
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool numeric(AbDataValue value, float *output, bool *present) {
    if (value.kind == AB_DATA_STRING) {
        return false;
    }
    *present = value.kind == AB_DATA_NUMBER;
    *output = 0;
    if (*present) {
        memcpy(output, &value.bits, sizeof(*output));
        return isfinite(*output);
    }
    return (unsigned)value.kind <= AB_DATA_TABLE;
}

static bool string(AbDataValue value, AbSpan *output) {
    if (value.kind != AB_DATA_STRING || (value.string.size && !value.string.data)) {
        return false;
    }
    *output = value.string;
    return true;
}

static bool read_sound(const AbObjectDefinition *definition, AbObjectForceData *data) {
    AbDataValue value = property(definition, "soundWhenForceApplied");
    if (value.kind == AB_DATA_STRING) {
        data->sound.kind = AB_FORCE_SOUND_NAMED;
        if (!string(value, &data->sound.name)) {
            return false;
        }
    } else if (value.kind == AB_DATA_TABLE) {
        if (!value.table || !value.table->count || !value.table->entries ||
            value.table->count > value.table->capacity ||
            value.table->count > UINT32_C(16777216)) {
            return false;
        }
        data->sound.kind = AB_FORCE_SOUND_ARRAY;
        data->sound.count = value.table->count;
        data->sound_choices = value.table;
    } else if (value.kind != AB_DATA_NIL && value.kind != AB_DATA_BOOLEAN) {
        return false;
    }
    return numeric(property(definition, "soundWhenForceAppliedVolume"),
                   &data->sound.gain, &data->sound.gain_present);
}

bool ab_object_force_data(const AbObjectDefinition *definition, float initial_mass,
                          bool gravity_enabled, AbDataValue current_sprite,
                          AbObjectForceData *output, AbError *error) {
    if (!definition || !definition->source || !output || !isfinite(initial_mass) ||
        initial_mass < 0) {
        return ab_fail(error, "Invalid initial object force data");
    }
    AbObjectForceData data = {
        .forces = {.object_mass = initial_mass, .gravity_enabled = gravity_enabled}};
    const char *const names[] = {"forceX",
                                 "forceY",
                                 "forceRelative",
                                 "airResistance",
                                 "frictionXRelative",
                                 "frictionYRelative"};
    float values[6];
    bool present[6];
    for (size_t index = 0; index < 6; ++index) {
        AbDataValue value = property(definition, names[index]);
        data.prepare_forces = data.prepare_forces || truthy(value);
        if (!numeric(value, values + index, present + index)) {
            return ab_fail(error, "Unsupported initial force field %s", names[index]);
        }
    }
    data.forces.force = (AbVec2){values[0], values[1]};
    data.forces.force_x_present = present[0];
    data.forces.force_y_present = present[1];
    data.forces.relative_force = values[2];
    data.forces.relative_present = present[2];
    data.friction.air_resistance = values[3];
    data.friction.relative = (AbVec2){values[4], values[5]};
    AbDataValue disabled = property(definition, "disableForce");
    data.forces.disabled = disabled.kind == AB_DATA_BOOLEAN && disabled.bits != 0;
    if (!numeric(property(definition, "targetVelocity"), &data.forces.target_velocity,
                 &data.forces.target_present) ||
        !numeric(property(definition, "applyForceInterval"), &data.interval.interval,
                 &data.interval_present) ||
        !read_sound(definition, &data)) {
        return ab_fail(error, "Unsupported initial force control fields");
    }
    if (current_sprite.kind == AB_DATA_STRING) {
        if (!string(current_sprite, &data.interval.sprite)) {
            return ab_fail(error, "Invalid current force sprite");
        }
    }
    if (data.interval_present &&
        (!string(current_sprite, &data.interval.sprite) ||
         !string(property(definition, "spriteWhenForceApplied"),
                 &data.interval.applied_sprite))) {
        return ab_fail(error, "Unsupported initial force interval sprites");
    }
    *output = data;
    return true;
}

bool ab_object_force_choices(const AbDataTable *table, AbSpan *choices, size_t capacity,
                             AbError *error) {
    if (!table || !table->count || !table->entries || !choices ||
        table->count > table->capacity || table->count > capacity ||
        table->count > UINT32_C(16777216)) {
        return ab_fail(error, "Invalid initial force sound choices");
    }
    for (size_t index = 0; index < table->count; ++index) {
        choices[index] = (AbSpan){.size = SIZE_MAX};
    }
    for (size_t index = 0; index < table->count; ++index) {
        const AbDataEntry *entry = table->entries + index;
        float key = 0;
        bool present = false;
        AbSpan name;
        if (!numeric(entry->key, &key, &present) || !present || key < 1 ||
            key > (float)table->count || key != (float)(size_t)key ||
            !string(entry->value, &name) || name.size == SIZE_MAX) {
            return ab_fail(error, "Unsupported initial force sound choice");
        }
        size_t slot = (size_t)key - 1;
        if (choices[slot].size != SIZE_MAX) {
            return ab_fail(error, "Duplicate initial force sound choice");
        }
        choices[slot] = name;
    }
    return true;
}
