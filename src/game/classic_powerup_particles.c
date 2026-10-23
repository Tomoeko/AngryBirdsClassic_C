#include "ab/classic_powerup_particles.h"
#include "ab/string_map.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { COPY_TABLES = 32, COPY_ENTRIES = 96 };

typedef struct {
    size_t object;
    AbSpan key;
    uint8_t name[256];
    float time, interval;
    AbPowerupParticlesKind kind;
    bool started;
} PowerupSpawner;

typedef struct {
    AbDataTable tables[COPY_TABLES];
    AbDataTableLayout layouts[COPY_TABLES];
    AbDataEntry entries[COPY_ENTRIES];
    size_t table_count, entry_count;
} ParticleCopy;

struct AbClassicPowerupParticles {
    AbClassicLevel *level;
    AbClassicBirds *birds;
    AbClassicParticles *particles;
    AbRandom48 *random;
    const bool *physics_enabled;
    PowerupSpawner *rows;
    ParticleCopy *graphs;
    AbLuaData copies;
    AbStringMap *map;
    size_t count, capacity, graph_count, graph_capacity;
    bool calling, failed, bound;
};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool number(const AbDataTable *table, const char *name, float *output,
                   AbError *error) {
    AbDataValue value = field(table, name);
    memcpy(output, &value.bits, sizeof(*output));
    return (value.kind == AB_DATA_NUMBER && isfinite(*output)) ||
           ab_fail(error, "Powerup particles require finite physical %s", name);
}

static bool random_unit(AbClassicPowerupParticles *owner, float *output,
                        AbError *error) {
    return ab_script_random(owner->random, 0, 0, 0, output) == AB_SCRIPT_RANDOM_OK ||
           ab_fail(error, "Powerup particle RNG failed");
}

static bool next_interval(AbClassicPowerupParticles *owner, PowerupSpawner *row,
                          AbError *error) {
    if (row->kind != AB_POWERUP_PARTICLES_BUBBLES) {
        row->interval = 0.025f;
        return true;
    }
    float interval;
    if (ab_script_random(owner->random, 2, 50, 80, &interval) != AB_SCRIPT_RANDOM_OK)
        return ab_fail(error, "Super Seeds bubble interval RNG failed");
    row->interval = interval / 100;
    return true;
}

bool ab_classic_powerup_particles_open(AbClassicLevel *level, AbClassicBirds *birds,
                                       AbClassicParticles *particles,
                                       AbRandom48 *random, const bool *physics_enabled,
                                       size_t bird_capacity, size_t storage_bytes,
                                       AbClassicPowerupParticles **output,
                                       AbError *error) {
    if (!output)
        return ab_fail(error, "Missing powerup particle output");
    *output = NULL;
    const size_t row_bytes = sizeof(ParticleCopy) + 3 * sizeof(PowerupSpawner);
    if (!ab_classic_level_view(level).properties || !birds || !particles || !random ||
        !physics_enabled || !bird_capacity ||
        storage_bytes < sizeof(AbClassicPowerupParticles) ||
        bird_capacity >
            (storage_bytes - sizeof(AbClassicPowerupParticles)) / row_bytes ||
        bird_capacity > SIZE_MAX / 3)
        return ab_fail(error, "Invalid powerup particle owners or storage");
    size_t capacity = bird_capacity * 3, nodes = 1;
    while (nodes < capacity) {
        if (nodes > SIZE_MAX / 2)
            return ab_fail(error, "Powerup particle map capacity overflow");
        nodes *= 2;
    }
    AbClassicPowerupParticles *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate powerup particles");
    *owner = (AbClassicPowerupParticles){.level = level,
                                         .birds = birds,
                                         .particles = particles,
                                         .random = random,
                                         .physics_enabled = physics_enabled,
                                         .capacity = capacity,
                                         .graph_capacity = bird_capacity};
    owner->rows = calloc(capacity, sizeof(*owner->rows));
    owner->graphs = calloc(bird_capacity, sizeof(*owner->graphs));
    if (!owner->rows || !owner->graphs ||
        !ab_string_map_open(0, nodes,
                            storage_bytes - sizeof(*owner) - bird_capacity * row_bytes,
                            &owner->map, error) ||
        !ab_classic_particles_bind_copies(particles, &owner->copies, error)) {
        ab_classic_powerup_particles_close(owner);
        if (error && !error->message[0])
            ab_fail(error, "Cannot allocate powerup particle rows");
        return false;
    }
    owner->bound = true;
    *output = owner;
    return true;
}

void ab_classic_powerup_particles_close(AbClassicPowerupParticles *owner) {
    if (!owner)
        return;
    if (owner->bound) {
        AbError ignored = {0};
        (void)ab_classic_particles_bind_copies(owner->particles, NULL, &ignored);
    }
    ab_string_map_close(owner->map);
    free(owner->graphs);
    free(owner->rows);
    free(owner);
}

static bool copy_table(AbClassicPowerupParticles *owner, ParticleCopy *graph,
                       const AbDataTable *source, AbDataTable **output, size_t depth,
                       AbError *error) {
    if (!source || source->count > source->capacity ||
        (source->count && !source->entries) || depth >= COPY_TABLES ||
        graph->table_count == COPY_TABLES ||
        source->count > COPY_ENTRIES - graph->entry_count ||
        source->count > COPY_ENTRIES)
        return ab_fail(error,
                       "Super Seeds particle copy exceeds bounded graph storage");
    AbClassicLevelView level = ab_classic_level_view(owner->level);
    const AbDataTableLayout *layout = ab_lua_table_layout(level.catalog_data, source);
    if (!layout || !layout->no_nil_assignments)
        return ab_fail(error, "Super Seeds particle copy requires its source layout");
    size_t order[COPY_ENTRIES];
    AbStringOrderNode nodes[COPY_ENTRIES];
    AbStringOrderWorkspace work = {order, COPY_ENTRIES, nodes, COPY_ENTRIES};
    if (layout->array_hint) {
        if (layout->hash_capacity)
            return ab_fail(error, "Unsupported mixed particle copy table");
        for (size_t index = 0; index < source->count; ++index) {
            float key = (float)(index + 1);
            uint32_t bits;
            memcpy(&bits, &key, sizeof(bits));
            size_t entry = 0;
            while (entry < source->count &&
                   (source->entries[entry].key.kind != AB_DATA_NUMBER ||
                    source->entries[entry].key.bits != bits))
                ++entry;
            if (entry == source->count)
                return ab_fail(error, "Unsupported sparse particle copy array");
            order[index] = entry;
        }
    } else if (!ab_data_string_order(source, layout->hash_capacity, &work, error)) {
        return false;
    }
    size_t slot = graph->table_count++;
    AbDataTable *table = graph->tables + slot;
    *table = (AbDataTable){graph->entries + graph->entry_count, source->count,
                           source->count, NULL};
    graph->entry_count += source->count;
    graph->layouts[slot] = (AbDataTableLayout){.table = table,
                                               .array_hint = layout->array_hint,
                                               .no_nil_assignments = true,
                                               .next = owner->copies.layouts};
    owner->copies.layouts = graph->layouts + slot;
    for (size_t index = 0; index < source->count; ++index) {
        table->entries[index] = source->entries[order[index]];
        if (table->entries[index].value.kind == AB_DATA_TABLE &&
            !copy_table(owner, graph, table->entries[index].value.table,
                        &table->entries[index].value.table, depth + 1, error))
            return false;
    }
    *output = table;
    return true;
}

static bool purple_smoke(AbDataTable *particles, const char *event, AbError *error) {
    AbDataValue group = field(particles, event);
    AbDataValue smoke = group.kind == AB_DATA_TABLE ? field(group.table, "smokeBuff")
                                                    : (AbDataValue){0};
    if (smoke.kind != AB_DATA_TABLE)
        return ab_fail(error, "Super Seeds particle copy lacks its smoke descriptor");
    AbObjectPropertyAccess access = {.physics_object = smoke};
    return ab_object_property_set(&access, span("definition"),
                                  text("smokeBuffBirdsPurple"), error);
}

static bool potion_overrides(AbClassicPowerupParticles *owner, size_t object,
                             AbError *error) {
    if (owner->graph_count == owner->graph_capacity)
        return ab_fail(error, "Super Seeds particle copy capacity exhausted");
    AbClassicLevelView level = ab_classic_level_view(owner->level);
    AbDataValue particles;
    AbDataTable *copy;
    ParticleCopy *graph = owner->graphs + owner->graph_count++;
    return ab_level_property_get(level.properties, object, span("particles"),
                                 &particles, error) &&
           (particles.kind == AB_DATA_TABLE ||
            ab_fail(error, "Super Seeds requires its bird particle descriptor")) &&
           copy_table(owner, graph, particles.table, &copy, 0, error) &&
           purple_smoke(copy, "collision", error) &&
           purple_smoke(copy, "removal", error) &&
           ab_level_property_set(level.properties, object, span("particles"),
                                 (AbDataValue){.kind = AB_DATA_TABLE, .table = copy},
                                 error);
}

bool ab_classic_powerup_particles_add(AbClassicPowerupParticles *owner, size_t object,
                                      AbPowerupParticlesKind kind, AbError *error) {
    if (!owner || owner->calling || owner->failed || owner->count == owner->capacity ||
        kind > AB_POWERUP_PARTICLES_BUBBLES)
        return ab_fail(error, "Invalid or exhausted powerup particle registry");
    AbClassicLevelView level = ab_classic_level_view(owner->level);
    if (object >= level.bodies->count || level.removed_objects[object])
        return ab_fail(error, "Powerup spawner has no live source bird");
    static const char *const prefixes[] = {"KingSlingTrailSpawner_",
                                           "PowerPotionTrailSpawner_",
                                           "PowerPotionParticleSpawner_"};
    PowerupSpawner *row = owner->rows + owner->count;
    AbSpan name = level.bodies->objects[object].name;
    size_t prefix = strlen(prefixes[kind]);
    if (name.size > sizeof(row->name) - prefix)
        return ab_fail(error, "Powerup spawner name exceeds storage");
    memcpy(row->name, prefixes[kind], prefix);
    memcpy(row->name + prefix, name.data, name.size);
    row->key = (AbSpan){row->name, prefix + name.size};
    row->object = object;
    row->kind = kind;
    if (!ab_string_map_set(owner->map, row->key, owner->count, true, error))
        return false;
    ++owner->count;
    return next_interval(owner, row, error) && (kind != AB_POWERUP_PARTICLES_POTION ||
                                                potion_overrides(owner, object, error));
}

static bool position(AbClassicPowerupParticles *owner, const PowerupSpawner *row,
                     const AbDataTable *bird, AbVec2 *output, AbError *error) {
    float x, y, radius, random_x, random_y;
    if (!number(bird, "radius", &radius, error) ||
        !random_unit(owner, &random_x, error) ||
        !random_unit(owner, &random_y, error) || !number(bird, "x", &x, error) ||
        !number(bird, "y", &y, error))
        return false;
    float offset_x = random_x * 2 - 1;
    float offset_y;
    if (row->kind == AB_POWERUP_PARTICLES_BUBBLES) {
        offset_y = -random_y;
        float length = (float)sqrt((double)(offset_x * offset_x + offset_y * offset_y));
        if (length == 0) {
            offset_x = 0;
            offset_y = -radius;
        } else {
            offset_x = (offset_x / length) * radius;
            offset_y = (offset_y / length) * radius;
        }
    } else {
        offset_x = offset_x * radius;
        offset_y = (random_y * 2 - 1) * radius;
    }
    *output = (AbVec2){(x + offset_x) * 20, (y + offset_y) * 20};
    return true;
}

static bool update(AbClassicPowerupParticles *owner, float seconds, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(owner->level);
    size_t cursor = 0;
    AbStringMapEntry entry;
    for (;;) {
        if (!ab_string_map_next(owner->map, &cursor, &entry, error))
            return false;
        if (!entry.present)
            return true;
        PowerupSpawner *row = owner->rows + entry.value;
        const AbDataTable *bird = &level.properties->objects[row->object].physical;
        bool remove = level.removed_objects[row->object];
        if (row->kind == AB_POWERUP_PARTICLES_BUBBLES) {
            AbDataValue key = {.kind = AB_DATA_STRING,
                               .string = level.bodies->objects[row->object].name};
            const AbDataValue *present =
                ab_data_get(ab_classic_birds_view(owner->birds).birds, key);
            remove |= !present || !truthy(*present) ||
                      !truthy(field(bird, "usedPowerPotion")) ||
                      truthy(field(bird, "shot"));
        } else {
            remove |= truthy(field(bird, "hasCollided")) ||
                      !truthy(field(bird, row->kind == AB_POWERUP_PARTICLES_KING
                                              ? "shotWithKingSling"
                                              : "shotWithPowerPotion"));
        }
        if (remove) {
            if (!ab_string_map_set(owner->map, entry.key, 0, false, error))
                return false;
            continue;
        }
        if (!*owner->physics_enabled)
            continue;
        row->time += seconds;
        if (row->time < row->interval &&
            (row->started || row->kind == AB_POWERUP_PARTICLES_BUBBLES))
            continue;
        row->time -= row->interval;
        if (!next_interval(owner, row, error))
            return false;
        static const char *const definitions[] = {"superSlingTrail", "powerPotionTrail",
                                                  "powerPotionBubbles"};
        AbParticleEmission emission = {.definition = span(definitions[row->kind]),
                                       .amount = 1};
        if (!position(owner, row, bird, &emission.position, error) ||
            !ab_particles_emit(ab_classic_particles_engine(owner->particles), &emission,
                               error))
            return false;
        row->started = true;
    }
}

bool ab_classic_powerup_particles_update(AbClassicPowerupParticles *owner,
                                         float seconds, AbError *error) {
    if (!owner || owner->calling || owner->failed || !isfinite(seconds))
        return ab_fail(error, "Invalid or reentrant powerup particle update");
    owner->calling = true;
    bool okay = update(owner, seconds, error);
    owner->calling = false;
    owner->failed = !okay;
    return okay;
}
