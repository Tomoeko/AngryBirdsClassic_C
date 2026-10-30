#include "ab/classic_level_runtime.h"
#include "ab/camera_bootstrap.h"
#include "ab/target_data.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { CAMERA_SOURCES = 7 };

struct AbClassicLevelRuntime {
    AbClassicLevelRuntimeConfig config;
    AbClassicLevelRuntimeView view;
    AbTargetData sources[CAMERA_SOURCES];
    AbCameraSequence sequences[CAMERA_SOURCES];
    AbCameraSnap snaps[2];
    float gravity;
};

static const char *const camera_names[CAMERA_SOURCES] = {
    "FollowShotBirdEndCameraSequence",
    "FollowShotBirdCameraSequence",
    "LevelEndSequence",
    "DefaultLevelStartCameraSequence",
    "RestartLevelCameraSequence",
    "AngerActivationCameraSequence",
    "BirdquakeCameraSequence"};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static bool sprite_size(void *context, AbSpan name, AbVec2 *output, AbError *error) {
    AbClassicLevelRuntime *runtime = context;
    const AbSpriteProvider *provider = runtime->config.playfield.sprites;
    AbSpriteAtlasBinding sprite;
    AbResolvedComposite composite;
    (void)error;
    if (provider->sprite(provider->context, name, &sprite) && sprite.sprite.region) {
        *output = (AbVec2){(float)sprite.sprite.region->width,
                           (float)sprite.sprite.region->height};
    } else if (provider->composite &&
               provider->composite(provider->context, name, &composite)) {
        *output =
            (AbVec2){(float)composite.bounds.width, (float)composite.bounds.height};
    } else
        *output = (AbVec2){0};
    return true;
}

static bool sequence(void *context, const char *name, const AbCameraSequence **output,
                     AbError *error) {
    AbClassicLevelRuntime *runtime = context;
    for (size_t index = 0; index < CAMERA_SOURCES; ++index)
        if (!strcmp(name, camera_names[index])) {
            *output = runtime->sequences + index;
            return true;
        }
    return ab_fail(error, "Unprepared actual level camera sequence: %s", name);
}

static bool sequence_log(void *context, AbSpan message, AbError *error) {
    AbClassicLevelRuntime *runtime = context;
    AbClassicCameraConfig *camera = &runtime->config.playfield.camera;
    return camera->sequence_log
               ? camera->sequence_log(camera->resource_context, message, error)
               : ab_fail(error, "Missing actual level camera log service");
}

static bool camera_sources(AbClassicLevelRuntime *runtime, AbSpan key, size_t *maximum,
                           AbError *error) {
    for (size_t index = 0; index < CAMERA_SOURCES; ++index) {
        char path[160];
        int size =
            snprintf(path, sizeof(path), "assets/data/scripts/cameraSequences/%s.lua",
                     camera_names[index]);
        if (size < 0 || (size_t)size >= sizeof(path) ||
            !ab_target_data_open(runtime->config.archive, path, key,
                                 runtime->config.resource_bytes, NULL,
                                 runtime->sources + index, error))
            return false;
        const AbDataValue *value = ab_data_get(
            runtime->sources[index].data.globals,
            (AbDataValue){.kind = AB_DATA_STRING, .string = span("sequence")});
        if (!value || value->kind != AB_DATA_TABLE ||
            !ab_camera_sequence_read(value->table, runtime->config.resource_bytes,
                                     runtime->sequences + index, error))
            return error && error->message[0]
                       ? false
                       : ab_fail(error, "Missing actual level camera sequence table");
        if (runtime->sequences[index].count > *maximum)
            *maximum = runtime->sequences[index].count;
    }
    return true;
}

static bool start_pose(AbClassicLevelRuntime *runtime, AbError *error) {
    AbClassicPlayfieldView playfield =
        ab_classic_playfield_view(runtime->view.playfield);
    if (!ab_classic_sling_level_reset(playfield.sling, error) ||
        !ab_classic_sling_next_bird(playfield.sling, 1, &runtime->view.first_bird,
                                    error))
        return false;
    if (runtime->view.first_bird == SIZE_MAX)
        return true;
    const AbSpriteProvider *provider = runtime->config.playfield.sprites;
    AbSpriteAtlasBinding sprite;
    if (!provider->sprite(provider->context, span("SLING_SHOT_01_BACK"), &sprite) ||
        !sprite.sprite.region)
        return ab_fail(error, "Missing actual first-bird sling back sprite");
    const AbSpriteRegion *back = sprite.sprite.region;
    float rounded = back->pivot_y + 0.5f;
    if (!isfinite(rounded) || (double)rounded < INT32_MIN ||
        (double)rounded > INT32_MAX)
        return ab_fail(error, "Invalid actual first-bird sling pivot");
    return ab_classic_sling_position_start(playfield.sling, runtime->view.first_bird,
                                           (float)(int32_t)rounded, (float)back->height,
                                           error);
}

bool ab_classic_level_runtime_open(AbClassicLevelRuntimeConfig config,
                                   AbClassicLevelRuntime **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing fresh level runtime output");
    *output = NULL;
    if (!config.archive || !config.level_path || !config.random ||
        !config.playfield.sprites || !config.playfield.sprites->sprite ||
        !config.playfield.shot_state || config.playfield.level)
        return ab_fail(error, "Missing fresh level runtime owners or selected source");
    AbClassicLevelRuntime *runtime = calloc(1, sizeof(*runtime));
    if (!runtime)
        return ab_fail(error, "Cannot allocate fresh level runtime owner");
    runtime->config = config;
    runtime->view.first_bird = SIZE_MAX;
    uint8_t key[32] = {0};
    AbClassicLevelAssets assets = {.level_path = config.level_path,
                                   .source = config.source,
                                   .catalog_path = "assets/data/scripts/blocks.lua",
                                   .resource_bytes = config.resource_bytes,
                                   .physics_scale = 0.05f,
                                   .physics_to_world = 20,
                                   .derive_placement_scale = true,
                                   .derive_ground_position = true,
                                   .derive_world_gravity = true,
                                   .random = config.random,
                                   .initial_birds = config.initial_birds,
                                   .prepare_context = config.prepare_context,
                                   .prepare = config.prepare,
                                   .sprite_context = runtime,
                                   .sprite_size = sprite_size};
    bool okay =
        ab_classic_level_load(config.archive, &assets, config.world,
                              config.level_limits, true, &runtime->view.level, error);
    size_t maximum = 0;
    AbClassicLevelView level = ab_classic_level_view(runtime->view.level);
    AbSpan resource_key = {key, sizeof(key)};
    if (okay && config.source) {
        AbClassicLevelSourceView shared;
        okay =
            ab_classic_level_source_view(config.source, config.archive, &shared, error);
        if (okay)
            resource_key = shared.key;
    }
    if (okay)
        okay =
            ab_level_placement_scale(level.data->globals, 20,
                                     &runtime->view.placement_scale, error) &&
            ab_camera_museum_snaps(level.data->globals,
                                   config.playfield.camera.pixel_size, runtime->snaps,
                                   error) &&
            (config.source || ab_target_resource_key(
                                  config.archive, config.resource_bytes, key, error)) &&
            camera_sources(runtime, resource_key, &maximum, error);
    memset(key, 0, sizeof(key));
    if (okay) {
        if (level.bird_count > UINT32_C(0x1000000)) {
            ab_classic_level_runtime_close(runtime);
            return ab_fail(error,
                           "Fresh level bird counter exceeds exact scalar domain");
        }
        config.playfield.shot_state->bird_count = (float)level.bird_count;
        runtime->gravity = level.world->definition.gravity.y;
        AbClassicPlayfieldConfig playfield = config.playfield;
        playfield.level = runtime->view.level;
        size_t count = level.bodies->count;
        size_t available = level.bodies->capacity - count;
        size_t additional = 0;
        if (config.reserve_objects &&
            !config.reserve_objects(config.reserve_context, level, &additional,
                                    error)) {
            ab_classic_level_runtime_close(runtime);
            return false;
        }
        if (additional > available ||
            config.additional_dynamic_objects > available - additional ||
            (level.bird_count &&
             config.dynamic_objects_per_bird >
                 (available - additional - config.additional_dynamic_objects) /
                     level.bird_count)) {
            ab_classic_level_runtime_close(runtime);
            return ab_fail(error, "Dynamic bird reservation exceeds level capacity");
        }
        size_t capacity = count + config.dynamic_objects_per_bird * level.bird_count +
                          config.additional_dynamic_objects + additional;
        runtime->view.object_capacity = capacity;
        if (playfield.limits.behavior_objects < capacity)
            playfield.limits.behavior_objects = capacity;
        if (playfield.limits.shot_objects < capacity)
            playfield.limits.shot_objects = capacity;
        if (playfield.camera.object_capacity < capacity)
            playfield.camera.object_capacity = capacity;
        playfield.camera.snaps = runtime->snaps;
        playfield.camera.snap_count = 2;
        playfield.camera.gravity = &runtime->gravity;
        playfield.camera.end_sequence = runtime->sequences;
        playfield.camera.shot_sequence = runtime->sequences + 1;
        playfield.camera.sequence_capacity = maximum;
        playfield.camera.resource_context = runtime;
        playfield.camera.load_sequence = sequence;
        playfield.camera.sequence_log = sequence_log;
        playfield.start = (AbClassicPlayfieldStart){
            .physics_scale = 0.05f,
            .default_force = -925,
            .current_bird_index = 1,
            .allow_reset_to_bird_camera =
                config.playfield.start.allow_reset_to_bird_camera};
        okay = ab_classic_playfield_open(playfield, &runtime->view.playfield, error) &&
               start_pose(runtime, error);
    }
    if (!okay) {
        ab_classic_level_runtime_close(runtime);
        return false;
    }
    *output = runtime;
    return true;
}

void ab_classic_level_runtime_close(AbClassicLevelRuntime *runtime) {
    if (!runtime)
        return;
    ab_classic_playfield_close(runtime->view.playfield);
    ab_classic_level_close(runtime->view.level);
    for (size_t index = 0; index < CAMERA_SOURCES; ++index) {
        ab_camera_sequence_free(runtime->sequences + index);
        ab_target_data_close(runtime->sources + index);
    }
    free(runtime);
}

bool ab_classic_level_runtime_resize(AbClassicLevelRuntime *runtime, AbVec2 pixel_size,
                                     AbError *error) {
    if (!runtime)
        return ab_fail(error, "Missing level drawable-size owner");
    AbClassicLevelView level = ab_classic_level_view(runtime->view.level);
    AbClassicPlayfieldView playfield =
        ab_classic_playfield_view(runtime->view.playfield);
    AbCameraSnap previous[2], refreshed[2];
    if (!ab_camera_museum_snaps(level.data->globals, pixel_size, refreshed, error))
        return false;
    memcpy(previous, runtime->snaps, sizeof(previous));
    memcpy(runtime->snaps, refreshed, sizeof(refreshed));
    if (!ab_camera_system_resize(ab_classic_camera_system(playfield.camera), pixel_size,
                                 error)) {
        memcpy(runtime->snaps, previous, sizeof(previous));
        return false;
    }
    return true;
}

AbClassicLevelRuntimeView
ab_classic_level_runtime_view(const AbClassicLevelRuntime *runtime) {
    return runtime ? runtime->view
                   : (AbClassicLevelRuntimeView){.first_bird = SIZE_MAX};
}
