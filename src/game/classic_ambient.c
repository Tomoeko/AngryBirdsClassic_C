#include "ab/classic_ambient.h"
#include "ab/ambient_animation_data.h"
#include "ab/ambient_particle_data.h"
#include "ab/identity_map.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbAmbientAnimationState state;
    AbAmbientAnimationVariant *variants;
    AbAmbientParticle *particles;
    AbSpan *sprites;
    size_t count, sprite_count, capacity, sprite_capacity;
} AmbientObject;

struct AbClassicAmbient {
    AbClassicAmbientConfig config;
    AbClassicLevelView level;
    AbIdentityMap *objects;
    AmbientObject *rows;
    size_t count, bytes;
    bool busy, failed;
};

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)name, strlen(name)}});
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static const AbDataTable *component(const AbClassicAmbient *owner,
                                    const AbDataTable *definition) {
    AbDataValue components = field(definition, "components");
    AbDataValue animation =
        components.kind == AB_DATA_TABLE
            ? field(components.table, owner->config.kind == AB_CLASSIC_AMBIENT_PARTICLES
                                          ? "ambientParticles"
                                          : "ambientAnimations")
            : (AbDataValue){0};
    return animation.kind == AB_DATA_TABLE ? animation.table : NULL;
}

static bool reserve(AbClassicAmbient *owner, size_t count, size_t size,
                    AbError *error) {
    if (count > (owner->config.storage_bytes - owner->bytes) / size)
        return ab_fail(error, "AmbientAnimations exceeds its retained storage bound");
    owner->bytes += count * size;
    return true;
}

static uintptr_t identity(const AbClassicAmbient *owner, size_t object) {
    return (uintptr_t)&owner->level.properties->objects[object].physical;
}

static bool reserve_object(AbClassicAmbient *owner, size_t object,
                           const AbDataTable *source, AbError *error) {
    /* A singleton's source pairs order is independent of its key/hash layout. */
    if (source->count != 1 || source->capacity < source->count || !source->entries ||
        source->entries[0].value.kind != AB_DATA_TABLE)
        return ab_fail(error, "AmbientAnimations requires its recovered variant order");
    AmbientObject *row = owner->rows + object;
    row->capacity = owner->config.applications_per_object;
    if (owner->config.kind == AB_CLASSIC_AMBIENT_PARTICLES) {
        if (!reserve(owner, row->capacity, sizeof(*row->particles), error))
            return false;
        row->particles = calloc(row->capacity, sizeof(*row->particles));
        return row->particles ||
               ab_fail(error, "Cannot allocate ambient particle records");
    }
    AbDataValue frames = field(source->entries[0].value.table, "sprites");
    if (frames.kind != AB_DATA_TABLE || !frames.table ||
        frames.table->count > SIZE_MAX / owner->config.applications_per_object)
        return ab_fail(error, "Invalid actual ambient frame array");
    row->sprite_capacity = frames.table->count * row->capacity;
    if (!reserve(owner, row->capacity, sizeof(*row->variants), error) ||
        !reserve(owner, row->sprite_capacity, sizeof(*row->sprites), error))
        return false;
    row->variants = calloc(row->capacity, sizeof(*row->variants));
    if (row->sprite_capacity)
        row->sprites = calloc(row->sprite_capacity, sizeof(*row->sprites));
    return (row->variants && (!row->sprite_capacity || row->sprites)) ||
           ab_fail(error, "Cannot allocate actual ambient animation records");
}

bool ab_classic_ambient_open(AbClassicAmbientConfig config, AbClassicAmbient **output,
                             AbError *error) {
    if (!output)
        return ab_fail(error, "Missing AmbientAnimations owner output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    if (!level.bodies || !level.properties || !config.applications_per_object ||
        (config.kind != AB_CLASSIC_AMBIENT_ANIMATIONS &&
         config.kind != AB_CLASSIC_AMBIENT_PARTICLES) ||
        config.storage_bytes < sizeof(AbClassicAmbient))
        return ab_fail(error, "Invalid AmbientAnimations owners/storage");
    AbClassicAmbient *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate AmbientAnimations owner");
    owner->config = config;
    owner->level = level;
    owner->count = level.bodies->count;
    owner->bytes = sizeof(*owner);
    if (!reserve(owner, owner->count, sizeof(*owner->rows), error))
        goto release;
    if (owner->count) {
        owner->rows = calloc(owner->count, sizeof(*owner->rows));
        if (!owner->rows) {
            ab_fail(error, "Cannot allocate ambient object rows");
            goto release;
        }
    }
    size_t active = 0;
    for (size_t object = 0; object < owner->count; ++object) {
        AbSpan name = level.bodies->objects[object].name;
        if (name.size == 6 && !memcmp(name.data, "ground", 6))
            continue;
        AbDataValue definition;
        if (!ab_object_definition_lookup(&level.properties->lookup, name, &definition,
                                         error))
            goto release;
        const AbDataTable *source = definition.kind == AB_DATA_TABLE
                                        ? component(owner, definition.table)
                                        : NULL;
        if (source) {
            ++active;
            if (!reserve_object(owner, object, source, error))
                goto release;
        }
    }
    if (active &&
        (!config.random ||
         (config.kind == AB_CLASSIC_AMBIENT_ANIMATIONS
              ? ab_classic_visuals_view(config.visuals).level != config.level
              : ab_classic_particles_level(config.particles) != config.level))) {
        ab_fail(error, "Active ambient component requires shared services/RNG");
        goto release;
    }
    size_t nodes = active ? 1 : 0;
    while (nodes < active) {
        if (nodes > SIZE_MAX / 2) {
            ab_fail(error, "Ambient identity capacity overflow");
            goto release;
        }
        nodes *= 2;
    }
    if (!ab_identity_map_open(0, nodes, config.storage_bytes - owner->bytes,
                              &owner->objects, error))
        goto release;
    owner->bytes += ab_identity_map_bytes(owner->objects);
    *output = owner;
    return true;
release:
    ab_classic_ambient_close(owner);
    return false;
}

void ab_classic_ambient_close(AbClassicAmbient *owner) {
    if (!owner)
        return;
    if (owner->rows)
        for (size_t index = 0; index < owner->count; ++index) {
            free(owner->rows[index].sprites);
            free(owner->rows[index].variants);
            free(owner->rows[index].particles);
        }
    ab_identity_map_close(owner->objects);
    free(owner->rows);
    free(owner);
}

size_t ab_classic_ambient_bytes(const AbClassicAmbient *owner) {
    return owner ? owner->bytes : 0;
}

bool ab_classic_ambient_apply(AbClassicAmbient *owner, size_t object,
                              const AbDataTable *definition, AbError *error) {
    if (!owner || owner->busy || owner->failed || object >= owner->count)
        return ab_fail(error, "Invalid or stopped ambient application");
    const AbDataTable *source = component(owner, definition);
    AmbientObject *row = owner->rows + object;
    uintptr_t previous;
    bool present;
    if (!source || source->count != 1 ||
        !ab_identity_map_get(owner->objects, identity(owner, object), &previous,
                             &present, error))
        return ab_fail(error, "Invalid ambient component/application identity");
    if (!present) {
        row->count = row->sprite_count = 0;
        row->state = (AbAmbientAnimationState){0};
    }
    if (row->count == row->capacity)
        return ab_fail(error, "Ambient application exceeds retained variant capacity");
    size_t order = 0;
    if (owner->config.kind == AB_CLASSIC_AMBIENT_PARTICLES) {
        size_t count;
        if (!ab_ambient_particle_data(source, &order, row->particles + row->count,
                                      row->capacity - row->count, &count, error))
            return false;
        row->count += count;
        return ab_identity_map_set(owner->objects, identity(owner, object), object + 1,
                                   true, error);
    }
    AbAmbientAnimationConfiguration decoded;
    if (!ab_ambient_animation_data(
            source, &order,
            (AbAmbientAnimationStorage){
                row->variants + row->count, row->capacity - row->count,
                row->sprites ? row->sprites + row->sprite_count : NULL,
                row->sprite_capacity - row->sprite_count},
            &decoded, error))
        return false;
    row->count += decoded.count;
    row->sprite_count += decoded.sprite_count;
    return ab_identity_map_set(owner->objects, identity(owner, object), object + 1,
                               true, error);
}

typedef struct {
    AbClassicAmbient *owner;
    size_t object;
} SpriteHost;

static bool set_sprite(void *context, AbSpan sprite, bool present, AbError *error) {
    SpriteHost *host = context;
    if (!present)
        return ab_fail(error, "Ambient nil sprite requires its native setSprite route");
    return ab_classic_visuals_set_sprite(host->owner->config.visuals, host->object,
                                         sprite, error);
}

static bool update_particles(AbClassicAmbient *owner, size_t object, AbDataValue name,
                             float seconds, AbError *error) {
    size_t target = 0;
    while (target < owner->level.bodies->count) {
        AbSpan candidate = owner->level.bodies->objects[target].name;
        if (candidate.size == name.string.size &&
            (!candidate.size ||
             !memcmp(candidate.data, name.string.data, candidate.size)))
            break;
        ++target;
    }
    if (target == owner->level.bodies->count)
        return ab_fail(error, "Ambient particle angle has no named native object");
    size_t body = owner->level.bodies->objects[target].physics.body;
    if (!ab_world_has_body(owner->level.world, body))
        return ab_fail(error, "Ambient particle angle body is absent");
    const AbDataTable *physical = &owner->level.properties->objects[object].physical;
    AbDataValue x = field(physical, "x"), y = field(physical, "y");
    if (x.kind != AB_DATA_NUMBER || y.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Ambient particle object has no script position");
    AbVec2 position;
    memcpy(&position.x, &x.bits, sizeof(position.x));
    memcpy(&position.y, &y.bits, sizeof(position.y));
    AmbientObject *row = owner->rows + object;
    return ab_ambient_particles_update(
        row->particles, row->count, seconds,
        owner->level.world->storage.bodies[body].motion.mass.angle, &position,
        owner->config.random,
        (AbAmbientParticleHost){.context = owner->config.particles,
                                .emit = ab_classic_particles_ambient},
        error);
}

static bool update_object(AbClassicAmbient *owner, size_t object, float seconds,
                          AbError *error) {
    AbDataTable *physical = &owner->level.properties->objects[object].physical;
    AbDataValue name = field(physical, "name");
    if (name.kind != AB_DATA_STRING || (name.string.size && !name.string.data))
        return ab_fail(error, "Ambient object has no current physical name");
    const AbDataValue *world = ab_data_get(&owner->level.properties->world, name);
    if (!world || !truthy(*world))
        return ab_identity_map_set(owner->objects, (uintptr_t)physical, 0, false,
                                   error);
    if (owner->config.kind == AB_CLASSIC_AMBIENT_PARTICLES)
        return update_particles(owner, object, name, seconds, error);
    AbDataValue sprite = field(physical, "sprite");
    if (sprite.kind != AB_DATA_STRING && sprite.kind != AB_DATA_NIL)
        return ab_fail(error, "Unsupported ambient current sprite");
    AmbientObject *row = owner->rows + object;
    if (!row->state.current_present &&
        !ab_ambient_animation_start(&row->state, row->variants, row->count,
                                    sprite.string, sprite.kind == AB_DATA_STRING,
                                    owner->config.random, error))
        return false;
    SpriteHost host = {owner, object};
    AbAmbientAnimationResult result;
    if (!ab_ambient_animation_advance(&row->state, row->variants, row->count, seconds,
                                      sprite.string, sprite.kind == AB_DATA_STRING,
                                      (AbAmbientAnimationHost){&host, set_sprite},
                                      &result, error))
        return false;
    if (result == AB_AMBIENT_ANIMATION_FINISHED)
        row->state.current_present = false;
    return true;
}

bool ab_classic_ambient_update(AbClassicAmbient *owner, float seconds, AbError *error) {
    if (!owner || owner->busy || owner->failed)
        return ab_fail(error, "Invalid or stopped ambient update");
    owner->busy = true;
    bool okay = true;
    size_t cursor = 0;
    AbIdentityMapEntry entry;
    while (okay) {
        okay = ab_identity_map_next(owner->objects, &cursor, &entry, error);
        if (!okay || !entry.present)
            break;
        size_t object = entry.value - 1;
        okay = object < owner->count && entry.key == identity(owner, object);
        if (!okay) {
            ab_fail(error, "Stale ambient object identity");
            break;
        }
        okay = update_object(owner, object, seconds, error) &&
               ab_identity_map_resume(owner->objects, entry.key, &cursor, error);
    }
    owner->busy = false;
    owner->failed = !okay;
    return okay;
}

bool ab_classic_ambient_dispose(AbClassicAmbient *owner, AbError *error) {
    if (!owner || owner->busy || owner->failed)
        return ab_fail(error, "Invalid or stopped ambient disposal");
    return ab_identity_map_reset(owner->objects, 0, error);
}

bool ab_classic_ambient_view(const AbClassicAmbient *owner, size_t object,
                             AbClassicAmbientView *output) {
    if (!owner || owner->busy || owner->failed || !output || object >= owner->count)
        return false;
    uintptr_t value;
    bool present;
    if (!ab_identity_map_get(owner->objects, identity(owner, object), &value, &present,
                             NULL))
        return false;
    const AmbientObject *row = owner->rows + object;
    *output = (AbClassicAmbientView){.state = &row->state,
                                     .variants = row->variants,
                                     .particles = row->particles,
                                     .count = row->count,
                                     .present = present};
    return true;
}
