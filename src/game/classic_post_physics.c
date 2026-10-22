#include "ab/classic_post_physics.h"

#include "ab/object_activity.h"
#include "ab/object_definition.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbSpan name;
    size_t source, rolling;
    uint32_t collider, shape;
    float radius;
} PostEntry;

typedef struct {
    AbSpan name;
    float volume;
    bool active;
} RollingEntry;

struct AbClassicPostPhysics {
    AbClassicLevel *level;
    AbClassicPostConfig config;
    AbClassicPostObject *objects;
    PostEntry *entries;
    RollingEntry *rolling;
    AbStringMap *rolling_levels;
    size_t *source_indices;
    size_t count, capacity, rolling_count, bytes;
    AbDataValue ignored_definitions;
    AbClassicActivityState activity;
};

static int name_order(AbSpan first, AbSpan second) {
    size_t length = first.size < second.size ? first.size : second.size;
    int order = length ? memcmp(first.data, second.data, length) : 0;
    return order ? order : (first.size > second.size) - (first.size < second.size);
}

static int object_order(const void *first, const void *second) {
    return name_order(((const PostEntry *)first)->name,
                      ((const PostEntry *)second)->name);
}

static int rolling_order(const void *first, const void *second) {
    return name_order(((const RollingEntry *)first)->name,
                      ((const RollingEntry *)second)->name);
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool initial_rolling(AbClassicLevelView level, const AbLevelObject *object,
                            AbSpan *output, AbError *error) {
    AbObjectDefinition definition;
    if (!ab_object_definition_open(level.catalog, object->definition_name, NULL,
                                   &definition, error)) {
        return false;
    }
    const AbDataValue *name =
        ab_data_get(definition.material,
                    (AbDataValue){.kind = AB_DATA_STRING,
                                  .string = {(const uint8_t *)"rollingSound", 12}});
    if (!name || !truthy(*name)) {
        *output = (AbSpan){0};
        return true;
    }
    if (name->kind != AB_DATA_STRING || (name->string.size && !name->string.data)) {
        return ab_fail(error, "Unsupported initial native rolling-sound name");
    }
    *output = name->string;
    return true;
}

static bool prepare_entries(AbClassicPostPhysics *post, AbClassicLevelView level,
                            AbError *error) {
    for (size_t index = 0; index < post->count; ++index) {
        const AbLevelObject *object = level.bodies->objects + index;
        if (!object->name.size || !object->name.data ||
            !ab_world_has_body(level.world, object->physics.body) ||
            level.frame->objects[index].body != object->physics.body ||
            level.frame->objects[index].collider == 5) {
            return ab_fail(error, "Unsupported classic post-physics object");
        }
        post->entries[index] =
            (PostEntry){.name = object->name,
                        .source = index,
                        .collider = level.frame->objects[index].collider,
                        .shape = object->physics.dimensions.radius < 0 ? 1u : 0u,
                        .radius = object->physics.dimensions.radius};
        if (!initial_rolling(level, object, &post->rolling[index].name, error)) {
            return false;
        }
    }
    qsort(post->entries, post->count, sizeof(*post->entries), object_order);
    qsort(post->rolling, post->count, sizeof(*post->rolling), rolling_order);
    for (size_t index = 0; index < post->count; ++index) {
        if (index && !object_order(post->entries + index - 1, post->entries + index)) {
            return ab_fail(error, "Duplicate classic post-physics object name");
        }
        post->source_indices[index] = post->entries[index].source;
        if (!post->rolling_count ||
            name_order(post->rolling[post->rolling_count - 1].name,
                       post->rolling[index].name)) {
            post->rolling[post->rolling_count++] = post->rolling[index];
        }
    }
    for (size_t index = 0; index < post->count; ++index) {
        PostEntry *entry = post->entries + index;
        AbSpan name = {0};
        if (!initial_rolling(level, level.bodies->objects + entry->source, &name,
                             error)) {
            return false;
        }
        size_t rolling = 0;
        while (rolling < post->rolling_count &&
               name_order(post->rolling[rolling].name, name)) {
            ++rolling;
        }
        if (rolling == post->rolling_count) {
            return ab_fail(error, "Missing prepared classic rolling key");
        }
        entry->rolling = rolling;
    }
    return true;
}

bool ab_classic_post_physics_open(AbClassicLevel *level, AbClassicPostConfig config,
                                  AbClassicPostPhysics **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing classic post-physics output");
    }
    *output = NULL;
    AbClassicLevelView view = ab_classic_level_view(level);
    AbClassicActivityBindings activity = config.activity;
    bool bound = activity.has_awake || activity.moving || activity.above_tolerance;
    if (bound && (!activity.has_awake || !activity.moving ||
                  !activity.above_tolerance || activity.has_awake == activity.moving ||
                  activity.has_awake == activity.above_tolerance ||
                  activity.moving == activity.above_tolerance))
        return ab_fail(error, "Activity publication requires three distinct globals");
    size_t count = view.bodies ? view.bodies->count : 0;
    size_t capacity = config.object_capacity ? config.object_capacity : count;
    size_t row_bytes = sizeof(AbClassicPostObject) + sizeof(PostEntry) +
                       sizeof(RollingEntry) + sizeof(size_t);
    if (!view.world || view.world->failed || view.world->locked || !view.bodies ||
        !count || !view.catalog || !view.frame || view.frame->count != count ||
        !view.motion_properties || !view.bodies->objects || !view.frame->objects ||
        config.storage_bytes < sizeof(AbClassicPostPhysics) || capacity < count ||
        capacity > view.bodies->capacity ||
        capacity > (config.storage_bytes - sizeof(AbClassicPostPhysics)) / row_bytes) {
        return ab_fail(error, "Invalid classic post-physics source or storage limit");
    }
    AbClassicPostPhysics *post = calloc(1, sizeof(*post));
    if (!post) {
        return ab_fail(error, "Cannot allocate classic post-physics owner");
    }
    post->level = level;
    post->config = config;
    post->count = count;
    post->capacity = capacity;
    post->bytes = sizeof(*post) + capacity * row_bytes;
    if (capacity) {
        post->objects = calloc(capacity, sizeof(*post->objects));
        post->entries = calloc(capacity, sizeof(*post->entries));
        post->rolling = calloc(capacity, sizeof(*post->rolling));
        post->source_indices = calloc(capacity, sizeof(*post->source_indices));
        if (!post->objects || !post->entries || !post->rolling ||
            !post->source_indices) {
            ab_fail(error, "Cannot allocate classic post-physics rows");
            goto release_post;
        }
    }
    if (!prepare_entries(post, view, error)) {
        goto release_post;
    }
    size_t maximum = 1;
    size_t rolling_limit = capacity > count ? capacity : post->rolling_count;
    while (maximum < rolling_limit) {
        if (maximum > SIZE_MAX / 2) {
            ab_fail(error, "Classic rolling map capacity overflows");
            goto release_post;
        }
        maximum *= 2;
    }
    if (!ab_string_map_open(0, maximum, config.storage_bytes - post->bytes,
                            &post->rolling_levels, error))
        goto release_post;
    post->bytes += ab_string_map_bytes(post->rolling_levels);
    *output = post;
    return true;

release_post:
    ab_classic_post_physics_close(post);
    return false;
}

void ab_classic_post_physics_close(AbClassicPostPhysics *post) {
    if (post) {
        ab_string_map_close(post->rolling_levels);
        free(post->source_indices);
        free(post->rolling);
        free(post->entries);
        free(post->objects);
        free(post);
    }
}

size_t ab_classic_post_physics_bytes(const AbClassicPostPhysics *post) {
    return post ? post->bytes : 0;
}

AbClassicPostView ab_classic_post_physics_view(AbClassicPostPhysics *post) {
    return post ? (AbClassicPostView){post->objects,        post->count,
                                      post->source_indices, &post->ignored_definitions,
                                      &post->activity,      post->rolling_levels}
                : (AbClassicPostView){0};
}

bool ab_classic_post_physics_register_circle(AbClassicPostPhysics *post, size_t object,
                                             AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(post ? post->level : NULL);
    if (!post || object != post->count || object >= post->capacity || !level.bodies ||
        object >= level.bodies->count || object >= level.frame->count ||
        level.world->failed || level.world->locked || level.removed_objects[object])
        return ab_fail(error, "Invalid new direct-circle post-physics binding");
    const AbLevelObject *source = level.bodies->objects + object;
    const AbGameFrameObject *frame = level.frame->objects + object;
    if (source->placement || source->physics.dimensions.radius < 0 ||
        !source->name.data || !source->name.size || frame->collider == 5 ||
        frame->body != source->physics.body ||
        !ab_world_has_body(level.world, source->physics.body))
        return ab_fail(error, "Unsupported direct-circle post-physics identity");
    size_t rank = 0;
    while (rank < post->count && name_order(post->entries[rank].name, source->name) < 0)
        ++rank;
    if (rank < post->count && !name_order(post->entries[rank].name, source->name))
        return ab_fail(error, "Duplicate direct-circle post-physics name");
    if (!post->rolling_count || post->rolling[0].name.size) {
        if (post->rolling_count == post->capacity)
            return ab_fail(error, "Direct-circle rolling name storage exhausted");
        memmove(post->rolling + 1, post->rolling,
                post->rolling_count * sizeof(*post->rolling));
        post->rolling[0] = (RollingEntry){0};
        ++post->rolling_count;
        for (size_t index = 0; index < post->count; ++index)
            ++post->entries[index].rolling;
    }
    size_t shifted = post->count - rank;
    memmove(post->entries + rank + 1, post->entries + rank,
            shifted * sizeof(*post->entries));
    memmove(post->source_indices + rank + 1, post->source_indices + rank,
            shifted * sizeof(*post->source_indices));
    post->entries[rank] = (PostEntry){.name = source->name,
                                      .source = object,
                                      .rolling = 0,
                                      .collider = frame->collider,
                                      .radius = source->physics.dimensions.radius};
    post->source_indices[rank] = object;
    ++post->count;
    return true;
}

static bool update_object(AbClassicPostPhysics *post, AbClassicLevelView level,
                          const PostEntry *entry, AbClassicActivityState *state,
                          AbError *error) {
    size_t index = entry->source;
    const AbClassicPostObject *object = post->objects + index;
    const AbBodyMotion *body =
        &level.world->storage.bodies[level.frame->objects[index].body].motion;
    AbVec2 velocity = body->mass.linear_velocity;
    AbObjectPropertyPlan properties;
    if (!ab_classic_level_property_plan(post->level, index, post->config.minimum_x,
                                        post->config.maximum_x, &properties, error) ||
        (properties.publish && !ab_classic_level_publish_properties(
                                   post->level, index, &properties.values, error))) {
        return false;
    }
    RollingEntry *rolling = post->rolling + entry->rolling;
    AbObjectActivityInput input = {
        .velocity = velocity,
        .angular_velocity = body->mass.angular_velocity,
        .mass = body->mass.mass,
        .rolling_coefficient = entry->radius,
        .previous_rolling_volume = rolling->volume,
        .collider = entry->collider,
        .shape = entry->shape,
        .awake = body->awake,
        .previous_awake = level.motion_properties[index].previous_awake,
        .pink_timer_present = object->pink_timer.kind != AB_DATA_NIL,
        .bomb_timer_present = object->bomb_timer.kind != AB_DATA_NIL,
        .shoot_delay_present = object->shoot_delay.kind != AB_DATA_NIL,
        .bubble_is_boolean = object->in_bubble.kind == AB_DATA_BOOLEAN,
        .bubble = object->in_bubble.bits != 0};
    AbObjectActivityPlan activity;
    if (!ab_object_activity_plan(&input, &activity, error)) {
        return false;
    }
    if (activity.moving && post->ignored_definitions.kind == AB_DATA_TABLE) {
        const AbDataValue *definition =
            ab_data_get(&level.properties->objects[index].physical,
                        (AbDataValue){.kind = AB_DATA_STRING,
                                      .string = {(const uint8_t *)"definition", 10}});
        if (!definition || definition->kind != AB_DATA_STRING)
            return ab_fail(error, "Unsupported post-physics definition coercion");
        const AbDataValue *ignored =
            ab_data_get(post->ignored_definitions.table, *definition);
        if (ignored && ignored->kind == AB_DATA_BOOLEAN) {
            activity.moving = false;
        }
    }
    state->has_awake |= activity.has_awake;
    state->moving |= activity.moving;
    state->above_tolerance |= activity.above_tolerance;
    if (activity.rolling_request) {
        rolling->volume = activity.rolling_volume;
        rolling->active = true;
        uint32_t word;
        memcpy(&word, &rolling->volume, sizeof(word));
        if (!ab_string_map_set(post->rolling_levels, rolling->name, word, true, error))
            return false;
    }
    return ab_classic_level_finish_object(post->level, index, error);
}

static bool audio_call(AbClassicPostPhysics *post, AbRollingAudioOperation operation,
                       const RollingEntry *entry, bool *playing, AbError *error) {
    if (!post->config.audio) {
        return ab_fail(error, "Missing classic rolling-audio resource host");
    }
    AbRollingAudioCall call = {operation, entry->name,
                               operation == AB_ROLLING_AUDIO_PLAY ||
                                       operation == AB_ROLLING_AUDIO_GAIN
                                   ? entry->volume
                                   : 0,
                               false};
    if (!post->config.audio(post->config.context, &call, error)) {
        return false;
    }
    if (playing) {
        *playing = call.playing;
    }
    return true;
}

static bool dispatch_rolling(AbClassicPostPhysics *post, AbError *error) {
    for (size_t index = 0; index < post->rolling_count; ++index) {
        const RollingEntry *entry = post->rolling + index;
        if (!entry->active || !entry->name.size) {
            continue;
        }
        if (entry->volume <= 0) {
            if (!audio_call(post, AB_ROLLING_AUDIO_STOP, entry, NULL, error)) {
                return false;
            }
        } else {
            bool playing = false;
            if (!audio_call(post, AB_ROLLING_AUDIO_QUERY, entry, &playing, error) ||
                !audio_call(post,
                            playing ? AB_ROLLING_AUDIO_GAIN : AB_ROLLING_AUDIO_PLAY,
                            entry, NULL, error)) {
                return false;
            }
        }
    }
    return true;
}

bool ab_classic_post_physics_update(AbClassicPostPhysics *post, bool physics_enabled,
                                    AbError *error) {
    if (!post) {
        return ab_fail(error, "Missing classic post-physics owner");
    }
    if (!physics_enabled) {
        return true;
    }
    AbClassicLevelView level = ab_classic_level_view(post->level);
    if (level.world->failed || level.world->locked ||
        level.bodies->count != post->count || level.frame->count != post->count) {
        return ab_fail(error, "Classic post-physics membership or world changed");
    }
    for (size_t index = 0; index < post->count; ++index) {
        if (level.removed_objects[index]) {
            continue;
        }
        size_t body = level.bodies->objects[index].physics.body;
        if (level.frame->objects[index].body != body ||
            !ab_world_has_body(level.world, body)) {
            return ab_fail(error, "Missing active classic post-physics body");
        }
    }
    for (size_t index = 0; index < post->rolling_count; ++index) {
        RollingEntry *entry = post->rolling + index;
        uintptr_t word = 0;
        bool present = false;
        if (!ab_string_map_get(post->rolling_levels, entry->name, &word, &present,
                               error) ||
            word > UINT32_MAX)
            return ab_fail(error, "Invalid shared classic rolling accumulator");
        uint32_t bits = (uint32_t)word;
        memcpy(&entry->volume, &bits, sizeof(bits));
        entry->active = present;
    }
    AbClassicActivityState state = {.initialized = true};
    for (size_t index = 0; index < post->count; ++index) {
        if (level.removed_objects[post->entries[index].source]) {
            continue;
        }
        if (!update_object(post, level, post->entries + index, &state, error)) {
            return false;
        }
    }
    post->activity = state;
    AbClassicActivityBindings bindings = post->config.activity;
    if (bindings.has_awake) {
        *bindings.has_awake =
            (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = state.has_awake};
        *bindings.moving = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = state.moving};
        *bindings.above_tolerance =
            (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = state.above_tolerance};
    }
    if (!dispatch_rolling(post, error)) {
        return false;
    }
    return ab_string_map_reset(post->rolling_levels, 0, error);
}
