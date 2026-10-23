#include "ab/classic_particles.h"

#include <string.h>

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    AbDataValue key = {.kind = AB_DATA_STRING, .string = span(name)};
    const AbDataValue *found = ab_data_get(table, key);
    return found ? *found : (AbDataValue){0};
}

static bool same(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || !memcmp(first.data, second.data, first.size));
}

static bool table(const AbDataTable *value) {
    return value && value->count <= value->capacity &&
           (!value->count || value->entries);
}

static bool append(AbClassicParticleNames *names, AbDataValue value, AbError *error) {
    /* newParticles152993 returns for the empty material name. It identifies
     * no resource file, even though Lua considers an empty string truthy. */
    if (value.kind == AB_DATA_STRING && !value.string.size)
        return true;
    if (value.kind != AB_DATA_STRING || !value.string.size || !value.string.data)
        return ab_fail(error,
                       "Invalid initial particle resource name (kind=%u, bytes=%zu)",
                       (unsigned)value.kind, value.string.size);
    for (size_t index = 0; index < names->count; ++index)
        if (same(names->names[index], value.string))
            return true;
    if (names->count == names->capacity)
        return ab_fail(error, "Initial particle resource list exceeds capacity");
    names->names[names->count++] = value.string;
    return true;
}

static bool descriptors(AbDataValue value, AbClassicParticleNames *names, size_t depth,
                        size_t *work, AbError *error) {
    if (!*work || depth > 32)
        return ab_fail(error, "Initial particle descriptor traversal exceeds bound");
    --*work;
    if (value.kind == AB_DATA_NIL || value.kind == AB_DATA_BOOLEAN)
        return true;
    if (value.kind == AB_DATA_STRING)
        return append(names, value, error);
    if (value.kind != AB_DATA_TABLE || !table(value.table))
        return ab_fail(error, "Invalid initial particle descriptor tree");
    AbDataValue definition = field(value.table, "definition");
    if (definition.kind != AB_DATA_NIL && !append(names, definition, error))
        return false;
    for (size_t index = 0; index < value.table->count; ++index) {
        if (!*work)
            return ab_fail(error, "Initial particle descriptor entries exceed bound");
        --*work;
        const AbDataEntry *entry = value.table->entries + index;
        if (entry->value.kind == AB_DATA_TABLE ||
            (entry->key.kind == AB_DATA_NUMBER &&
             entry->value.kind == AB_DATA_STRING)) {
            if (!descriptors(entry->value, names, depth + 1, work, error))
                return false;
        }
    }
    return true;
}

bool ab_classic_particle_names(AbClassicLevel *owner, AbClassicParticleNames *names,
                               AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(owner);
    if (!level.bodies || !level.properties || !names ||
        names->count > names->capacity || (names->capacity && !names->names))
        return ab_fail(error, "Invalid initial particle resource collection");
    for (size_t index = 0; index < names->count; ++index)
        if (!names->names[index].size || !names->names[index].data)
            return ab_fail(error, "Invalid retained particle resource name");
    size_t work = names->work_limit;
    for (size_t index = 0; index < level.bodies->count; ++index) {
        const AbLevelObject *object = level.bodies->objects + index;
        if (same(object->name, span("ground")))
            continue;
        if (!work)
            return ab_fail(error, "Initial particle object traversal exceeds bound");
        --work;
        AbDataValue particles;
        AbError reached = {0};
        if (!ab_level_property_get(level.properties, index, span("particles"),
                                   &particles, &reached) ||
            !descriptors(particles, names, 0, &work, &reached))
            return ab_fail(error, "Object %.*s particle preload: %s",
                           (int)(object->name.size < 96 ? object->name.size : 96),
                           object->name.data, reached.message);
        AbObjectDefinition definition;
        if (!ab_object_definition_open(level.catalog, object->definition_name, NULL,
                                       &definition, error))
            return false;
        AbDataValue specialty = field(definition.source, "specialty");
        if (specialty.kind == AB_DATA_STRING &&
            same(specialty.string, span("GRENADE"))) {
            AbDataValue name = field(definition.source, "projectile");
            AbObjectDefinition projectile;
            if (name.kind != AB_DATA_STRING ||
                !ab_object_definition_open(level.catalog, name.string, NULL,
                                           &projectile, error) ||
                !descriptors(field(projectile.source, "particles"), names, 0, &work,
                             error))
                return error && error->message[0]
                           ? false
                           : ab_fail(error, "Invalid white bird projectile particles");
        }
        AbDataValue damage = field(definition.source, "damageStateChangedParticles");
        if (damage.kind == AB_DATA_NIL)
            continue;
        if (damage.kind != AB_DATA_TABLE || !table(damage.table) ||
            !append(names, field(damage.table, "name"), error))
            return error && error->message[0]
                       ? false
                       : ab_fail(error, "Invalid initial damage-state particle data");
    }
    return true;
}
