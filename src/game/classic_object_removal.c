#include "ab/classic_object_removal.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbGameObjectCore core;
    size_t references;
} RemovalObject;

struct AbClassicObjectRemoval {
    AbClassicObjectRemovalConfig config;
    AbClassicLevelView level;
    RemovalObject *objects;
    AbGameObjectCore **birds;
    AbGameObjectCoreRegistry registry;
    AbGameObjectCoreHost host;
    AbGameObjectRemoval removal;
    size_t bytes, count, capacity;
    bool calling, invalid;
};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)});
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static RemovalObject *renderable(AbClassicObjectRemoval *owner, uintptr_t handle) {
    for (size_t index = 0; index < owner->count; ++index)
        if (handle == (uintptr_t)(owner->objects + index))
            return owner->objects + index;
    return NULL;
}

static void retain(void *context, uintptr_t handle) {
    AbClassicObjectRemoval *owner = context;
    RemovalObject *object = renderable(owner, handle);
    if (!object || !object->references || object->references == SIZE_MAX)
        owner->invalid = true;
    else
        ++object->references;
}

static void release(void *context, uintptr_t handle) {
    AbClassicObjectRemoval *owner = context;
    RemovalObject *object = renderable(owner, handle);
    if (!object || !object->references)
        owner->invalid = true;
    else
        --object->references;
}

static bool detach(void *context, uintptr_t handle, AbError *error) {
    AbClassicObjectRemoval *owner = context;
    RemovalObject *object = renderable(owner, handle);
    if (!object || owner->invalid || object->references < 3)
        return ab_fail(error, "Invalid retained classic renderable");
    size_t index = (size_t)(object - owner->objects);
    bool found = false;
    if (!ab_classic_render_scene_remove_object(owner->config.scene, index, &found,
                                               error) ||
        !found)
        return ab_fail(error, "Attached classic renderer left its scene");
    release(owner, handle);
    return ab_classic_visuals_detach(owner->config.visuals, index, error);
}

static void forget(void *context, AbGameObjectCore *core) {
    AbClassicObjectRemoval *owner = context;
    for (size_t index = 0; index < owner->count; ++index) {
        if (&owner->objects[index].core == core) {
            owner->invalid |=
                !ab_classic_level_forget_object(owner->config.level, index, NULL);
            return;
        }
    }
    owner->invalid = true;
}

static bool close_camera(void *context, AbSpan name, AbError *error) {
    AbClassicObjectRemoval *owner = context;
    return owner->config.close_camera_animation
               ? owner->config.close_camera_animation(owner->config.context, name,
                                                      error)
               : ab_fail(error, "Missing removed classic camera animation service");
}

static bool prepare(AbClassicObjectRemoval *owner, AbError *error) {
    for (size_t index = 0; index < owner->count; ++index) {
        const AbLevelObject *source = owner->level.bodies->objects + index;
        if (owner->level.removed_objects[index] ||
            !ab_world_has_body(owner->level.world, source->physics.body))
            return ab_fail(error, "Classic removal requires intact initial objects");
        AbObjectDefinition definition;
        if (!ab_object_definition_open(owner->level.catalog, source->definition_name,
                                       NULL, &definition, error))
            return false;
        AbDataValue rolling = field(definition.material, "rollingSound");
        if (truthy(rolling) && (rolling.kind != AB_DATA_STRING ||
                                (rolling.string.size && !rolling.string.data)))
            return ab_fail(error, "Unsupported classic native rolling name");
        const AbObjectVisual *visual =
            ab_classic_visuals_object(owner->config.visuals, index);
        if (!visual)
            return ab_fail(error, "Missing classic removal visual slot");
        RemovalObject *object = owner->objects + index;
        object->core = (AbGameObjectCore){
            .name = source->name,
            .rolling_sound = truthy(rolling) ? rolling.string : (AbSpan){0},
            .body = source->physics.body,
            .collider = (AbGameColliderType)owner->level.frame->objects[index].collider,
            .renderable = visual->attached ? (uintptr_t)object : 0};
        object->references = visual->attached ? 2 : 0;
        if (object->core.collider == AB_COLLIDER_BIRD ||
            object->core.collider == AB_COLLIDER_EAGLE)
            owner->birds[owner->registry.bird_count++] = &object->core;
        if (!ab_string_map_set(owner->registry.objects, source->name,
                               (uintptr_t)&object->core, true, error))
            return false;
    }
    return true;
}

bool ab_classic_object_removal_open(AbClassicObjectRemovalConfig config,
                                    AbClassicObjectRemoval **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing classic object-removal output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    AbClassicVisualView visuals = ab_classic_visuals_view(config.visuals);
    size_t count = level.bodies ? level.bodies->count : 0;
    size_t capacity = config.object_capacity ? config.object_capacity : count;
    size_t row_bytes = sizeof(RemovalObject) + sizeof(AbGameObjectCore *);
    if (!count || !level.world || level.world->failed || !level.properties ||
        !level.joints || !config.rolling_levels || !config.teleporters ||
        visuals.level != config.level || visuals.count != count ||
        ab_classic_render_scene_level(config.scene) != config.level ||
        config.storage_bytes < sizeof(AbClassicObjectRemoval) || capacity < count ||
        capacity > level.bodies->capacity ||
        capacity > (config.storage_bytes - sizeof(AbClassicObjectRemoval)) / row_bytes)
        return ab_fail(error, "Invalid classic object-removal source or storage");
    AbClassicObjectRemoval *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate classic object removal");
    owner->config = config;
    owner->level = level;
    owner->count = count;
    owner->capacity = capacity;
    owner->bytes = sizeof(*owner) + capacity * row_bytes;
    owner->objects = calloc(capacity, sizeof(*owner->objects));
    owner->birds = calloc(capacity, sizeof(*owner->birds));
    size_t maximum = 1;
    while (maximum < capacity && maximum <= SIZE_MAX / 2)
        maximum *= 2;
    if (!owner->objects || !owner->birds || maximum < capacity ||
        !ab_string_map_open(count, maximum, config.storage_bytes - owner->bytes,
                            &owner->registry.objects, error))
        goto release_owner;
    owner->bytes += ab_string_map_bytes(owner->registry.objects);
    owner->registry.world = level.world;
    owner->registry.rolling_levels = config.rolling_levels;
    owner->registry.birds = owner->birds;
    owner->registry.bird_capacity = capacity;
    owner->host =
        (AbGameObjectCoreHost){owner, close_camera, retain, detach, release, forget};
    owner->removal =
        (AbGameObjectRemoval){.objects = &owner->registry,
                              .joints = level.joints,
                              .teleporters = config.teleporters,
                              .object_host = &owner->host,
                              .teleporter_context = config.context,
                              .destroy_teleporter = config.destroy_teleporter};
    if (!prepare(owner, error))
        goto release_owner;
    *output = owner;
    return true;

release_owner:
    if (error && !error->message[0])
        ab_fail(error, "Cannot prepare classic object-removal storage");
    ab_classic_object_removal_close(owner);
    return false;
}

void ab_classic_object_removal_close(AbClassicObjectRemoval *owner) {
    if (owner) {
        ab_string_map_close(owner->registry.objects);
        free(owner->birds);
        free(owner->objects);
        free(owner);
    }
}

size_t ab_classic_object_removal_bytes(const AbClassicObjectRemoval *owner) {
    return owner ? owner->bytes : 0;
}

AbGameObjectCoreRegistry *
ab_classic_object_removal_registry(AbClassicObjectRemoval *owner) {
    return owner ? &owner->registry : NULL;
}

bool ab_classic_object_removal_register_circle(AbClassicObjectRemoval *owner,
                                               size_t object, AbError *error) {
    if (!owner || owner->calling || owner->invalid || object != owner->count ||
        object >= owner->capacity || object >= owner->level.bodies->count ||
        object >= owner->level.frame->count || owner->level.world->failed ||
        owner->level.world->locked || owner->level.removed_objects[object] ||
        owner->registry.bird_count > owner->registry.bird_capacity)
        return ab_fail(error, "Invalid new native object-removal binding");
    const AbLevelObject *source = owner->level.bodies->objects + object;
    const AbObjectVisual *visual =
        ab_classic_visuals_object(owner->config.visuals, object);
    const AbGameFrameObject *frame = owner->level.frame->objects + object;
    if (source->placement || source->physics.dimensions.radius < 0 || !visual ||
        !source->name.data || !source->name.size ||
        frame->collider >= AB_GAME_COLLIDER_TYPES ||
        frame->body != source->physics.body ||
        !ab_world_has_body(owner->level.world, source->physics.body) ||
        object >= ab_classic_render_scene_view(owner->config.scene).object_count ||
        (source->physics.controllable &&
         owner->registry.bird_count == owner->registry.bird_capacity))
        return ab_fail(error, "Unsupported direct-circle removal identity");
    uintptr_t previous = 0;
    bool present = false;
    if (!ab_string_map_get(owner->registry.objects, source->name, &previous, &present,
                           error) ||
        present)
        return ab_fail(error, "New physical removal name already exists");
    RemovalObject *entry = owner->objects + object;
    *entry =
        (RemovalObject){.core = {.name = source->name,
                                 .body = source->physics.body,
                                 .collider = (AbGameColliderType)frame->collider,
                                 .renderable = visual->attached ? (uintptr_t)entry : 0},
                        .references = visual->attached ? 2u : 0u};
    if (!ab_string_map_set(owner->registry.objects, source->name,
                           (uintptr_t)&entry->core, true, error))
        return false;
    ++owner->count;
    if (source->physics.controllable)
        owner->birds[owner->registry.bird_count++] = &entry->core;
    return true;
}

bool ab_classic_object_removal_call(void *context, AbSpan name, AbError *error) {
    AbClassicObjectRemoval *owner = context;
    if (!owner || owner->calling || owner->invalid)
        return ab_fail(error, "Invalid or reentrant classic object removal");
    owner->calling = true;
    bool removed = false;
    bool okay = ab_game_object_remove(&owner->removal, name, &removed, error);
    owner->calling = false;
    return okay && (!owner->invalid ||
                    ab_fail(error, "Invalid classic object-removal lifetime"));
}
