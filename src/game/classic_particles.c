#include "ab/classic_particles.h"

#include "ab/target_data.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct AbClassicParticles {
    AbClassicParticleConfig config;
    AbClassicLevelView level;
    AbTargetData *files;
    AbParticleSource *sources;
    AbParticles *engine;
    const AbLuaData *copies;
    size_t bytes, resource_bytes;
    bool calling;
};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(AbSpan name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = name};
}

static AbDataValue field(const AbDataTable *table, const char *key) {
    const AbDataValue *value = ab_data_get(table, text(span(key)));
    return value ? *value : (AbDataValue){0};
}

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric classic particle argument");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite classic particle argument");
}

static bool path(AbSpan name, char output[256], AbError *error) {
    const char *prefix = "assets/data/scripts/particles/";
    size_t prefix_length = strlen(prefix);
    if (!name.size || name.size > 255 - prefix_length - 4 || !name.data)
        return ab_fail(error, "Invalid packed particle definition name");
    for (size_t index = 0; index < name.size; ++index) {
        uint8_t byte = name.data[index];
        if (!((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
              (byte >= '0' && byte <= '9') || byte == '_' || byte == '-'))
            return ab_fail(error, "Unsupported packed particle path component");
    }
    memcpy(output, prefix, prefix_length);
    memcpy(output + prefix_length, name.data, name.size);
    memcpy(output + prefix_length + name.size, ".lua", 5);
    return true;
}

bool ab_classic_particles_open(const AbZip *archive, AbClassicParticleConfig config,
                               AbClassicParticles **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing classic particle owner output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    if (!archive || !level.properties || !level.catalog_data ||
        (config.definition_count && !config.definitions) ||
        config.storage_bytes < sizeof(AbClassicParticles))
        return ab_fail(error, "Invalid classic particle binding or storage");
    size_t remaining = config.storage_bytes - sizeof(AbClassicParticles);
    if (config.definition_count > remaining / sizeof(AbTargetData))
        return ab_fail(error, "Classic particle files exceed byte limit");
    remaining -= config.definition_count * sizeof(AbTargetData);
    if (config.definition_count > remaining / sizeof(AbParticleSource))
        return ab_fail(error, "Classic particle source rows exceed byte limit");
    remaining -= config.definition_count * sizeof(AbParticleSource);
    AbClassicParticles *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate classic particles");
    owner->config = config;
    owner->level = level;
    owner->bytes = config.storage_bytes - remaining;
    if (config.definition_count) {
        owner->files = calloc(config.definition_count, sizeof(*owner->files));
        owner->sources = calloc(config.definition_count, sizeof(*owner->sources));
        if (!owner->files || !owner->sources) {
            ab_classic_particles_close(owner);
            return ab_fail(error, "Cannot allocate classic particle sources");
        }
    }
    uint8_t key[32] = {0};
    AbSpan resource_key = {key, sizeof(key)};
    bool okay = true;
    if (config.definition_count && config.source) {
        AbClassicLevelSourceView shared;
        okay = ab_classic_level_source_view(config.source, archive, &shared, error);
        if (okay)
            resource_key = shared.key;
    } else if (config.definition_count)
        okay = ab_target_resource_key(archive, config.resource_bytes, key, error);
    for (size_t index = 0; okay && index < config.definition_count; ++index) {
        char resource[256];
        okay = path(config.definitions[index], resource, error) &&
               ab_target_data_open(archive, resource, resource_key,
                                   config.resource_bytes - owner->resource_bytes, NULL,
                                   owner->files + index, error);
        if (okay) {
            const AbTargetData *file = owner->files + index;
            owner->resource_bytes += file->bytes.size + file->chunk.allocated_bytes +
                                     file->data.allocated_bytes;
            owner->sources[index] =
                (AbParticleSource){config.definitions[index], file->data.globals};
        }
    }
    memset(key, 0, sizeof(key));
    okay = okay &&
           ab_particles_open((AbParticleConfig){.sources = owner->sources,
                                                .source_count = config.definition_count,
                                                .capacity = config.capacity,
                                                .level_capacity = config.level_capacity,
                                                .storage_bytes = remaining,
                                                .random = config.engine_random,
                                                .admission = config.admission,
                                                .context = config.context,
                                                .sprite = config.sprite},
                             &owner->engine, error);
    if (!okay) {
        ab_classic_particles_close(owner);
        return false;
    }
    owner->bytes += ab_particles_bytes(owner->engine);
    *output = owner;
    return true;
}

void ab_classic_particles_close(AbClassicParticles *owner) {
    if (owner) {
        ab_particles_close(owner->engine);
        if (owner->files) {
            for (size_t index = 0; index < owner->config.definition_count; ++index)
                ab_target_data_close(owner->files + index);
        }
        free(owner->sources);
        free(owner->files);
        free(owner);
    }
}

size_t ab_classic_particles_bytes(const AbClassicParticles *owner) {
    return owner ? owner->bytes : 0;
}

size_t ab_classic_particles_resource_bytes(const AbClassicParticles *owner) {
    return owner ? owner->resource_bytes : 0;
}

AbParticles *ab_classic_particles_engine(AbClassicParticles *owner) {
    return owner ? owner->engine : NULL;
}

AbClassicLevel *ab_classic_particles_level(const AbClassicParticles *owner) {
    return owner ? owner->config.level : NULL;
}

const AbDataTable *ab_classic_particles_definition(const AbClassicParticles *owner,
                                                   AbSpan name) {
    if (!owner || (name.size && !name.data))
        return NULL;
    for (size_t index = 0; index < owner->config.definition_count; ++index) {
        AbSpan selected = owner->sources[index].name;
        if (name.size == selected.size &&
            (!name.size || !memcmp(name.data, selected.data, name.size)))
            return owner->sources[index].table;
    }
    return NULL;
}

bool ab_classic_particles_bind_copies(AbClassicParticles *owner,
                                      const AbLuaData *copies, AbError *error) {
    if (!owner || owner->calling || (copies && owner->copies))
        return ab_fail(error, "Invalid runtime particle descriptor owner binding");
    owner->copies = copies;
    return true;
}

static bool amount(AbClassicParticles *owner, const AbDataTable *descriptor,
                   float *output, AbError *error) {
    AbDataValue explicit = field(descriptor, "amount");
    if (truthy(explicit))
        return numeric(explicit, output, error);
    AbDataValue limits = field(descriptor, "amountLimits");
    if (limits.kind != AB_DATA_TABLE || !valid_table(limits.table))
        return ab_fail(error, "Missing classic particle amount limits");
    const AbDataValue *first =
        ab_data_get(limits.table, (AbDataValue){.kind = AB_DATA_NUMBER,
                                                .bits = UINT32_C(0x3f800000)});
    const AbDataValue *second =
        ab_data_get(limits.table, (AbDataValue){.kind = AB_DATA_NUMBER,
                                                .bits = UINT32_C(0x40000000)});
    float low, high;
    if (!first || !second || !numeric(*first, &low, error) ||
        !numeric(*second, &high, error))
        return false;
    if (low < -0x1p31f || low >= 0x1p31f || high < -0x1p31f || high >= 0x1p31f ||
        truncf(low) != low || truncf(high) != high)
        return ab_fail(error, "Unsupported classic particle integer interval");
    return ab_script_random(owner->config.script_random, 2, (int32_t)low, (int32_t)high,
                            output) == AB_SCRIPT_RANDOM_OK ||
           ab_fail(error, "Classic particle amount RNG failed");
}

static bool geometry(AbClassicParticles *owner, size_t object,
                     AbParticleEmission *output, AbDataValue *cached, AbError *error) {
    if (object >= owner->level.properties->count)
        return ab_fail(error, "Unknown classic particle object");
    AbSpan name = owner->level.bodies->objects[object].name;
    AbDataValue current;
    if (!ab_object_world_lookup(&owner->level.properties->lookup, text(name), &current,
                                error))
        return false;
    *cached = current;
    if (current.kind == AB_DATA_NIL)
        return true;
    if (current.kind != AB_DATA_TABLE ||
        current.table != &owner->level.properties->objects[object].physical)
        return ab_fail(error, "Replaced classic particle world object");
    AbParticleEmission emission = {0};
    if (!numeric(field(current.table, "x"), &emission.position.x, error) ||
        !numeric(field(current.table, "y"), &emission.position.y, error))
        return false;
    emission.position.x = emission.position.x * 20;
    emission.position.y = emission.position.y * 20;
    AbDataValue radius = field(current.table, "radius");
    if (radius.kind == AB_DATA_NIL) {
        if (!numeric(field(current.table, "width"), &emission.size.x, error) ||
            !numeric(field(current.table, "height"), &emission.size.y, error))
            return false;
    } else {
        if (!numeric(radius, &emission.size.x, error))
            return false;
        emission.size.x = emission.size.x * 2;
        emission.size.y = emission.size.x;
    }
    emission.size.x = emission.size.x * 20;
    emission.size.y = emission.size.y * 20;
    *output = emission;
    return true;
}

static bool object_angle(AbClassicParticles *owner, size_t object, AbDataValue cached,
                         float *output, AbError *error) {
    AbSpan name = owner->level.bodies->objects[object].name;
    AbDataValue current_name = field(cached.table, "name");
    if (current_name.kind != AB_DATA_STRING || current_name.string.size != name.size ||
        (name.size && (!current_name.string.data ||
                       memcmp(current_name.string.data, name.data, name.size))))
        return ab_fail(error, "Unsupported renamed classic particle angle object");
    /* getAngle161583 reads objects.world[name].angle. A level removal hook can
     * already have removed the native body while the canonical Lua table is
     * still live for subsequent destruction particles. It also retains the
     * published/interpolated pose instead of sampling a newer physics angle. */
    AbDataValue current;
    return ab_object_world_lookup(&owner->level.properties->lookup, text(name),
                                  &current, error) &&
           (current.kind == AB_DATA_TABLE ||
            ab_fail(error, "Missing live classic particle angle table")) &&
           numeric(field(current.table, "angle"), output, error);
}

static bool new_particles(AbClassicParticles *owner, const AbParticleEmission *emission,
                          AbError *error) {
    /* Source newParticles152993 rejects the empty name before reaching the
     * native engine. Caller-side amount RNG, geometry and angle queries have
     * already happened; preserve those even for this suppressed request. */
    return !emission->definition.size ||
           ab_particles_emit(owner->engine, emission, error);
}

static bool contact(AbClassicParticles *owner, size_t object, AbDataValue descriptor,
                    bool ignore_limits, bool alternate, AbError *error) {
    AbParticleEmission emission = {.ignore_limits = ignore_limits,
                                   .alternate = alternate};
    AbDataValue current = {0};
    if (!geometry(owner, object, &emission, &current, error))
        return false;
    if (current.kind == AB_DATA_NIL)
        return true;
    emission.ignore_limits = ignore_limits;
    emission.alternate = alternate;
    if (descriptor.kind != AB_DATA_TABLE || !valid_table(descriptor.table))
        return ab_fail(error, "Invalid classic particle burst descriptor");
    const AbDataTableLayout *layout =
        ab_lua_table_layout(owner->level.catalog_data, descriptor.table);
    bool copied = !layout && owner->copies;
    if (copied)
        layout = ab_lua_table_layout(owner->copies, descriptor.table);
    size_t capacity = 0;
    if (!layout || !layout->no_nil_assignments ||
        !(copied ? ab_data_string_order_growing(
                       descriptor.table, owner->config.workspace, &capacity, error)
                 : ab_data_string_order(descriptor.table, layout->hash_capacity,
                                        owner->config.workspace, error)))
        return ab_fail(error, "Unsupported classic particle descriptor iteration");
    for (size_t index = 0; index < descriptor.table->count; ++index) {
        AbDataValue child =
            descriptor.table->entries[owner->config.workspace->indices[index]].value;
        if (child.kind != AB_DATA_TABLE || !valid_table(child.table))
            return ab_fail(error, "Invalid classic particle burst entry");
        if (!amount(owner, child.table, &emission.amount, error))
            return false;
        AbDataValue definition = field(child.table, "definition");
        if (definition.kind != AB_DATA_STRING)
            return ab_fail(error, "Invalid classic particle burst definition");
        emission.definition = definition.string;
        if (!object_angle(owner, object, current, &emission.angle, error))
            return false;
        if (!new_particles(owner, &emission, error))
            return false;
    }
    return true;
}

bool ab_classic_particles_contact(void *context, size_t object, AbDataValue descriptor,
                                  AbError *error) {
    AbClassicParticles *owner = context;
    if (!owner || owner->calling)
        return ab_fail(error, "Invalid or reentrant classic particle burst");
    owner->calling = true;
    bool okay = contact(owner, object, descriptor, false, false, error);
    owner->calling = false;
    return okay;
}

static bool named_object(AbClassicParticles *owner, AbSpan name, size_t *output,
                         AbError *error) {
    if (name.size && !name.data)
        return ab_fail(error, "Invalid classic particle object name");
    for (size_t index = 0; index < owner->level.bodies->count; ++index) {
        AbSpan candidate = owner->level.bodies->objects[index].name;
        if (candidate.size == name.size &&
            (!name.size || !memcmp(candidate.data, name.data, name.size))) {
            *output = index;
            return true;
        }
    }
    /* The original lookup returns before indexing particle arguments. */
    *output = SIZE_MAX;
    return true;
}

bool ab_classic_particles_removal(void *context, AbSpan name, AbDataValue descriptor,
                                  bool ignore_limits, bool alternate, AbError *error) {
    AbClassicParticles *owner = context;
    if (!owner || owner->calling)
        return ab_fail(error, "Invalid or reentrant classic removal particles");
    owner->calling = true;
    size_t object = SIZE_MAX;
    bool okay = named_object(owner, name, &object, error);
    if (okay && object != SIZE_MAX)
        okay = contact(owner, object, descriptor, ignore_limits, alternate, error);
    owner->calling = false;
    return okay;
}

static AbDataValue item(const AbDataTable *table, size_t index) {
    float number = (float)index;
    uint32_t bits;
    memcpy(&bits, &number, sizeof(bits));
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits});
    return value ? *value : (AbDataValue){0};
}

static bool legacy_emit(AbClassicParticles *owner, size_t object, AbDataValue cached,
                        AbDataValue definition, AbDataValue amount_value,
                        AbParticleEmission *emission, AbError *error) {
    if (!object_angle(owner, object, cached, &emission->angle, error))
        return false;
    if (definition.kind != AB_DATA_STRING)
        return ab_fail(error, "Unsupported legacy particle definition value");
    emission->definition = definition.string;
    return !emission->definition.size ||
           (numeric(amount_value, &emission->amount, error) &&
            new_particles(owner, emission, error));
}

static bool legacy(AbClassicParticles *owner, size_t object, AbDataValue definitions,
                   AbDataValue amount_value, bool ignore_limits, bool alternate,
                   AbError *error) {
    AbParticleEmission emission = {0};
    AbDataValue cached = {0};
    if (!geometry(owner, object, &emission, &cached, error))
        return false;
    if (cached.kind == AB_DATA_NIL)
        return true;
    emission.ignore_limits = ignore_limits;
    emission.alternate = alternate;
    if (definitions.kind != AB_DATA_TABLE)
        return legacy_emit(owner, object, cached, definitions, amount_value, &emission,
                           error);
    if (!valid_table(definitions.table))
        return ab_fail(error, "Invalid legacy particle definition list");
    if (!truthy(item(definitions.table, 1)))
        return legacy_emit(owner, object, cached, definitions, amount_value, &emission,
                           error);
    size_t length = 0;
    for (size_t index = 0; index < definitions.table->count; ++index) {
        AbDataEntry entry = definitions.table->entries[index];
        if (entry.key.kind != AB_DATA_NUMBER || entry.value.kind == AB_DATA_NIL)
            continue;
        float key;
        memcpy(&key, &entry.key.bits, sizeof(key));
        if (!isfinite(key) || key < 1 || key > 16777215 || key != floorf(key))
            return ab_fail(error, "Unsupported legacy particle array key");
        if ((size_t)key > length)
            length = (size_t)key;
    }
    for (size_t index = 1; index <= length; ++index) {
        AbDataValue definition = item(definitions.table, index);
        if (definition.kind == AB_DATA_NIL)
            return ab_fail(error, "Sparse legacy particle definition list");
        const AbDataTable *source = NULL;
        for (size_t slot = 0; slot < owner->config.definition_count; ++slot) {
            AbSpan name = owner->sources[slot].name;
            if (definition.kind == AB_DATA_STRING &&
                definition.string.size == name.size &&
                (!name.size ||
                 (definition.string.data &&
                  !memcmp(definition.string.data, name.data, name.size)))) {
                source = owner->sources[slot].table;
                break;
            }
        }
        if (!source)
            return ab_fail(error, "Missing cached legacy particle definition");
        AbDataValue override = field(source, "amount");
        if (truthy(override))
            amount_value = override;
        if (!legacy_emit(owner, object, cached, definition, amount_value, &emission,
                         error))
            return false;
    }
    return true;
}

bool ab_classic_particles_legacy(void *context, AbSpan name, AbDataValue definitions,
                                 AbDataValue amount_value, bool ignore_limits,
                                 bool alternate, AbError *error) {
    AbClassicParticles *owner = context;
    if (!owner || owner->calling)
        return ab_fail(error, "Invalid or reentrant classic legacy particles");
    owner->calling = true;
    size_t object = SIZE_MAX;
    bool okay = named_object(owner, name, &object, error);
    if (okay && object != SIZE_MAX)
        okay = legacy(owner, object, definitions, amount_value, ignore_limits,
                      alternate, error);
    owner->calling = false;
    return okay;
}

bool ab_classic_particles_ambient(void *context,
                                  const AbAmbientParticleEmission *emission,
                                  AbError *error) {
    AbClassicParticles *owner = context;
    if (!owner || owner->calling || !emission)
        return ab_fail(error, "Invalid or reentrant classic ambient emission");
    owner->calling = true;
    bool okay = new_particles(owner,
                              &(AbParticleEmission){.definition = emission->particles,
                                                    .amount = emission->amount,
                                                    .position = emission->position},
                              error);
    owner->calling = false;
    return okay;
}
