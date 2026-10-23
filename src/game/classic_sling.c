#include "ab/classic_sling.h"
#include "ab/rotation.h"
#include "ab/slingshot.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataTable table;
    AbDataEntry entries[2];
} SlingVector;

typedef struct {
    AbDataTable table;
    AbDataEntry entries[2];
} SlingReadyEvent;

struct AbClassicSling {
    AbClassicSlingConfig config;
    AbClassicLevelView level;
    AbShotState *shot;
    AbClassicSlingView state;
    SlingVector *bands, *impulses;
    SlingReadyEvent *ready_events;
    size_t capacity, band_index, impulse_count, ready_count, bytes;
    AbVec2 direction;
    bool band_exposed, busy;
};

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool healthy(const AbClassicSling *sling, AbError *error) {
    return sling && !sling->level.world->failed && !sling->level.world->locked
               ? true
               : ab_fail(error, "Invalid or locked classic sling world");
}

static void vector_initialize(SlingVector *vector) {
    vector->entries[0].key = text("x");
    vector->entries[1].key = text("y");
    vector->table = (AbDataTable){vector->entries, 2, 2, NULL};
}

static void vector_write(SlingVector *vector, AbVec2 value) {
    vector->entries[0].value = number(value.x);
    vector->entries[1].value = number(value.y);
}

static void publish_band(AbClassicSling *sling) {
    vector_write(sling->bands + sling->band_index, sling->state.band.rubber_band);
}

static bool bird_index(AbClassicSling *sling, const AbShotBird *bird, size_t *output,
                       AbError *error) {
    for (size_t index = 0; index < sling->level.bodies->count; ++index) {
        if (bird && bird == ab_classic_shots_bird(sling->config.shots, index) &&
            !sling->level.removed_objects[index]) {
            *output = index;
            return true;
        }
    }
    return ab_fail(error, "Missing or removed classic sling bird");
}

static bool named_bird(AbClassicSling *sling, const char *name, size_t *output,
                       AbError *error) {
    for (size_t index = 0; name && index < sling->level.bodies->count; ++index) {
        AbShotBird *bird = ab_classic_shots_bird(sling->config.shots, index);
        if (bird && bird->name && !strcmp(bird->name, name))
            return bird_index(sling, bird, output, error);
    }
    return ab_fail(error, "Unknown classic sling current bird");
}

static bool next_bird(AbClassicSling *sling, float wanted, size_t *output,
                      AbError *error) {
    *output = SIZE_MAX;
    for (size_t index = 0; index < sling->level.bird_count; ++index) {
        size_t object = sling->level.bird_order[index];
        if (object >= sling->level.bodies->count)
            return ab_fail(error, "Invalid classic sling queue object");
        if (sling->level.removed_objects[object])
            continue;
        /* Initial constructor fields also have owned cached storage. Read the
         * birds table independently of a rebound GameObject.physicsObject. */
        const AbDataTable *physical =
            &sling->level.properties->objects[object].physical;
        const AbDataValue *raw_control = ab_data_get(physical, text("controllable"));
        const AbDataValue *raw_start = ab_data_get(physical, text("startNumber"));
        bool controllable =
            raw_control ? truthy(*raw_control)
                        : sling->level.bodies->objects[object].physics.controllable;
        AbDataValue start =
            raw_start ? *raw_start : sling->level.bodies->objects[object].start_number;
        float start_number = 0;
        memcpy(&start_number, &start.bits, sizeof(start_number));
        if (controllable && start.kind == AB_DATA_NUMBER && start_number == wanted) {
            *output = object;
            return true;
        }
    }
    return true;
}

bool ab_classic_sling_next_bird(AbClassicSling *sling, float index, size_t *object,
                                AbError *error) {
    if (!healthy(sling, error) || !object || !isfinite(index))
        return ab_fail(error, "Invalid classic next-bird query");
    return next_bird(sling, index, object, error);
}

bool ab_classic_sling_open(AbClassicSlingConfig config, AbClassicSling **output,
                           AbError *error) {
    if (!output)
        return ab_fail(error, "Missing classic sling output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    AbShotState *shot = ab_classic_shots_state(config.shots);
    if (!level.bodies || !level.character_objects || !level.properties || !shot ||
        ab_classic_shots_level(config.shots) != config.level || !config.services ||
        !config.events || !config.audio || !isfinite(config.physics_scale) ||
        !isfinite(config.default_force) || !isfinite(config.current_bird_index) ||
        level.bird_count == SIZE_MAX || config.storage_bytes < sizeof(AbClassicSling))
        return ab_fail(error, "Invalid classic sling bindings or byte limit");
    size_t count = level.bird_count + 1;
    size_t stride = sizeof(SlingVector) * 2 + sizeof(SlingReadyEvent);
    if (count > (config.storage_bytes - sizeof(AbClassicSling)) / stride)
        return ab_fail(error, "Classic sling queue exceeds byte limit");
    AbClassicSling *sling = calloc(1, sizeof(*sling));
    if (!sling)
        return ab_fail(error, "Cannot allocate classic sling");
    sling->config = config;
    sling->level = level;
    sling->shot = shot;
    sling->capacity = count;
    sling->bytes = sizeof(*sling) + count * stride;
    sling->bands = calloc(count, sizeof(*sling->bands));
    sling->impulses = calloc(count, sizeof(*sling->impulses));
    sling->ready_events = calloc(count, sizeof(*sling->ready_events));
    if (!sling->bands || !sling->impulses || !sling->ready_events) {
        ab_classic_sling_close(sling);
        return ab_fail(error, "Cannot allocate classic sling retained tables");
    }
    sling->state = (AbClassicSlingView){
        .band = {.rest = config.start.level_start, .rubber_band = config.start.band},
        .transfer_object = SIZE_MAX,
        .current_bird_index = config.current_bird_index,
        .fill_in_next_bird = config.fill_in_next_bird,
        .allow_reset_to_bird_camera = config.allow_reset_to_bird_camera,
        .disabled_for_current_level = config.disabled_for_current_level};
    for (size_t index = 0; index < count; ++index) {
        vector_initialize(sling->bands + index);
        vector_initialize(sling->impulses + index);
        SlingReadyEvent *event = sling->ready_events + index;
        event->table = (AbDataTable){event->entries, 2, 2, NULL};
        event->entries[0] =
            (AbDataEntry){text("id"), text("EID_BIRD_PUT_TO_SLINGSHOT")};
        event->entries[1].key = text("name");
    }
    publish_band(sling);
    *output = sling;
    return true;
}

void ab_classic_sling_close(AbClassicSling *sling) {
    if (!sling)
        return;
    free(sling->ready_events);
    free(sling->impulses);
    free(sling->bands);
    free(sling);
}

size_t ab_classic_sling_bytes(const AbClassicSling *sling) {
    return sling ? sling->bytes : 0;
}

AbClassicSlingView ab_classic_sling_view(const AbClassicSling *sling) {
    return sling ? sling->state : (AbClassicSlingView){.transfer_object = SIZE_MAX};
}

AbClassicShots *ab_classic_sling_shots(const AbClassicSling *sling) {
    return sling ? sling->config.shots : NULL;
}

bool ab_classic_sling_power(AbClassicSling *sling, float *output, AbError *error) {
    if (!output || !healthy(sling, error))
        return ab_fail(error, "Invalid ordinary slingshot power query");
    *output = sling->state.king ? 1.0f + 0.3f : 1;
    return true;
}

bool ab_classic_sling_set_king(AbClassicSling *sling, bool scope, AbError *error) {
    if (!healthy(sling, error) || sling->busy)
        return ab_fail(error, "Invalid or busy KingSling selection");
    sling->state.king = true;
    sling->state.has_scope = scope;
    return true;
}

bool ab_classic_sling_set_scope(AbClassicSling *sling, bool enabled, AbError *error) {
    if (!healthy(sling, error) || sling->busy)
        return ab_fail(error, "Invalid or busy sling scope selection");
    sling->state.has_scope = enabled;
    return true;
}

bool ab_classic_sling_disable_for_level(AbClassicSling *sling, AbError *error) {
    if (!healthy(sling, error) || sling->busy)
        return ab_fail(error, "Invalid or busy sling disable binding");
    sling->state.disabled_for_current_level = true;
    return true;
}

bool ab_classic_sling_start_queue(AbClassicSling *sling, AbError *error) {
    if (!healthy(sling, error) || sling->busy)
        return ab_fail(error, "Invalid or busy sling start-queue binding");
    sling->state.current_bird_index = 0;
    sling->state.fill_in_next_bird = true;
    return true;
}

bool ab_classic_sling_is_visible(AbClassicSling *sling, bool *visible, AbError *error) {
    if (!healthy(sling, error) || !visible || !sling->config.camera)
        return ab_fail(error, "Invalid sling visibility binding");
    AbCameraViewport camera;
    AbVec2 base;
    if (!ab_camera_system_query_viewport(sling->config.camera, &camera) ||
        !ab_coordinate_point(NULL, AB_SPACE_PHYSICS, AB_SPACE_WORLD,
                             (AbVec2){sling->config.start.base.x, 0}, &base))
        return ab_fail(error, "Unpublished camera or invalid sling base");
    *visible = camera.top_left.x < base.x;
    return true;
}

bool ab_classic_sling_finish_input(AbClassicSling *sling, AbVec2 cursor,
                                   AbError *error) {
    if (!healthy(sling, error) || sling->busy || !isfinite(cursor.x) ||
        !isfinite(cursor.y))
        return ab_fail(error, "Invalid sling input housekeeping");
    sling->state.mouse.old_cursor = cursor;
    return true;
}

bool ab_classic_sling_shot(AbClassicSling *sling, AbShotCall *call, AbError *error) {
    if (!healthy(sling, error) || !call)
        return false;
    if (call->operation == AB_SHOT_NOTIFY_BIRD) {
        if (!call->launch_position || !call->launch_impulse ||
            sling->impulse_count == sling->capacity)
            return ab_fail(error, "Classic sling launch vector capacity exhausted");
        SlingVector *impulse = sling->impulses + sling->impulse_count++;
        vector_write(impulse, *call->launch_impulse);
        vector_write(sling->bands + sling->band_index, *call->launch_position);
        sling->band_exposed = true;
        if (!ab_shot_services_bind_vectors(sling->config.services,
                                           &sling->bands[sling->band_index].table,
                                           &impulse->table, error))
            return false;
    }
    bool okay = ab_shot_services_shot(sling->config.services, call, error);
    if (call->operation == AB_SHOT_NOTIFY_BIRD) {
        sling->state.band.rubber_band = *call->launch_position;
        publish_band(sling);
    }
    return okay;
}

void ab_classic_sling_set_launch_cue(AbClassicSling *sling) {
    if (sling) {
        sling->state.launch_cue = true;
        sling->state.band.scope_sound_eligible = true;
    }
}

bool ab_classic_sling_play_scope_sound(AbClassicSling *sling, AbError *error) {
    if (!healthy(sling, error) || sling->busy)
        return ab_fail(error, "Invalid or busy scope sound binding");
    if (!sling->state.band.scope_sound_eligible)
        return true;
    uint32_t handle;
    if (!ab_audio_bank_play_resource(sling->config.audio, span("laser_sight"), 1, false,
                                     0, &handle, error))
        return false;
    sling->state.band.scope_sound_eligible = false;
    return true;
}

static bool play_sound(AbClassicSling *sling, const char *name, bool resolve,
                       AbError *error) {
    if (resolve && !ab_audio_bank_name(sling->config.audio, name, &name, error))
        return false;
    uint32_t handle;
    return ab_audio_bank_play(
        sling->config.audio,
        (AbAudioPlay){.name = name ? text(name) : (AbDataValue){0},
                      .volume = number(1),
                      .loop = {.kind = AB_DATA_BOOLEAN}},
        &handle, error);
}

static bool selection_sound(AbClassicSling *sling, AbError *error) {
    size_t object = 0;
    AbDataValue definition;
    if (!bird_index(sling, sling->shot->dragged, &object, error) ||
        !ab_object_definition_lookup(&sling->level.properties->lookup,
                                     sling->level.bodies->objects[object].name,
                                     &definition, error))
        return false;
    if (definition.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing selected sling definition");
    const AbDataValue *value = ab_data_get(definition.table, text("selectionSound"));
    const char *name = NULL;
    if (value && value->kind != AB_DATA_NIL) {
        if (value->kind != AB_DATA_STRING ||
            memchr(value->string.data, 0, value->string.size))
            return ab_fail(error, "Unsupported sling selection sound type");
        name = (const char *)value->string.data;
    }
    return play_sound(sling, name, true, error);
}

bool ab_classic_sling_select(AbClassicSling *sling, AbError *error) {
    if (!healthy(sling, error) || sling->busy)
        return ab_fail(error, "Invalid or reentrant sling selection");
    size_t object = 0;
    if (!named_bird(sling, sling->shot->current_bird_name, &object, error))
        return false;
    if (!ab_camera_system_stop_movement(sling->config.camera))
        return ab_fail(error, "Missing sling-selection camera system");
    sling->shot->dragged = ab_classic_shots_bird(sling->config.shots, object);
    return true;
}

static bool stretch(AbClassicSling *sling, AbVec2 displacement, AbError *error) {
    AbSlingBandState *band = &sling->state.band;
    AbSlingDragFrame frame;
    AbSlingDragInput input = {.rest = band->rest,
                              .displacement = displacement,
                              .maximum_length = 2.2f + 3.2f,
                              .previous_distance = band->previous_distance,
                              .wobble = band->wobble,
                              .selection_played = sling->shot->selection_played};
    if (!ab_slingshot_drag(&input, &frame))
        return ab_fail(error, "Unsupported classic sling drag arithmetic");
    if (sling->band_exposed && sling->band_index + 1 == sling->capacity)
        return ab_fail(error, "Classic sling band identity capacity exhausted");
    band->angle = frame.angle;
    float squared = displacement.x * displacement.x + displacement.y * displacement.y;
    float length = (float)sqrt((double)squared);
    bool inside = length < input.maximum_length;
    if (!inside)
        band->wobble = 0;
    if (inside && length - band->previous_distance > 0.1f) {
        bool playing;
        if (!ab_audio_bank_is_playing(sling->config.audio, span("slingshot_stretched"),
                                      &playing, error) ||
            (!playing && !play_sound(sling, "slingshot_stretched", false, error)))
            return false;
    }
    if (inside)
        band->previous_distance = length;
    else if (!sling->shot->selection_played &&
             !play_sound(sling, "slingshot_stretched", false, error))
        return false;
    if (!sling->shot->selection_played) {
        if (!selection_sound(sling, error))
            return false;
        sling->shot->selection_played = true;
    }
    /* The local class vector becomes the system band after audio returns.
     * A failed sound preserves the previous band's identity and coordinates. */
    if (sling->band_exposed) {
        ++sling->band_index;
        sling->band_exposed = false;
    }
    band->rubber_band = frame.rubber_band;
    band->distance = frame.distance;
    band->previous_distance = frame.distance;
    publish_band(sling);
    return true;
}

bool ab_classic_sling_stretch(AbClassicSling *sling, AbVec2 displacement,
                              AbError *error) {
    if (!healthy(sling, error) || sling->busy)
        return ab_fail(error, "Invalid or reentrant sling stretch");
    sling->busy = true;
    bool okay = stretch(sling, displacement, error);
    sling->busy = false;
    return okay;
}

static bool control(void *context, AbSlingControlCall *call, AbError *error) {
    AbClassicSling *sling = context;
    if (call->operation == AB_SLING_CONTROL_QUERY_LASER) {
        publish_band(sling);
        return ab_audio_bank_is_playing(sling->config.audio, span("laser_sight"),
                                        &call->audio_playing, error);
    }
    size_t object = 0;
    return bird_index(sling, call->bird, &object, error) &&
           ab_classic_shots_set_position(sling->config.shots, object, call->position,
                                         error);
}

static bool query(void *context, AbSlingLaunchCall *call, AbError *error) {
    AbClassicSling *sling = context;
    AbSlingBandState *band = &sling->state.band;
    switch (call->operation) {
        case AB_SLING_LAUNCH_DIRECTION:
            sling->direction = (AbVec2){band->rest.x - band->rubber_band.x,
                                        band->rest.y - band->rubber_band.y};
            call->direction = &sling->direction;
            return true;
        case AB_SLING_LAUNCH_DISTANCE: {
            AbSlingStretch measured;
            if (!ab_slingshot_measure_stretch(band->rest, band->rubber_band,
                                              2.2f + 3.2f, &measured))
                return ab_fail(error, "Unsupported sling distance");
            call->number = measured.distance;
            return true;
        }
        case AB_SLING_LAUNCH_STRETCH:
            call->number = band->distance / (2.2f + 3.2f);
            call->number = call->number < 0 ? 0 : call->number > 1 ? 1 : call->number;
            return true;
        case AB_SLING_LAUNCH_POWER:
            return ab_classic_sling_power(sling, &call->number, error);
        case AB_SLING_LAUNCH_POSITION:
            call->position = &band->rubber_band;
            return true;
        default:
            return ab_shot_services_sling(sling->config.services, call, error);
    }
}

bool ab_classic_sling_launch_velocity(AbClassicSling *sling, AbVec2 *output,
                                      AbError *error) {
    if (!output || !healthy(sling, error) || sling->busy)
        return ab_fail(error, "Invalid or busy live sling velocity query");
    if (!sling->shot->current_bird_name) {
        *output = (AbVec2){0};
        return true;
    }
    size_t object;
    AbDataValue definition;
    if (!named_bird(sling, sling->shot->current_bird_name, &object, error) ||
        !ab_object_definition_lookup(&sling->level.properties->lookup,
                                     sling->level.bodies->objects[object].name,
                                     &definition, error))
        return false;
    if (definition.kind != AB_DATA_TABLE)
        return ab_fail(error, "Sling velocity requires its actual bird definition");
    const AbDataValue *raw_force =
        ab_data_get(definition.table, text("slingshotLaunchForce"));
    const AbDataValue *raw_mass =
        ab_data_get(&sling->level.properties->objects[object].physical, text("mass"));
    AbSlingLaunchInput input = {.rest = sling->state.band.rest,
                                .rubber_band = sling->state.band.rubber_band,
                                .launch_force = sling->config.default_force,
                                .physics_scale = sling->config.physics_scale,
                                .bird_present = true};
    if (!raw_mass || raw_mass->kind != AB_DATA_NUMBER ||
        (raw_force && truthy(*raw_force) && raw_force->kind != AB_DATA_NUMBER))
        return ab_fail(error, "Sling velocity requires numeric force and mass");
    memcpy(&input.bird_mass, &raw_mass->bits, sizeof(input.bird_mass));
    if (raw_force && truthy(*raw_force))
        memcpy(&input.launch_force, &raw_force->bits, sizeof(input.launch_force));
    AbSlingLaunchCall call = {.operation = AB_SLING_LAUNCH_STRETCH};
    if (!query(sling, &call, error))
        return false;
    input.stretch = call.number;
    if (!ab_classic_sling_power(sling, &input.slingshot_power, error))
        return false;
    return ab_slingshot_launch_velocity(&input, output) ||
           ab_fail(error, "Unsupported live sling launch velocity");
}

static bool release(AbClassicSling *sling, AbError *error) {
    AbSlingStretch measured;
    if (!ab_slingshot_measure_stretch(sling->state.band.rest,
                                      sling->state.band.rubber_band, 2.2f + 3.2f,
                                      &measured))
        return ab_fail(error, "Unsupported sling release distance");
    if (ab_slingshot_release_ready(measured.distance, 2)) {
        AbSlingLaunchState launch = {.default_force = sling->config.default_force,
                                     .physics_scale = sling->config.physics_scale};
        return ab_classic_shots_launch_sling(sling->config.shots, &launch, NULL, query,
                                             sling, error);
    }
    return ab_slingshot_cancel_drag(&sling->state.band, sling->shot, control, sling,
                                    error);
}

bool ab_classic_sling_release(AbClassicSling *sling, AbError *error) {
    if (!healthy(sling, error) || sling->busy)
        return ab_fail(error, "Invalid or reentrant sling release");
    sling->busy = true;
    bool okay = release(sling, error);
    sling->busy = false;
    return okay;
}

bool ab_classic_sling_cancel_drag(AbClassicSling *sling, AbError *error) {
    if (!healthy(sling, error) || sling->busy)
        return ab_fail(error, "Invalid or reentrant sling cancellation");
    sling->busy = true;
    bool okay = ab_slingshot_cancel_drag(&sling->state.band, sling->shot, control,
                                         sling, error);
    publish_band(sling);
    sling->busy = false;
    return okay;
}

bool ab_classic_sling_level_reset(AbClassicSling *sling, AbError *error) {
    if (!healthy(sling, error) || sling->busy)
        return ab_fail(error, "Invalid or active sling level reset");
    sling->busy = true;
    sling->state.disabled_for_current_level = false;
    sling->state.transfer =
        (AbSlingTransferInput){.phase_angle = 0.75f * 0x1.921fb6p+1f};
    sling->state.transfer_object = SIZE_MAX;
    sling->shot->bird_reload_delay = 1;
    sling->config.start = (AbSlingshotStartPose){0};
    sling->state.band.rest = (AbVec2){0};
    bool okay = ab_slingshot_reset_band(&sling->state.band, control, sling, error);
    if (okay) {
        sling->state.cooldown_trigger_time = 0;
        sling->shot->dragged = NULL;
        sling->shot->selection_played = false;
        sling->state.fill_in_next_bird = false;
        sling->shot->bird_fired = false;
        sling->shot->bird_ready = false;
        sling->shot->next_bird_timer = 0.5f;
    }
    publish_band(sling);
    sling->busy = false;
    return okay;
}

static bool start_number(AbClassicSling *sling, size_t object, const char *name,
                         float *output, bool optional, AbError *error) {
    AbDataValue value;
    if (!ab_level_property_get(sling->level.properties, object, span(name), &value,
                               error))
        return false;
    if (optional && !truthy(value)) {
        *output = 0;
        return true;
    }
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Sling start requires numeric physical fields");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite sling start physical field");
}

bool ab_classic_sling_position_start(AbClassicSling *sling, size_t object,
                                     float back_pivot_y, float back_height,
                                     AbError *error) {
    if (!healthy(sling, error) || sling->busy)
        return ab_fail(error, "Invalid or active sling start-position owner");
    if (object == SIZE_MAX)
        return true;
    if (object >= sling->level.bodies->count || sling->level.removed_objects[object])
        return ab_fail(error, "Missing canonical sling start object");
    sling->busy = true;
    AbSlingshotStartInput input = {.back_pivot_y = back_pivot_y,
                                   .back_height = back_height,
                                   .physics_scale = sling->config.physics_scale,
                                   .object_present = true,
                                   .radius_present = true};
    bool okay =
        start_number(sling, object, "radius", &input.radius, true, error) &&
        start_number(sling, object, "x", &input.object_position.x, false, error) &&
        start_number(sling, object, "y", &input.object_position.y, false, error);
    AbSlingshotStartPose pose;
    if (okay)
        okay = ab_slingshot_start_pose(&input, &pose, error);
    if (okay) {
        sling->config.start = pose;
        sling->state.band.rest = pose.level_start;
        sling->state.band.rubber_band = pose.band;
        publish_band(sling);
    }
    sling->busy = false;
    return okay;
}

AbSlingshotStartPose ab_classic_sling_start_pose(const AbClassicSling *sling) {
    return sling ? sling->config.start : (AbSlingshotStartPose){0};
}

bool ab_classic_sling_mouse(AbClassicSling *sling, AbSlingMouseInput input,
                            AbError *error) {
    if (!healthy(sling, error) || sling->busy)
        return ab_fail(error, "Invalid or reentrant sling mouse input");
    size_t object = 0;
    input.bird_present = sling->shot->current_bird_name != NULL;
    if (input.bird_present &&
        !named_bird(sling, sling->shot->current_bird_name, &object, error))
        return false;
    input.bird_position = input.bird_present
                              ? sling->level.character_objects[object].jump.position
                              : (AbVec2){0};
    input.rest = sling->state.band.rest;
    input.bird_ready = sling->shot->bird_ready;
    input.bird_dragged = sling->shot->dragged != NULL;
    input.sling_disabled = sling->state.disabled_for_current_level;
    input.shoot_range = 2.2f;
    AbSlingMousePlan plan;
    if (!ab_game_sling_mouse_plan(&sling->state.mouse, &input, &plan, error))
        return false;
    sling->busy = true;
    sling->state.mouse = plan.next;
    bool okay = true;
    for (size_t index = 0; okay && index < plan.count; ++index) {
        AbSlingMouseCall call = plan.calls[index];
        sling->state.mouse.drag_started = call.drag_started;
        sling->state.mouse.in_game_pressed = call.in_game_pressed;
        if (call.kind == AB_SLING_MOUSE_SELECT) {
            okay = ab_camera_system_stop_movement(sling->config.camera);
            if (okay)
                sling->shot->dragged =
                    ab_classic_shots_bird(sling->config.shots, object);
            else
                ab_fail(error, "Missing sling-selection camera system");
        } else if (call.kind == AB_SLING_MOUSE_STRETCH)
            okay = stretch(sling, call.vector, error);
        else if (call.kind == AB_SLING_MOUSE_RELEASE)
            okay = release(sling, error);
    }
    if (okay)
        sling->state.mouse = plan.next;
    sling->busy = false;
    return okay;
}

static bool movement_event(AbClassicSling *sling, size_t object, const char *event,
                           AbError *error) {
    const AbStringMap *groups =
        ab_classic_behaviors_groups(sling->config.behaviors, object);
    if (!groups)
        return ab_fail(error, "Missing sling-move behavior registry");
    return ab_game_object_delegate(groups, span(event), NULL, 0, error);
}

static bool hold(AbClassicSling *sling, AbError *error) {
    if (!sling->shot->bird_ready || !sling->shot->current_bird_name)
        return true;
    size_t object = 0;
    if (!named_bird(sling, sling->shot->current_bird_name, &object, error))
        return false;
    AbBodyMotion *body = &sling->level.world->storage
                              .bodies[sling->level.bodies->objects[object].physics.body]
                              .motion;
    if (!ab_classic_shots_set_position(sling->config.shots, object,
                                       sling->state.band.rubber_band, error) ||
        !ab_body_velocity_set(body, (AbVec2){0}) ||
        !ab_classic_level_set_rotation(sling->config.level, object, 0, error) ||
        !ab_body_angular_velocity_set(body, 0))
        return ab_fail(error, "Could not hold the current sling bird");
    if (sling->level.character_objects[object].has_hat)
        return ab_fail(error, "Sling-held hat motion is not recovered");
    return true;
}

static bool begin_transfer(AbClassicSling *sling, float next_index, AbError *error) {
    sling->state.transfer.time = 0;
    size_t object = SIZE_MAX;
    if (!next_bird(sling, next_index, &object, error))
        return false;
    sling->state.transfer_object = object;
    if (object == SIZE_MAX)
        return ab_fail(error, "Next bird not found: level birds must be numbered");
    AbBirdJumpState *jump = &sling->level.character_objects[object].jump;
    jump->timer = 10000;
    if (jump->jumping) {
        jump->position.y = jump->saved_y;
        jump->jumping = false;
        if (!ab_level_properties_publish_position(sling->level.properties, object,
                                                  jump->position, error))
            return false;
    }
    AbSlingTransferStart start = {.position = jump->position,
                                  .target = sling->state.band.rest,
                                  .rotation = jump->rotation};
    if (!ab_slingshot_transfer_begin(&start, &sling->state.transfer))
        return ab_fail(error, "Unsupported sling transfer initialization");
    return movement_event(sling, object, "onStartToSlingshotMove", error);
}

static bool complete_transfer(AbClassicSling *sling, AbError *error) {
    sling->state.transfer_object = SIZE_MAX;
    sling->state.transfer.time = 0;
    sling->state.fill_in_next_bird = false;
    sling->shot->bird_ready = true;
    sling->state.current_bird_index += 1;
    size_t object = SIZE_MAX;
    if (!next_bird(sling, sling->state.current_bird_index, &object, error))
        return false;
    sling->shot->current_bird_name =
        object == SIZE_MAX ? NULL
                           : ab_classic_shots_bird(sling->config.shots, object)->name;
    if (object == SIZE_MAX)
        return ab_fail(error, "Completed sling transfer has no current bird");
    AbVec2 sitting = {sling->state.band.rest.x - 0.1f, sling->state.band.rest.y - 0.1f};
    if (!ab_classic_shots_set_position(sling->config.shots, object, sitting, error))
        return false;
    sling->state.band.rubber_band = sitting;
    if (!ab_script_angle_from_components(-0.1f, 0.1f, &sling->state.band.angle))
        return ab_fail(error, "Cannot form sitting sling angle");
    publish_band(sling);
    float range = 0, distance = 0;
    if (!sling->config.camera_distances ||
        !sling->config.camera_distances(sling->config.context, &range, &distance,
                                        error))
        return ab_fail(error, "Missing or failed sling reload camera distances");
    float gain = 1 - distance / range;
    gain = gain < 0.25f ? 0.25f : gain;
    gain = 1 < gain ? 1 : gain;
    if (!isfinite(range) || !isfinite(distance) || range == 0 || !isfinite(gain))
        return ab_fail(error, "Unsupported sling reload camera geometry");
    if (!sling->config.reload_sound ||
        !sling->config.reload_sound(sling->config.context, gain, error))
        return ab_fail(error, "Missing or failed sling reload sound");
    size_t body = sling->level.bodies->objects[object].physics.body;
    if (!ab_world_set_body_type(sling->level.world, body, AB_BODY_DYNAMIC, error) ||
        !ab_classic_level_set_rotation(sling->config.level, object, 0, error))
        return false;
    if (sling->level.character_objects[object].has_hat)
        return ab_fail(error, "Sling reload hat activation is not recovered");
    if (!movement_event(sling, object, "onEndToSlingshotMove", error))
        return false;
    if (sling->ready_count == sling->capacity)
        return ab_fail(error, "Classic sling ready-event capacity exhausted");
    SlingReadyEvent *event = sling->ready_events + sling->ready_count++;
    event->entries[1].value = text(sling->shot->current_bird_name);
    return ab_event_manager_notify(sling->config.events, &event->table, error);
}

static bool reload(AbClassicSling *sling, float seconds, AbError *error) {
    AbSlingReloadState state = {.timer = sling->shot->next_bird_timer,
                                .current_bird_index = sling->state.current_bird_index,
                                .bird_fired = sling->shot->bird_fired,
                                .fill_in_next_bird = sling->state.fill_in_next_bird,
                                .allow_reset_to_bird_camera =
                                    sling->state.allow_reset_to_bird_camera};
    AbSlingReloadGate gate;
    if (!ab_slingshot_reload_gate(&state, seconds, false, &gate))
        return ab_fail(error, "Unsupported classic sling reload timer");
    if (gate.query_next_bird) {
        sling->shot->next_bird_timer = gate.next.timer;
        size_t object = SIZE_MAX;
        if (!next_bird(sling, gate.queried_bird_index, &object, error) ||
            !ab_slingshot_reload_gate(&state, seconds, object != SIZE_MAX, &gate))
            return false;
    }
    sling->shot->next_bird_timer = gate.next.timer;
    sling->shot->bird_fired = gate.next.bird_fired;
    sling->state.fill_in_next_bird = gate.next.fill_in_next_bird;
    sling->state.allow_reset_to_bird_camera = gate.next.allow_reset_to_bird_camera;
    if (gate.clear_launch_cue)
        sling->state.launch_cue = false;
    if (!sling->state.fill_in_next_bird)
        return true;
    if (sling->state.transfer_object == SIZE_MAX &&
        !begin_transfer(sling, state.current_bird_index + 1, error))
        return false;
    AbSlingTransferInput *input = &sling->state.transfer;
    size_t object = sling->state.transfer_object;
    input->seconds = seconds;
    input->rotation = sling->level.character_objects[object].jump.rotation;
    AbSlingTransferFrame frame;
    if (!ab_slingshot_transfer_frame(input, &frame))
        return ab_fail(error, "Unsupported classic sling transfer frame");
    /* The original increments its timer before its object/position calls. */
    input->time += seconds;
    if (!ab_classic_shots_set_position(sling->config.shots, object, frame.position,
                                       error) ||
        !ab_classic_level_set_rotation(sling->config.level, object, frame.rotation,
                                       error))
        return false;
    if (sling->level.character_objects[object].has_hat)
        return ab_fail(error, "Sling transfer hat motion is not recovered");
    return !frame.complete || complete_transfer(sling, error);
}

bool ab_classic_sling_update(AbClassicSling *sling, float seconds, AbError *error) {
    if (!healthy(sling, error) || sling->busy || !isfinite(seconds))
        return ab_fail(error, "Invalid or reentrant classic sling update");
    sling->busy = true;
    bool okay = hold(sling, error) && reload(sling, seconds, error);
    sling->busy = false;
    return okay;
}

bool ab_classic_sling_wobble(AbClassicSling *sling, float seconds, size_t maximum_steps,
                             AbError *error) {
    if (!healthy(sling, error) || sling->busy)
        return ab_fail(error, "Invalid or reentrant sling spring update");
    if (!sling->shot->bird_fired)
        return true;
    if (!isfinite(seconds))
        return ab_fail(error, "Unsupported sling spring duration");
    AbSlingBandState *band = &sling->state.band;
    size_t steps = 0;
    while (seconds > 0) {
        if (steps++ == maximum_steps)
            return ab_fail(error, "Sling spring update exceeds step limit");
        float elapsed = seconds < 0.05f ? seconds : 0.05f;
        seconds -= elapsed;
        AbVec2 direction = {band->rest.x - band->rubber_band.x,
                            band->rest.y - band->rubber_band.y};
        AbSlingStretch measured;
        if (!ab_slingshot_measure_stretch(band->rest, band->rubber_band, 2.2f + 3.2f,
                                          &measured) ||
            !ab_script_angle_from_components(direction.y, direction.x, &band->angle))
            return ab_fail(error, "Unsupported sling spring geometry");
        band->distance = measured.distance;
        float spring = 1500.0f * band->distance - 50.0f * band->wobble;
        float change = spring * 0.05f;
        change *= sling->config.physics_scale;
        band->wobble += change;
        if (!isfinite(band->wobble))
            return ab_fail(error, "Sling spring speed exceeds finite range");
        if (band->distance > 0) {
            AbVec2 offset = {direction.x / band->distance,
                             direction.y / band->distance};
            offset.x *= band->wobble;
            offset.y *= band->wobble;
            offset.x *= elapsed;
            offset.y *= elapsed;
            band->rubber_band.x += offset.x;
            band->rubber_band.y += offset.y;
            publish_band(sling);
            if (!ab_slingshot_measure_stretch(band->rest, band->rubber_band,
                                              2.2f + 3.2f, &measured))
                return ab_fail(error, "Sling spring position exceeds finite range");
            band->distance = measured.distance;
        }
    }
    return true;
}
