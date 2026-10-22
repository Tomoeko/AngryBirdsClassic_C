#include "ab/damage_particles.h"

#include "ab/coordinates.h"

#include <math.h>
#include <string.h>

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)name, strlen(name)}});
    return value ? *value : (AbDataValue){0};
}

bool ab_damage_state_particles(const AbDataTable *raw_definition,
                               const AbVec2 *object_position,
                               AbAmbientParticleHost host, AbError *error) {
    if (!valid_table(raw_definition)) {
        return ab_fail(error, "Invalid raw damage-particle definition");
    }
    AbDataValue descriptor = field(raw_definition, "damageStateChangedParticles");
    if (descriptor.kind == AB_DATA_NIL) {
        return true;
    }
    if (!object_position || !isfinite(object_position->x) ||
        !isfinite(object_position->y)) {
        return ab_fail(error, "Invalid damage-particle physics position");
    }
    AbAmbientParticleEmission emission = {0};
    if (host.physics_to_world) {
        if (!host.physics_to_world(host.context, *object_position, &emission.position,
                                   error)) {
            return false;
        }
    } else if (!ab_coordinate_point(NULL, AB_SPACE_PHYSICS, AB_SPACE_WORLD,
                                    *object_position, &emission.position)) {
        return ab_fail(error, "Cannot convert damage-particle position");
    }
    if (descriptor.kind != AB_DATA_TABLE || !valid_table(descriptor.table)) {
        return ab_fail(error, "Invalid non-nil damage-particle descriptor");
    }
    AbDataValue name = field(descriptor.table, "name");
    AbDataValue amount = field(descriptor.table, "amount");
    if (name.kind != AB_DATA_STRING || (name.string.size && !name.string.data) ||
        amount.kind != AB_DATA_NUMBER) {
        return ab_fail(error,
                       "Damage particles require a string name and numeric amount");
    }
    emission.particles = name.string;
    memcpy(&emission.amount, &amount.bits, sizeof(emission.amount));
    if (!isfinite(emission.amount) || !isfinite(emission.position.x) ||
        !isfinite(emission.position.y) || !host.emit) {
        return ab_fail(error, "Invalid damage-particle emission or host");
    }
    return host.emit(host.context, &emission, error);
}
