#include "ab/ambient_particle_data.h"
#include "component_data_internal.h"

static bool optional_number(const AbDataValue *value, float *output, bool *present) {
    *present = value && value->kind != AB_DATA_NIL &&
               (value->kind != AB_DATA_BOOLEAN || value->bits);
    return !*present || number(value, output);
}

static bool descriptor(const AbDataTable *table, AbAmbientParticle *output) {
    if (!valid_table(table)) {
        return false;
    }
    AbAmbientParticle particle = {0};
    const AbDataValue *name = field(table, "particles");
    if (!name || name->kind != AB_DATA_STRING ||
        (name->string.size && !name->string.data) ||
        !number(field(table, "amount"), &particle.amount) ||
        !number(field(table, "x"), &particle.offset.x) ||
        !number(field(table, "y"), &particle.offset.y) ||
        !optional_number(field(table, "timer"), &particle.timer,
                         &particle.timer_present) ||
        !optional_number(field(table, "delay"), &particle.delay,
                         &particle.delay_present)) {
        return false;
    }
    particle.particles = name->string;
    if (!particle.delay_present &&
        (!number(field(table, "minDelay"), &particle.min_delay) ||
         !number(field(table, "maxDelay"), &particle.max_delay))) {
        return false;
    }
    *output = particle;
    return true;
}

bool ab_ambient_particle_data(const AbDataTable *component, const size_t *entry_order,
                              AbAmbientParticle *particles, size_t capacity,
                              size_t *output_count, AbError *error) {
    if (!valid_table(component) || !output_count || component->count > capacity ||
        component->count > UINT32_C(16777216) ||
        (component->count && (!particles || !entry_order))) {
        return ab_fail(error, "Invalid ambient particle component storage");
    }
    for (size_t index = 0; index < component->count; ++index) {
        size_t slot = entry_order[index];
        if (slot >= component->count) {
            return ab_fail(error, "Invalid ambient particle entry order");
        }
        for (size_t previous = 0; previous < index; ++previous) {
            if (entry_order[previous] == slot) {
                return ab_fail(error, "Duplicate ambient particle entry order");
            }
        }
        const AbDataValue *value = &component->entries[slot].value;
        if (value->kind != AB_DATA_TABLE ||
            !descriptor(value->table, particles + index)) {
            return ab_fail(error, "Unsupported ambient particle descriptor");
        }
    }
    *output_count = component->count;
    return true;
}
