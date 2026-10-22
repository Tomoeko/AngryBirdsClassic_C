#include "ab/classic_launch_host.h"

#include <string.h>
#include <math.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

bool ab_classic_launch_host_initialize(AbClassicLaunchHostConfig config,
                                       AbClassicLaunchHost *output, AbError *error) {
    if (!output || !config.level || !ab_classic_level_view(config.level).properties ||
        !config.shots || !config.sling || !config.audio || !config.powerups_enabled ||
        !config.red_special_available)
        return ab_fail(error, "Invalid actual launch-host bindings");
    *output = (AbClassicLaunchHost){.config = config, .initialized = true};
    return true;
}

static bool bird_index(AbClassicLaunchHost *host, const AbShotCall *call,
                       size_t *output, AbError *error) {
    AbClassicShots *shots = *host->config.shots;
    if (!shots || ab_classic_shots_level(shots) != host->config.level)
        return ab_fail(error, "Unbound actual launch-host shot owner");
    AbClassicLevelView level = ab_classic_level_view(host->config.level);
    for (size_t index = 0; index < level.bodies->count; ++index) {
        if (call->bird && call->bird == ab_classic_shots_bird(shots, index)) {
            *output = index;
            return true;
        }
    }
    return ab_fail(error, "Foreign or absent actual launch-host bird");
}

static bool remaining(AbClassicLaunchHost *host, AbShotCall *call, AbError *error) {
    AbClassicLaunchHostConfig *config = &host->config;
    switch (call->operation) {
        case AB_SHOT_LEVEL_HOOK:
            return config->level_hook
                       ? config->level_hook(config->context, call, error)
                       : ab_fail(error, "Missing reached actual level shot hook");
        case AB_SHOT_DEFINITION_HOOK: {
            if (call->text && !strcmp(call->text, "redBirdShot")) {
                if (!truthy(*config->red_special_available))
                    return ab_classic_red_bird_shot(config->level, 0, false, error);
                size_t object;
                return bird_index(host, call, &object, error) &&
                       ab_classic_red_bird_shot(config->level, object, true, error);
            }
            return config->definition_hook
                       ? config->definition_hook(config->context, call, error)
                       : ab_fail(error, "Missing reached actual catalog shot hook");
        }
        case AB_SHOT_ATTACH_PARTICLES: {
            size_t object;
            return bird_index(host, call, &object, error) &&
                   ab_classic_shot_attach_particles(
                       config->level, object, config->powerups,
                       truthy(*config->powerups_enabled), error);
        }
        case AB_SHOT_RESOLVE_AUDIO:
            return ab_audio_bank_name(config->audio, call->text, &call->text, error);
        case AB_SHOT_PLAY_AUDIO: {
            AbDataValue volume = {.kind = AB_DATA_NUMBER};
            memcpy(&volume.bits, &call->value, sizeof(call->value));
            AbDataValue name = {0};
            if (call->text)
                name = (AbDataValue){
                    .kind = AB_DATA_STRING,
                    .string = {(const uint8_t *)call->text, strlen(call->text)}};
            uint32_t handle;
            return ab_audio_bank_play(config->audio,
                                      (AbAudioPlay){.name = name, .volume = volume},
                                      &handle, error);
        }
        case AB_SHOT_PLAY_SLING_SOUND: {
            uint32_t handle;
            if (!ab_audio_bank_default_sling_launch(config->audio, &handle, error))
                return false;
            AbClassicSling *sling = *config->sling;
            if (!sling || ab_classic_sling_shots(sling) != *config->shots)
                return ab_fail(error, "Missing reached actual sling launch-cue owner");
            ab_classic_sling_set_launch_cue(sling);
            return true;
        }
        default:
            return ab_fail(error, "Unexpected actual launch-host service %u",
                           (unsigned)call->operation);
    }
}

bool ab_classic_launch_host_remaining(void *context, AbShotCall *call, AbError *error) {
    AbClassicLaunchHost *host = context;
    if (!host || !host->initialized || host->failed || host->servicing || !call)
        return ab_fail(error, "Invalid or reentrant actual remaining launch service");
    host->servicing = true;
    bool okay = remaining(host, call, error);
    host->servicing = false;
    host->failed = !okay;
    return okay;
}

bool ab_classic_launch_host_dispatch(void *context, AbShotCall *call, AbError *error) {
    AbClassicLaunchHost *host = context;
    if (!host || !host->initialized || host->failed || host->dispatching ||
        host->servicing || !call)
        return ab_fail(error, "Invalid or reentrant actual launch dispatch");
    AbClassicSling *sling = *host->config.sling;
    if (!sling || ab_classic_sling_shots(sling) != *host->config.shots ||
        ab_classic_shots_level(*host->config.shots) != host->config.level)
        return ab_fail(error, "Missing shared actual launch shot/sling owners");
    host->dispatching = true;
    bool okay = ab_classic_sling_shot(sling, call, error);
    host->dispatching = false;
    host->failed = !okay;
    return okay;
}

static bool distance(AbVec2 first, AbVec2 second, float *output, AbError *error) {
    float x = first.x - second.x;
    float y = first.y - second.y;
    float xx = x * x, yy = y * y;
    float value = (float)sqrt((double)(xx + yy));
    if (!isfinite(first.x) || !isfinite(first.y) || !isfinite(second.x) ||
        !isfinite(second.y) || !isfinite(value))
        return ab_fail(error, "Nonfinite actual sling camera distance");
    *output = value;
    return true;
}

bool ab_classic_launch_host_camera_distances(void *context, float *castle_to_bird,
                                             float *to_bird, AbError *error) {
    AbClassicLaunchHost *host = context;
    if (!host || !host->initialized || host->failed || host->servicing ||
        host->dispatching || !castle_to_bird || !to_bird)
        return ab_fail(error, "Invalid actual reload camera query");
    host->servicing = true;
    AbCameraSystemView camera =
        ab_camera_system_view(host->config.camera ? *host->config.camera : NULL);
    bool okay = camera.limits && camera.limits->snaps && camera.limits->snap_count == 2;
    if (!okay)
        ab_fail(error, "Missing actual two-snap Museum reload camera");
    if (okay) {
        float range = 0, from_screen = 0;
        okay = distance(camera.limits->snaps[1].position,
                        camera.limits->snaps[0].position, &range, error) &&
               distance(camera.screen_position, camera.limits->snaps[0].position,
                        &from_screen, error);
        if (okay) {
            *castle_to_bird = range;
            *to_bird = from_screen;
        }
    }
    host->servicing = false;
    host->failed = !okay;
    return okay;
}

bool ab_classic_launch_host_reload_audio(void *context, float gain, AbError *error) {
    AbClassicLaunchHost *host = context;
    if (!host || !host->initialized || host->failed || host->servicing ||
        host->dispatching)
        return ab_fail(error, "Invalid actual reload audio request");
    host->servicing = true;
    AbDataValue selected;
    const AbDataValue *source = host->config.next_bird_sound;
    bool okay = source &&
                ab_audio_bank_name_value(host->config.audio, *source, &selected, error);
    if (!source)
        ab_fail(error, "Missing actual next-bird sound field");
    if (okay && selected.kind != AB_DATA_STRING)
        okay = ab_fail(error, "Unsupported actual reload resource sound name");
    uint32_t handle;
    if (okay)
        okay = ab_audio_bank_play_resource(host->config.audio, selected.string, gain,
                                           false, 0, &handle, error);
    host->servicing = false;
    host->failed = !okay;
    return okay;
}
