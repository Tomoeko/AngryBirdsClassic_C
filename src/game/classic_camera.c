#include "ab/classic_camera.h"
#include "ab/bird_camera_behavior.h"
#include "ab/camera_follow_world.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbClassicCamera *owner;
    size_t index;
    AbCameraFollowBird bird;
    AbBirdImpactWorldHost prediction;
    AbCameraFollowWorld world;
} ClassicCameraObject;

struct AbClassicCamera {
    AbClassicCameraConfig config;
    AbClassicLevelView level;
    AbCameraSystem *system;
    AbBirdImpactWorld world;
    ClassicCameraObject *objects;
    size_t count, capacity, flying, stage_capacity;
    AbCameraSequence sequence;
    AbCameraSequenceStage *stages;
    bool sequence_loaded;
};

static bool named(AbSpan value, const char *name) {
    return value.size == strlen(name) &&
           (!value.size || (value.data && !memcmp(value.data, name, value.size)));
}

static AbDataValue field(const AbDataTable *source, const char *name) {
    AbDataValue key = {.kind = AB_DATA_STRING,
                       .string = {(const uint8_t *)name, strlen(name)}};
    const AbDataValue *value = ab_data_get(source, key);
    return value ? *value : (AbDataValue){0};
}

static bool valid_table(AbDataValue value) {
    return value.kind == AB_DATA_TABLE && value.table &&
           value.table->count <= value.table->capacity &&
           (!value.table->count || value.table->entries);
}

static bool refresh_object(ClassicCameraObject *object, AbError *error) {
    AbClassicCamera *camera = object->owner;
    AbDataTable *physical = &camera->level.properties->objects[object->index].physical;
    AbDataValue launch = field(physical, "launchVelocity");
    AbCameraFollowBird bird = {
        .position = camera->level.character_objects[object->index].jump.position};
    if (launch.kind != AB_DATA_NIL && (launch.kind != AB_DATA_BOOLEAN || launch.bits)) {
        if (!valid_table(launch)) {
            return ab_fail(error, "Unsupported owned camera launch vector");
        }
        AbDataValue x = field(launch.table, "x"), y = field(launch.table, "y");
        if (x.kind != AB_DATA_NUMBER || y.kind != AB_DATA_NUMBER) {
            return ab_fail(error, "Nonnumeric owned camera launch vector");
        }
        memcpy(&bird.launch_velocity.x, &x.bits, sizeof(x.bits));
        memcpy(&bird.launch_velocity.y, &y.bits, sizeof(y.bits));
        bird.launch_present = true;
    }
    object->bird = bird;
    object->world.host.gravity = *camera->config.gravity;
    return true;
}

static bool prediction_radius(void *context, float *output, AbError *error) {
    ClassicCameraObject *object = context;
    AbDataValue value = {0};
    if (!ab_level_property_get(object->owner->level.properties, object->index,
                               (AbSpan){(const uint8_t *)"radius", 6}, &value, error)) {
        return false;
    }
    if (value.kind != AB_DATA_NUMBER) {
        return ab_fail(error, "Nonnumeric owned camera radius");
    }
    memcpy(output, &value.bits, sizeof(*output));
    return refresh_object(object, error);
}

static bool prediction_print(void *context, size_t rays, AbError *error) {
    ClassicCameraObject *object = context;
    AbClassicImpactServices *services = &object->owner->config.prediction;
    if (!services->print) {
        return ab_fail(error, "Missing owned camera prediction diagnostic");
    }
    return services->print(services->context, rays, error) &&
           refresh_object(object, error);
}

static bool prediction_observe(void *context, AbVec2 first, AbVec2 second,
                               const AbLevelRaycast *result, AbError *error) {
    AbClassicImpactServices *services =
        &((ClassicCameraObject *)context)->owner->config.prediction;
    return !services->observe ||
           services->observe(services->context, first, second, result, error);
}

static bool start_sequence(void *context, const char *name, AbError *error) {
    AbClassicCamera *camera = context;
    const char *message;
    if (!strcmp(name, "FollowShotBirdCameraSequence")) {
        message = "<CameraSequenceManager> loading sequence from file: "
                  "FollowShotBirdCameraSequence";
    } else if (!strcmp(name, "FollowShotBirdEndCameraSequence")) {
        message = "<CameraSequenceManager> loading sequence from file: "
                  "FollowShotBirdEndCameraSequence";
    } else if (!strcmp(name, "LevelEndSequence")) {
        message =
            "<CameraSequenceManager> loading sequence from file: LevelEndSequence";
    } else if (!strcmp(name, "DefaultLevelStartCameraSequence")) {
        message = "<CameraSequenceManager> loading sequence from file: "
                  "DefaultLevelStartCameraSequence";
    } else if (!strcmp(name, "RestartLevelCameraSequence")) {
        message = "<CameraSequenceManager> loading sequence from file: "
                  "RestartLevelCameraSequence";
    } else {
        return ab_fail(error, "Unsupported owned camera sequence resource");
    }
    if (!camera->config.load_sequence)
        return ab_fail(error, "Missing owned camera sequence loader");
    const AbCameraSequence *source = NULL;
    if (!camera->config.load_sequence(camera->config.resource_context, name, &source,
                                      error)) {
        return false;
    }
    if (!camera->config.sequence_log)
        return ab_fail(error, "Missing owned camera sequence logger");
    if (!camera->config.sequence_log(
            camera->config.resource_context,
            (AbSpan){(const uint8_t *)message, strlen(message)}, error)) {
        return false;
    }
    message = "<CameraSequenceManager> starting new camera sequence";
    if (!camera->config.sequence_log(
            camera->config.resource_context,
            (AbSpan){(const uint8_t *)message, strlen(message)}, error)) {
        return false;
    }
    if (!source || source->count > camera->stage_capacity ||
        (source->count && !source->stages)) {
        return ab_fail(error, "Changed owned camera sequence storage");
    }
    if (source->count)
        memcpy(camera->stages, source->stages, source->count * sizeof(*camera->stages));
    camera->sequence = (AbCameraSequence){camera->stages, source->count};
    camera->sequence_loaded = true;
    return ab_camera_system_start(camera->system, &camera->sequence, NULL, NULL, error);
}

static bool cancel_sequence(void *context, AbError *error) {
    return ab_camera_system_cancel_sequence(((AbClassicCamera *)context)->system,
                                            error);
}

static bool resolve_target(void *context, AbDataValue target,
                           const AbCameraFollowBird **bird,
                           AbCameraFollowHost **environment, AbError *error) {
    AbClassicCamera *camera = context;
    AbDataValue selected = target;
    if (target.kind == AB_DATA_STRING) {
        if (!ab_object_world_lookup(&camera->level.properties->lookup, target,
                                    &selected, error))
            return false;
        if (selected.kind != AB_DATA_NIL &&
            (selected.kind != AB_DATA_BOOLEAN || selected.bits)) {
            if (!ab_object_world_lookup(&camera->level.properties->lookup, target,
                                        &selected, error))
                return false;
        } else if (named(target.string, "flyingBird")) {
            selected = camera->flying == SIZE_MAX
                           ? (AbDataValue){0}
                           : (AbDataValue){.kind = AB_DATA_TABLE,
                                           .table = &camera->level.properties
                                                         ->objects[camera->flying]
                                                         .physical};
        } else if (named(target.string, "pigWithMostStrength")) {
            return ab_fail(error,
                           "Live goal selector requires camera binding recovery");
        } else {
            selected = (AbDataValue){0};
        }
    }
    *bird = NULL;
    *environment = NULL;
    if (selected.kind == AB_DATA_NIL ||
        (selected.kind == AB_DATA_BOOLEAN && !selected.bits))
        return true;
    size_t index = 0;
    while (index < camera->count &&
           (selected.kind != AB_DATA_TABLE ||
            selected.table != &camera->level.properties->objects[index].physical))
        ++index;
    if (index == camera->count)
        return ab_fail(error, "Unsupported owned camera target value or identity");
    ClassicCameraObject *object = camera->objects + index;
    if (!refresh_object(object, error))
        return false;
    *bird = &object->bird;
    *environment = &object->world.host;
    return true;
}

static bool initialize_object(AbClassicCamera *camera, size_t index, AbError *error) {
    ClassicCameraObject *object = camera->objects + index;
    object->owner = camera;
    object->index = index;
    object->prediction = (AbBirdImpactWorldHost){
        object, camera->config.prediction.maximum_raycasts, prediction_radius,
        prediction_print, prediction_observe};
    AbCameraFollowWorldConfig world = {&camera->world, &object->prediction,
                                       camera->config.gravity, camera, start_sequence};
    return ab_camera_follow_world_initialize(&object->world, &world, error);
}

bool ab_classic_camera_open(const AbClassicCameraConfig *config,
                            AbClassicCamera **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing owned level camera output");
    *output = NULL;
    if (!config || !config->level || !config->gravity || !config->shot_sequence ||
        !config->end_sequence || config->storage_bytes <= sizeof(AbClassicCamera)) {
        return ab_fail(error, "Invalid owned level camera configuration");
    }
    AbClassicLevelView level = ab_classic_level_view(config->level);
    if (!level.bodies || !level.character_objects || !level.properties ||
        level.properties->count != level.bodies->count) {
        return ab_fail(error, "Invalid owned level camera object storage");
    }
    size_t stages = config->shot_sequence->count > config->end_sequence->count
                        ? config->shot_sequence->count
                        : config->end_sequence->count;
    if (config->sequence_capacity > stages)
        stages = config->sequence_capacity;
    size_t remaining = config->storage_bytes - sizeof(AbClassicCamera);
    size_t capacity =
        config->object_capacity ? config->object_capacity : level.bodies->count;
    if (capacity < level.bodies->count || capacity > level.bodies->capacity ||
        capacity > remaining / sizeof(ClassicCameraObject))
        return ab_fail(error, "Owned camera objects exceed the storage limit");
    remaining -= capacity * sizeof(ClassicCameraObject);
    if (stages >= UINT32_C(0x1000000) ||
        stages > remaining / sizeof(AbCameraSequenceStage) ||
        (config->shot_sequence->count && !config->shot_sequence->stages) ||
        (config->end_sequence->count && !config->end_sequence->stages)) {
        return ab_fail(error, "Owned camera stages exceed the storage limit");
    }
    remaining -= stages * sizeof(AbCameraSequenceStage);
    AbClassicCamera *camera = calloc(1, sizeof(*camera));
    if (!camera)
        return ab_fail(error, "Cannot allocate owned level camera");
    camera->config = *config;
    camera->level = level;
    camera->count = level.bodies->count;
    camera->capacity = capacity;
    camera->stage_capacity = stages;
    camera->flying = SIZE_MAX;
    camera->objects = calloc(capacity, sizeof(*camera->objects));
    camera->stages = stages ? calloc(stages, sizeof(*camera->stages)) : NULL;
    if ((capacity && !camera->objects) || (stages && !camera->stages)) {
        ab_fail(error, "Cannot allocate owned level camera arrays");
        goto release_camera;
    }
    if (!ab_camera_system_open(config->snaps, config->snap_count, config->pixel_size,
                               config->absolute_bottom, config->horizontal_leeway,
                               remaining, &camera->system, error)) {
        goto release_camera;
    }
    camera->world =
        (AbBirdImpactWorld){level.world, level.bodies, level.removed_objects};
    for (size_t index = 0; index < camera->count; ++index) {
        if (!initialize_object(camera, index, error))
            goto release_camera;
    }
    if (!ab_camera_system_bind_follow(camera->system, camera, resolve_target))
        goto release_camera;
    *output = camera;
    return true;

release_camera:
    ab_classic_camera_close(camera);
    return false;
}

void ab_classic_camera_close(AbClassicCamera *camera) {
    if (camera) {
        ab_camera_system_close(camera->system);
        free(camera->objects);
        free(camera->stages);
        free(camera);
    }
}

AbCameraSystem *ab_classic_camera_system(AbClassicCamera *camera) {
    return camera ? camera->system : NULL;
}

AbClassicCameraView ab_classic_camera_view(const AbClassicCamera *camera) {
    AbClassicCameraView view = {.flying_object = SIZE_MAX, .target_object = SIZE_MAX};
    if (!camera)
        return view;
    view.system = ab_camera_system_view(camera->system);
    view.loaded_sequence = camera->sequence_loaded ? &camera->sequence : NULL;
    view.flying_object = camera->flying;
    view.object_count = camera->count;
    view.object_capacity = camera->capacity;
    const AbCameraFollowBird *target = ab_camera_system_target_bird(camera->system);
    for (size_t index = 0; target && index < camera->count; ++index) {
        if (target == &camera->objects[index].bird) {
            view.target_object = index;
            break;
        }
    }
    return view;
}

bool ab_classic_camera_register_object(AbClassicCamera *camera, size_t object,
                                       AbError *error) {
    if (!camera || object != camera->count || object >= camera->capacity ||
        object >= camera->level.bodies->count ||
        object >= camera->level.properties->count ||
        camera->level.removed_objects[object] || camera->level.world->failed ||
        camera->level.world->locked)
        return ab_fail(error, "Invalid new owned camera object binding");
    if (!initialize_object(camera, object, error))
        return false;
    ++camera->count;
    return true;
}

bool ab_classic_camera_set_flying(AbClassicCamera *camera, size_t object,
                                  AbError *error) {
    if (!camera || (object != SIZE_MAX && object >= camera->count))
        return ab_fail(error, "Invalid owned camera flying-bird identity");
    camera->flying = object;
    return true;
}

bool ab_classic_camera_on_launch(AbClassicCamera *camera, AbError *error) {
    if (!camera)
        return ab_fail(error, "Missing owned camera launch binding");
    AbBirdCameraBehaviorHost host = {camera, start_sequence, cancel_sequence};
    return ab_bird_camera_on_launch(&host, error);
}

bool ab_classic_camera_start_sequence(AbClassicCamera *camera,
                                      const char *resource_name, AbError *error) {
    if (!camera || !resource_name)
        return ab_fail(error, "Missing owned camera sequence request");
    return start_sequence(camera, resource_name, error);
}

bool ab_classic_camera_blues_initialize(AbClassicCamera *camera, AbError *error) {
    if (!camera)
        return ab_fail(error, "Missing owned Blues camera binding");
    AbBirdCameraBehaviorHost host = {camera, start_sequence, cancel_sequence};
    return ab_bird_camera_blues_initialize(&host, error);
}

bool ab_classic_camera_remove_bird(AbClassicCamera *camera, size_t object,
                                   AbError *error) {
    if (!camera || object >= camera->count)
        return ab_fail(error, "Invalid owned camera bird removal");
    return ab_camera_system_remove_bird(camera->system, &camera->objects[object].bird);
}

bool ab_classic_camera_update(AbClassicCamera *camera, float seconds,
                              const AbCameraSystemInput *input, AbError *error) {
    if (!camera || !input)
        return ab_fail(error, "Missing owned level camera update");
    const AbCameraFollowBird *target = ab_camera_system_target_bird(camera->system);
    if (target) {
        size_t index = 0;
        while (index < camera->count && target != &camera->objects[index].bird)
            ++index;
        if (index == camera->count)
            return ab_fail(error, "Invalid owned level camera target binding");
        if (!refresh_object(camera->objects + index, error))
            return false;
    }
    AbCameraSystemInput resolved = *input;
    if (camera->config.input &&
        !camera->config.input(camera->config.input_context, camera->system, seconds,
                              &resolved, error))
        return false;
    return ab_camera_system_update(camera->system, seconds, &resolved, error);
}
