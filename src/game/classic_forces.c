#include "ab/classic_forces.h"

#include "ab/object_force_data.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbSpan name;
    size_t source, rank;
    AbObjectForceData data;
    AbClassicForces *owner;
} ForceEntry;

struct AbClassicForces {
    AbClassicLevel *level;
    AbPhysicsWorld *world;
    AbGamePhysicsObject *objects;
    size_t *source_indices;
    ForceEntry *entries;
    ForceEntry **order;
    AbSpan *choices;
    size_t count, capacity, bytes;
    AbRandom48 *random;
    AbClassicForceHost host;
    void *context;
};

static int compare_names(const void *left, const void *right) {
    const ForceEntry *first = left, *second = right;
    size_t common =
        first->name.size < second->name.size ? first->name.size : second->name.size;
    int order = common ? memcmp(first->name.data, second->name.data, common) : 0;
    if (order) {
        return order;
    }
    return (first->name.size > second->name.size) -
           (first->name.size < second->name.size);
}

static bool reserve_bytes(size_t count, size_t element, size_t *remaining) {
    if (count > *remaining / element) {
        return false;
    }
    *remaining -= count * element;
    return true;
}

static bool resource_request(void *context, const AbObjectForceCall *call,
                             AbError *error) {
    ForceEntry *entry = context;
    AbClassicForces *owner = entry->owner;
    if (!owner->host) {
        return ab_fail(error, "Missing classic force resource host");
    }
    return owner->host(owner->context, entry->source, call, error);
}

static bool read_entries(AbClassicForces *forces, AbClassicLevelView level,
                         size_t choice_limit, size_t *choice_count, AbError *error) {
    size_t count = 0;
    for (size_t index = 0; index < forces->count; ++index) {
        const AbLevelObject *object = level.bodies->objects + index;
        ForceEntry *entry = forces->entries + index;
        if (!object->name.size || !object->name.data ||
            !ab_world_has_body(level.world, object->physics.body)) {
            return ab_fail(error, "Invalid classic force object at %zu", index);
        }
        entry->name = object->name;
        entry->source = index;
        entry->owner = forces;
        if (object->name.size == 6 && memcmp(object->name.data, "ground", 6) == 0) {
            entry->data.forces.gravity_enabled = object->physics.gravity_enabled;
            continue;
        }
        AbObjectDefinition definition;
        AbDataValue sprite = level.sprites_initialized
                                 ? level.initial_sprites[index].sprite
                                 : (AbDataValue){0};
        if (!ab_object_definition_open(level.catalog, object->definition_name,
                                       object->placement, &definition, error) ||
            !ab_object_force_data(&definition, object->initial_mass,
                                  object->physics.gravity_enabled, sprite, &entry->data,
                                  error)) {
            return false;
        }
        if (entry->data.sound.count > choice_limit - count) {
            return ab_fail(error, "Classic force sound choices exceed their limit");
        }
        count += entry->data.sound.count;
    }
    qsort(forces->entries, forces->count, sizeof(*forces->entries), compare_names);
    for (size_t index = 1; index < forces->count; ++index) {
        if (!compare_names(forces->entries + index - 1, forces->entries + index)) {
            return ab_fail(error, "Duplicate classic force object name");
        }
    }
    *choice_count = count;
    return true;
}

static bool bind_objects(AbClassicForces *forces, AbClassicLevelView level,
                         AbRandom48 *random, AbError *error) {
    size_t used = 0;
    for (size_t index = 0; index < forces->count; ++index) {
        ForceEntry *entry = forces->entries + index;
        AbObjectForceData *data = &entry->data;
        const AbLevelObject *source = level.bodies->objects + entry->source;
        if (data->sound_choices) {
            AbSpan *choices = forces->choices + used;
            if (!ab_object_force_choices(data->sound_choices, choices,
                                         data->sound.count, error)) {
                return false;
            }
            data->sound.choices = choices;
            used += data->sound.count;
        }
        forces->source_indices[index] = entry->source;
        entry->rank = index;
        forces->order[index] = entry;
        forces->objects[index] = (AbGamePhysicsObject){
            .body = source->physics.body,
            .prepare_forces = data->prepare_forces,
            .friction = data->friction,
            .forces = data->forces,
            .gravity_enabled = &source->physics.gravity_enabled,
            .interval = data->interval_present ? &data->interval : NULL,
            .sound = &data->sound,
            .random = random,
            .resource_host = resource_request,
            .resource_context = entry,
            .removed = level.removed_objects + entry->source,
        };
    }
    return true;
}

bool ab_classic_forces_open(AbClassicLevel *level, AbClassicForceLimits limits,
                            AbRandom48 *random, AbClassicForceHost host, void *context,
                            AbClassicForces **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing classic force output");
    }
    *output = NULL;
    AbClassicLevelView source = ab_classic_level_view(level);
    if (!source.world || source.world->failed || source.world->locked ||
        !source.bodies || !source.bodies->count || !source.bodies->objects ||
        source.bodies->count > source.bodies->capacity || !source.catalog ||
        (source.sprites_initialized && !source.initial_sprites) ||
        limits.storage_bytes < sizeof(AbClassicForces)) {
        return ab_fail(error, "Invalid classic force source or limits");
    }
    size_t remaining = limits.storage_bytes - sizeof(AbClassicForces);
    size_t count = source.bodies->count;
    size_t capacity = limits.object_capacity ? limits.object_capacity : count;
    if (capacity < count || capacity > source.bodies->capacity ||
        !reserve_bytes(capacity, sizeof(AbGamePhysicsObject), &remaining) ||
        !reserve_bytes(capacity, sizeof(ForceEntry), &remaining) ||
        !reserve_bytes(capacity, sizeof(ForceEntry *), &remaining) ||
        !reserve_bytes(capacity, sizeof(size_t), &remaining)) {
        return ab_fail(error, "Classic force storage exceeds its byte limit");
    }
    AbClassicForces *forces = calloc(1, sizeof(*forces));
    if (!forces) {
        return ab_fail(error, "Cannot allocate classic force owner");
    }
    forces->count = count;
    forces->capacity = capacity;
    forces->level = level;
    forces->random = random;
    forces->world = source.world;
    forces->host = host;
    forces->context = context;
    forces->objects = calloc(capacity, sizeof(*forces->objects));
    forces->entries = calloc(capacity, sizeof(*forces->entries));
    forces->order = calloc(capacity, sizeof(*forces->order));
    forces->source_indices = calloc(capacity, sizeof(*forces->source_indices));
    bool okay = false;
    size_t choices = 0;
    if (!forces->objects || !forces->entries || !forces->order ||
        !forces->source_indices) {
        ab_fail(error, "Cannot allocate classic force arrays");
        goto release_forces;
    }
    if (!read_entries(forces, source, limits.sound_choices, &choices, error)) {
        goto release_forces;
    }
    if (!reserve_bytes(choices, sizeof(AbSpan), &remaining)) {
        ab_fail(error, "Classic force sound storage exceeds its byte limit");
        goto release_forces;
    }
    if (choices) {
        forces->choices = calloc(choices, sizeof(*forces->choices));
        if (!forces->choices) {
            ab_fail(error, "Cannot allocate classic force sound choices");
            goto release_forces;
        }
    }
    if (!bind_objects(forces, source, random, error)) {
        goto release_forces;
    }
    forces->bytes = limits.storage_bytes - remaining;
    *output = forces;
    okay = true;

release_forces:
    if (!okay) {
        ab_classic_forces_close(forces);
    }
    return okay;
}

void ab_classic_forces_close(AbClassicForces *forces) {
    if (forces) {
        free(forces->choices);
        free(forces->source_indices);
        free(forces->entries);
        free(forces->order);
        free(forces->objects);
        free(forces);
    }
}

size_t ab_classic_forces_bytes(const AbClassicForces *forces) {
    return forces ? forces->bytes : 0;
}

AbClassicForceView ab_classic_forces_view(AbClassicForces *forces) {
    return forces ? (AbClassicForceView){forces->objects, forces->source_indices,
                                         forces->count}
                  : (AbClassicForceView){0};
}

static size_t insertion_rank(AbClassicForces *forces, AbSpan name) {
    ForceEntry wanted = {.name = name};
    size_t begin = 0, end = forces->count;
    while (begin < end) {
        size_t middle = begin + (end - begin) / 2;
        int order = compare_names(&wanted, forces->order[middle]);
        if (order <= 0) {
            end = middle;
        } else {
            begin = middle + 1;
        }
    }
    return begin;
}

static ForceEntry *find_entry(AbClassicForces *forces, AbSpan name) {
    size_t rank = insertion_rank(forces, name);
    ForceEntry wanted = {.name = name};
    return rank < forces->count && !compare_names(&wanted, forces->order[rank])
               ? forces->order[rank]
               : NULL;
}

bool ab_classic_forces_recreate_circle(AbClassicForces *forces, size_t object,
                                       AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(forces ? forces->level : NULL);
    if (!forces || !level.bodies || object >= level.bodies->count ||
        level.removed_objects[object] || level.world->failed || level.world->locked)
        return ab_fail(error, "Invalid replacement circle force binding");
    const AbLevelObject *source = level.bodies->objects + object;
    ForceEntry *entry = find_entry(forces, source->name);
    if (!entry || entry->source != object || source->placement ||
        source->physics.dimensions.radius < 0 ||
        !ab_world_has_body(level.world, source->physics.body))
        return ab_fail(error, "Replacement circle has no retained force row");
    entry->data = (AbObjectForceData){0};
    entry->data.forces.gravity_enabled = source->physics.gravity_enabled;
    entry->data.forces.object_mass = source->initial_mass;
    forces->objects[entry->rank] =
        (AbGamePhysicsObject){.body = source->physics.body,
                              .forces = entry->data.forces,
                              .gravity_enabled = &source->physics.gravity_enabled,
                              .sound = &entry->data.sound,
                              .random = forces->random,
                              .resource_host = resource_request,
                              .resource_context = entry,
                              .removed = level.removed_objects + object};
    return true;
}

static bool register_object(AbClassicForces *forces, size_t object, bool box,
                            AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(forces ? forces->level : NULL);
    if (!forces || object != forces->count || object >= forces->capacity ||
        !level.bodies || object >= level.bodies->count ||
        level.removed_objects[object] || level.world != forces->world ||
        level.world->failed || level.world->locked)
        return ab_fail(error, "Invalid new direct-circle force binding");
    const AbLevelObject *source = level.bodies->objects + object;
    bool unsupported = box ? source->physics.dimensions.radius != -1 ||
                                 source->physics.dimensions.width <= 0 ||
                                 source->physics.dimensions.height <= 0
                           : source->physics.dimensions.radius < 0;
    if (source->placement || unsupported || !source->name.size || !source->name.data ||
        !ab_world_has_body(level.world, source->physics.body) ||
        find_entry(forces, source->name))
        return ab_fail(error, "Unsupported direct-circle force identity");
    size_t rank = insertion_rank(forces, source->name);
    ForceEntry *entry = forces->entries + forces->count;
    *entry = (ForceEntry){
        .name = source->name, .source = object, .rank = rank, .owner = forces};
    entry->data.forces.gravity_enabled = source->physics.gravity_enabled;
    entry->data.forces.object_mass = source->initial_mass;
    if (box || source->defined_constructor) {
        AbObjectDefinition definition;
        if (!ab_object_definition_open(level.catalog, source->definition_name, NULL,
                                       &definition, error) ||
            !ab_object_force_data(
                &definition, source->initial_mass, source->physics.gravity_enabled,
                level.initial_sprites[object].sprite, &entry->data, error))
            return false;
        if (entry->data.sound_choices || entry->data.sound.count)
            return ab_fail(error, "New box requires reserved force sound choices");
    }
    size_t shifted = forces->count - rank;
    memmove(forces->objects + rank + 1, forces->objects + rank,
            shifted * sizeof(*forces->objects));
    memmove(forces->source_indices + rank + 1, forces->source_indices + rank,
            shifted * sizeof(*forces->source_indices));
    memmove(forces->order + rank + 1, forces->order + rank,
            shifted * sizeof(*forces->order));
    forces->objects[rank] = (AbGamePhysicsObject){
        .body = source->physics.body,
        .prepare_forces = entry->data.prepare_forces,
        .friction = entry->data.friction,
        .forces = entry->data.forces,
        .gravity_enabled = &source->physics.gravity_enabled,
        .interval = entry->data.interval_present ? &entry->data.interval : NULL,
        .sound = &entry->data.sound,
        .random = forces->random,
        .resource_host = resource_request,
        .resource_context = entry,
        .removed = level.removed_objects + object};
    forces->source_indices[rank] = object;
    forces->order[rank] = entry;
    ++forces->count;
    for (size_t index = rank + 1; index < forces->count; ++index)
        forces->order[index]->rank = index;
    return true;
}

bool ab_classic_forces_register_circle(AbClassicForces *forces, size_t object,
                                       AbError *error) {
    return register_object(forces, object, false, error);
}

bool ab_classic_forces_register_box(AbClassicForces *forces, size_t object,
                                    AbError *error) {
    return register_object(forces, object, true, error);
}

static bool queued_object_exists(void *context, AbSpan name, bool *present,
                                 AbError *error) {
    (void)error;
    AbClassicForces *forces = context;
    ForceEntry *entry = find_entry(forces, name);
    size_t index = entry ? entry->rank : 0;
    *present = entry &&
               (!forces->objects[index].removed || !*forces->objects[index].removed) &&
               ab_world_has_body(forces->world, forces->objects[index].body);
    return true;
}

static bool apply_queued_object_force(void *context, const AbQueuedForce *force,
                                      AbError *error) {
    AbClassicForces *forces = context;
    ForceEntry *entry = find_entry(forces, force->object);
    if (!entry) {
        return ab_fail(error, "Queued force object disappeared");
    }
    size_t index = entry->rank;
    size_t body = forces->objects[index].body;
    if ((forces->objects[index].removed && *forces->objects[index].removed) ||
        !ab_world_has_body(forces->world, body)) {
        return ab_fail(error, "Queued force body disappeared");
    }
    return ab_object_force_at(&forces->world->storage.bodies[body].motion, force->force,
                              force->point, error);
}

bool ab_classic_forces_apply_queue(AbClassicForces *forces, const AbForceQueue *queue,
                                   AbError *error) {
    if (!forces || !forces->world || forces->world->failed || forces->world->locked) {
        return ab_fail(error, "Invalid classic queued force world");
    }
    const AbForceQueueHost host = {.context = forces,
                                   .exists = queued_object_exists,
                                   .apply = apply_queued_object_force};
    return ab_force_queue_apply(queue, host, error);
}
