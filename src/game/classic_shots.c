#include "ab/classic_shots.h"
#include "classic_shots_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbShotBird bird;
    AbShotDefinition definition;
    AbDataValue flying_sprite;
    const AbDataTable *source;
} ShotObject;

struct AbClassicShots {
    AbClassicBirds *birds;
    AbClassicShotConfig config;
    AbClassicLevelView level;
    AbShotState owned_state;
    AbShotState *state;
    ShotObject *objects;
    char *strings;
    size_t count, capacity, string_size, string_used, bytes;
    bool launching;
};

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool copy_string(AbSpan value, char **cursor, size_t *remaining,
                        const char **output, AbError *error) {
    if (!value.data || value.size >= *remaining || memchr(value.data, 0, value.size))
        return ab_fail(error, "Invalid or oversized classic shot identifier");
    *output = *cursor;
    memcpy(*cursor, value.data, value.size);
    (*cursor)[value.size] = 0;
    *cursor += value.size + 1;
    *remaining -= value.size + 1;
    return true;
}

static bool definition_string(AbDataValue value, bool false_is_absent, char **cursor,
                              size_t *remaining, const char **output, AbError *error) {
    *output = NULL;
    if (value.kind == AB_DATA_NIL ||
        (false_is_absent && value.kind == AB_DATA_BOOLEAN && !value.bits))
        return true;
    if (value.kind != AB_DATA_STRING)
        return ab_fail(error, "Unsupported classic shot definition identifier");
    return copy_string(value.string, cursor, remaining, output, error);
}

static bool prepare_definition(ShotObject *object, const AbDataTable *source,
                               char **cursor, size_t *remaining, AbError *error) {
    object->source = source;
    object->flying_sprite = field(source, "spriteFlying");
    return definition_string(field(source, "onBirdShot"), true, cursor, remaining,
                             &object->definition.on_shot, error) &&
           definition_string(field(source, "spriteFlying"), true, cursor, remaining,
                             &object->definition.sprite_flying, error) &&
           definition_string(field(source, "launchSound"), false, cursor, remaining,
                             &object->definition.launch_sound, error);
}

static bool prepare_objects(AbClassicShots *shots, size_t remaining, AbError *error) {
    char *cursor = shots->strings;
    for (size_t index = 0; index < shots->count; ++index) {
        ShotObject *object = shots->objects + index;
        const AbLevelObject *source = shots->level.bodies->objects + index;
        if (!copy_string(source->name, &cursor, &remaining, &object->bird.name, error))
            return false;
        object->bird.position = shots->level.character_objects[index].jump.position;
        object->bird.mass = source->initial_mass;
        if (!source->physics.controllable)
            continue;
        AbObjectDefinition definition;
        if (!ab_object_definition_open(shots->level.catalog, source->definition_name,
                                       NULL, &definition, error))
            return false;
        /* ObjectCreation creates a GameObject even with no behavior groups.
         * Missing ownership must fail at dispatch, never suppress onLaunch. */
        object->bird.has_script = true;
        if (!prepare_definition(object, definition.source, &cursor, &remaining, error))
            return false;
    }
    shots->string_used = shots->string_size - remaining;
    return true;
}

bool ab_classic_shots_open(AbClassicShotConfig config, AbClassicShots **output,
                           AbError *error) {
    if (!output)
        return ab_fail(error, "Missing classic shot output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    if (!level.bodies || !level.properties || !level.character_objects ||
        ab_classic_visuals_view(config.visuals).level != config.level ||
        config.storage_bytes < sizeof(AbClassicShots))
        return ab_fail(error, "Invalid classic shot binding or byte limit");
    size_t remaining = config.storage_bytes - sizeof(AbClassicShots);
    size_t capacity =
        config.object_capacity ? config.object_capacity : level.bodies->count;
    if (capacity < level.bodies->count || capacity > level.bodies->capacity ||
        capacity > remaining / sizeof(ShotObject))
        return ab_fail(error, "Classic shot objects exceed byte limit");
    remaining -= capacity * sizeof(ShotObject);
    AbClassicShots *shots = calloc(1, sizeof(*shots));
    if (!shots)
        return ab_fail(error, "Cannot allocate classic shots");
    shots->config = config;
    shots->state = config.state ? config.state : &shots->owned_state;
    shots->level = level;
    shots->count = level.bodies->count;
    shots->capacity = capacity;
    shots->string_size = remaining;
    shots->bytes = config.storage_bytes;
    shots->objects = calloc(capacity, sizeof(*shots->objects));
    shots->strings = malloc(remaining ? remaining : 1);
    if ((!shots->objects && shots->count) || !shots->strings ||
        !prepare_objects(shots, remaining, error)) {
        if (!error || !error->message[0])
            ab_fail(error, "Cannot allocate classic shot storage");
        ab_classic_shots_close(shots);
        return false;
    }
    *output = shots;
    return true;
}

void ab_classic_shots_close(AbClassicShots *shots) {
    if (!shots)
        return;
    free(shots->strings);
    free(shots->objects);
    free(shots);
}

size_t ab_classic_shots_bytes(const AbClassicShots *shots) {
    return shots ? shots->bytes : 0;
}

AbShotState *ab_classic_shots_state(AbClassicShots *shots) {
    return shots ? shots->state : NULL;
}

AbClassicLevel *ab_classic_shots_level(const AbClassicShots *shots) {
    return shots ? shots->config.level : NULL;
}

AbClassicGameGlobals *ab_classic_shots_globals(const AbClassicShots *shots) {
    return shots ? shots->config.globals : NULL;
}

bool ab_classic_shots_refresh_globals(AbClassicShots *shots, AbError *error) {
    if (!shots)
        return ab_fail(error, "Missing classic shots global binding");
    return !shots->config.globals || ab_classic_game_globals_refresh_shot(
                                         shots->config.globals, shots->state, error);
}

AbShotBird *ab_classic_shots_bird(AbClassicShots *shots, size_t object) {
    return shots && object < shots->count ? &shots->objects[object].bird : NULL;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool register_object(AbClassicShots *shots, size_t object, bool box,
                            AbError *error) {
    if (!shots || shots->launching || object != shots->count ||
        object >= shots->capacity || object >= shots->level.bodies->count ||
        object >= shots->level.properties->count || shots->level.world->failed ||
        shots->level.world->locked || shots->level.removed_objects[object])
        return ab_fail(error, "Invalid new classic shot identity");
    const AbLevelObject *source = shots->level.bodies->objects + object;
    AbLevelPropertyObject *properties = shots->level.properties->objects + object;
    AbDataValue script = field(&properties->physical, "gameObjectScript");
    bool unsupported = box ? source->physics.dimensions.radius != -1 ||
                                 source->physics.dimensions.width <= 0 ||
                                 source->physics.dimensions.height <= 0
                           : source->physics.dimensions.radius < 0;
    if (source->placement || unsupported ||
        !ab_world_has_body(shots->level.world, source->physics.body) ||
        (truthy(script) &&
         (script.kind != AB_DATA_TABLE || script.table != &properties->script)))
        return ab_fail(error, "Unsupported direct-circle shot metadata");
    ShotObject *entry = shots->objects + object;
    *entry = (ShotObject){
        .bird = {.position = shots->level.character_objects[object].jump.position,
                 .mass = source->initial_mass,
                 .shot = truthy(field(&properties->physical, "shot")),
                 .has_collided = truthy(field(&properties->physical, "hasCollided")),
                 .override_drawing =
                     shots->level.character_objects[object].override_drawing,
                 .has_script = truthy(script)}};
    size_t remaining = shots->string_size - shots->string_used;
    char *cursor = shots->strings + shots->string_used;
    if (!copy_string(source->name, &cursor, &remaining, &entry->bird.name, error) ||
        !definition_string(field(&properties->physical, "sprite"), false, &cursor,
                           &remaining, &entry->bird.sprite, error))
        return false;
    AbDataValue definition_name = field(&properties->physical, "definition");
    if (source->physics.controllable && definition_name.kind != AB_DATA_NIL) {
        AbDataValue definition = {0};
        if (definition_name.kind != AB_DATA_STRING ||
            !ab_object_definition_lookup(&shots->level.properties->lookup, source->name,
                                         &definition, error) ||
            definition.kind != AB_DATA_TABLE)
            return ab_fail(error, "Unsupported new shot definition");
        if (!prepare_definition(entry, definition.table, &cursor, &remaining, error))
            return false;
    }
    shots->string_used = shots->string_size - remaining;
    ++shots->count;
    return true;
}

bool ab_classic_shots_register_circle(AbClassicShots *shots, size_t object,
                                      AbError *error) {
    return register_object(shots, object, false, error);
}

bool ab_classic_shots_register_box(AbClassicShots *shots, size_t object,
                                   AbError *error) {
    return register_object(shots, object, true, error);
}

static bool object_index(AbClassicShots *shots, const AbShotBird *bird, size_t *output,
                         AbError *error) {
    for (size_t index = 0; index < shots->count; ++index) {
        if (&shots->objects[index].bird == bird) {
            if (shots->level.removed_objects[index])
                return ab_fail(error, "Removed classic shot object");
            *output = index;
            return true;
        }
    }
    return ab_fail(error, "Foreign classic shot bird");
}

bool ab_classic_shots_set_position(AbClassicShots *shots, size_t object,
                                   AbVec2 position, AbError *error) {
    if (!shots || object >= shots->count)
        return ab_fail(error, "Invalid classic shot position object");
    if (!ab_classic_level_set_position(shots->config.level, object, position, error))
        return false;
    if (!shots->level.removed_objects[object])
        shots->objects[object].bird.position = position;
    return true;
}

bool ab_classic_shots_sync_positions(AbClassicShots *shots, AbError *error) {
    if (!shots || shots->launching)
        return ab_fail(error, "Invalid or reentrant classic shot synchronization");
    for (size_t index = 0; index < shots->count; ++index)
        if (!shots->level.removed_objects[index])
            shots->objects[index].bird.position =
                shots->level.character_objects[index].jump.position;
    return true;
}

static bool read_definition(AbClassicShots *shots, AbShotCall *call, AbError *error) {
    AbDataValue definition;
    if (!ab_object_definition_lookup(&shots->level.properties->lookup,
                                     span(call->bird->name), &definition, error))
        return false;
    for (size_t index = 0; index < shots->count; ++index) {
        if (definition.kind == AB_DATA_TABLE &&
            definition.table == shots->objects[index].source) {
            call->definition = shots->objects[index].definition;
            return true;
        }
    }
    return ab_fail(error, "Classic shot definition was not prepared");
}

static bool service_operation(void *context, AbShotCall *call, AbError *error) {
    AbClassicShots *shots = context;
    size_t index = 0;
    if (call->bird && !object_index(shots, call->bird, &index, error))
        return false;
    switch (call->operation) {
        case AB_SHOT_SET_POSITION:
            return ab_classic_shots_set_position(shots, index, call->vector, error);
        case AB_SHOT_SET_VELOCITY:
        case AB_SHOT_APPLY_IMPULSE: {
            size_t body = shots->level.bodies->objects[index].physics.body;
            if (!ab_world_has_body(shots->level.world, body))
                return ab_fail(error, "Missing classic shot body");
            AbBodyMotion *motion = &shots->level.world->storage.bodies[body].motion;
            bool okay = call->operation == AB_SHOT_SET_VELOCITY
                            ? ab_body_velocity_set(motion, call->vector)
                            : ab_body_impulse_apply(motion, call->vector, &call->point);
            return okay || ab_fail(error, "Unsupported classic shot body effect");
        }
        case AB_SHOT_READ_DEFINITION:
            return read_definition(shots, call, error);
        case AB_SHOT_SET_SPRITE: {
            /* The raw field borrows catalog bytes, so it survives this adapter's
             * teardown. The transition's C identifiers are owned copies. */
            AbDataValue value = {0};
            for (size_t item = 0; item < shots->count; ++item)
                if (call->text == shots->objects[item].definition.sprite_flying)
                    value = shots->objects[item].flying_sprite;
            if (value.kind != AB_DATA_STRING)
                return ab_fail(error, "Unprepared classic shot sprite");
            if (!ab_level_property_set(shots->level.properties, index, span("sprite"),
                                       value, error))
                return false;
            return ab_classic_visuals_set_sprite(shots->config.visuals, index,
                                                 span(call->text), error);
        }
        case AB_SHOT_SCRIPT_LAUNCH:
            if (!shots->config.behaviors)
                return ab_fail(error, "Missing classic shot behavior owner");
            return ab_classic_behaviors_launch(shots->config.behaviors, index,
                                               call->vector, error);
        default:
            return shots->config.service
                       ? shots->config.service(shots->config.context, call, error)
                       : ab_fail(error, "Missing classic shot service %u",
                                 (unsigned)call->operation);
    }
}

static bool service(void *context, AbShotCall *call, AbError *error) {
    AbClassicShots *shots = context;
    if (!ab_classic_shots_refresh_globals(shots, error))
        return false;
    bool okay = service_operation(context, call, error);
    AbError ignored = {0};
    bool refreshed = ab_classic_shots_refresh_globals(shots, okay ? error : &ignored);
    return okay && refreshed;
}

static bool publish(void *context, AbShotPublication publication,
                    const AbShotState *state, AbError *error) {
    AbClassicShots *shots = context;
    if (publication == AB_SHOT_PUBLISH_SCORE)
        return !shots->config.globals || ab_classic_game_globals_publish_shot(
                                             shots->config.globals, state, error);
    if (publication == AB_SHOT_PUBLISH_OTHER_BIRDS)
        return !shots->birds || ab_classic_birds_reset_other(shots->birds, error);
    size_t index = 0;
    AbShotBird *bird =
        publication == AB_SHOT_PUBLISH_FLYING ? state->flying : state->dragged;
    if (!object_index(shots, bird, &index, error))
        return false;
    AbLevelProperties *properties = shots->level.properties;
    switch (publication) {
        case AB_SHOT_PUBLISH_START_IMPULSE:
            return ab_level_property_set(properties, index, span("startImpulseX"),
                                         number(bird->start_impulse.x), error) &&
                   ab_level_property_set(properties, index, span("startImpulseY"),
                                         number(bird->start_impulse.y), error);
        case AB_SHOT_PUBLISH_FLYING:
            if (shots->config.flying_object)
                *shots->config.flying_object =
                    (AbDataValue){.kind = AB_DATA_TABLE,
                                  .table = &properties->objects[index].physical};
            return !shots->config.camera ||
                   ab_classic_camera_set_flying(shots->config.camera, index, error);
        case AB_SHOT_PUBLISH_FLAGS:
            if (shots->config.globals) {
                shots->config.globals->extra_tutorial_timer =
                    number(state->tutorial_timer);
                shots->config.globals->specialty_available = (AbDataValue){
                    .kind = AB_DATA_BOOLEAN, .bits = state->specialty_available};
            }
            shots->level.character_objects[index].shot = bird->shot;
            return ab_level_property_set(
                       properties, index, span("shot"),
                       (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = bird->shot},
                       error) &&
                   ab_level_property_set(properties, index, span("hasCollided"),
                                         (AbDataValue){.kind = AB_DATA_BOOLEAN,
                                                       .bits = bird->has_collided},
                                         error);
        case AB_SHOT_PUBLISH_VELOCITY:
            return ab_level_properties_publish_launch(properties, index,
                                                      bird->launch_velocity, error);
        case AB_SHOT_PUBLISH_OTHER_BIRDS:
        case AB_SHOT_PUBLISH_SCORE:
            break;
    }
    return ab_fail(error, "Invalid classic shot publication");
}

bool ab_classic_shots_launch(AbClassicShots *shots, AbVec2 *position, AbVec2 *impulse,
                             AbError *error) {
    if (!shots || shots->launching || !shots->level.world ||
        shots->level.world->failed || shots->level.world->locked)
        return ab_fail(error, "Invalid or reentrant classic shot transition");
    if (!ab_classic_shots_refresh_globals(shots, error))
        return false;
    shots->launching = true;
    bool okay = ab_bird_shot_publish(shots->state, position, impulse, service, publish,
                                     shots, error);
    shots->launching = false;
    return okay;
}

typedef struct {
    AbClassicShots *shots;
    AbSlingLaunchState *sling;
    AbSlingLaunchHost query;
    void *context;
} SlingBinding;

static bool sling_service(void *context, AbSlingLaunchCall *call, AbError *error) {
    SlingBinding *binding = context;
    AbClassicShots *shots = binding->shots;
    if (!ab_classic_shots_refresh_globals(shots, error))
        return false;
    bool okay;
    if (call->operation == AB_SLING_LAUNCH_WITH_IMPULSE) {
        okay = ab_bird_shot_publish(shots->state, call->position, call->impulse,
                                    service, publish, shots, error);
    } else if (call->operation == AB_SLING_LAUNCH_FORCE) {
        AbDataValue definition;
        okay = ab_object_definition_lookup(&shots->level.properties->lookup,
                                           span(call->name), &definition, error);
        if (okay && definition.kind != AB_DATA_TABLE)
            okay = ab_fail(error, "Missing classic sling definition table");
        if (okay) {
            AbDataValue force = field(definition.table, "slingshotLaunchForce");
            call->force_present = force.kind != AB_DATA_NIL &&
                                  (force.kind != AB_DATA_BOOLEAN || force.bits);
            if (call->force_present) {
                if (force.kind != AB_DATA_NUMBER)
                    okay = ab_fail(error, "Unsupported classic sling force type");
                else
                    memcpy(&call->number, &force.bits, sizeof(call->number));
            }
        }
    } else {
        okay = binding->query ? binding->query(binding->context, call, error)
                              : ab_fail(error, "Missing classic sling query %u",
                                        (unsigned)call->operation);
    }
    binding->sling->dragged = shots->state->dragged;
    AbError ignored = {0};
    bool refreshed = ab_classic_shots_refresh_globals(shots, okay ? error : &ignored);
    return okay && refreshed;
}

bool ab_classic_shots_launch_sling(AbClassicShots *shots, AbSlingLaunchState *sling,
                                   AbVec2 *direction, AbSlingLaunchHost query,
                                   void *context, AbError *error) {
    if (!shots || !sling || shots->launching || shots->level.world->failed ||
        shots->level.world->locked)
        return ab_fail(error, "Invalid or reentrant classic sling launch");
    if (!ab_classic_shots_refresh_globals(shots, error))
        return false;
    shots->launching = true;
    sling->dragged = shots->state->dragged;
    SlingBinding binding = {shots, sling, query, context};
    bool okay = ab_slingshot_launch(sling, direction, sling_service, &binding, error);
    sling->dragged = shots->state->dragged;
    shots->launching = false;
    return okay;
}

bool ab_classic_shots_bind_birds(AbClassicShots *shots, AbClassicBirds *birds,
                                 AbError *error) {
    if (!shots || (birds && shots->birds))
        return ab_fail(error, "Invalid or duplicate owned shot bird registry");
    shots->birds = birds;
    return true;
}
