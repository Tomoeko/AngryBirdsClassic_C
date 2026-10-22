#include "ab/classic_game_start.h"

#include <math.h>
#include <string.h>

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool pressed(AbClassicGameStartConfig *config, const char *key) {
    const AbDataValue *value = ab_data_get(config->keys, text(key));
    return value && truthy(*value);
}

static bool debug(AbClassicGameStartConfig *config, AbClassicGameStartDebug operation,
                  AbError *error) {
    return config->debug ? config->debug(config->context, operation, error)
                         : ab_fail(error, "Missing actual game debug/editor service");
}

static bool debug_input(AbClassicGameStartConfig *config, AbError *error) {
    if (!truthy(*config->release_build) || truthy(*config->cheats_enabled)) {
        if (pressed(config, "PAGEUP") &&
            !debug(config, AB_CLASSIC_GAME_START_PREVIOUS_LEVEL, error))
            return false;
        if (pressed(config, "PAGEDOWN") &&
            !debug(config, AB_CLASSIC_GAME_START_NEXT_LEVEL, error))
            return false;
    }
    if (truthy(*config->cheats_enabled) && pressed(config, "A") &&
        !debug(config, AB_CLASSIC_GAME_START_ANGER_KEY, error))
        return false;
    if (truthy(*config->started_from_editor) &&
        (pressed(config, "F5") || pressed(config, "R")) &&
        !debug(config, AB_CLASSIC_GAME_START_EDITOR_RESTART, error))
        return false;
    return true;
}

static bool audio_name(AbDataValue value, AbSpan *output, AbError *error) {
    if (value.kind != AB_DATA_STRING || (value.string.size && !value.string.data))
        return ab_fail(error, "Unsupported concrete game startup audio name");
    *output = value.string;
    return true;
}

static bool start_level(AbClassicGameStartConfig *config, AbError *error) {
    if (ab_classic_sling_view(config->sling).disabled_for_current_level)
        return true;
    float timer = config->state->level_start_timer;
    if (!isfinite(timer))
        return ab_fail(error, "Unsupported initial level-start timer");
    if (timer != 0)
        return true;
    config->state->level_start_timer = timer + 0.01f;
    AbDataValue selected;
    AbSpan name;
    uint32_t handle;
    if (!ab_audio_bank_name_value(config->audio, *config->level_start_sound, &selected,
                                  error) ||
        !audio_name(selected, &name, error) ||
        !ab_audio_bank_play_resource(config->audio, name, 1, false, 0, &handle,
                                     error) ||
        !audio_name(*config->main_menu_song, &name, error) ||
        !ab_audio_bank_stop(config->audio, name, error))
        return false;
    return ab_classic_sling_start_queue(config->sling, error);
}

static bool music(AbClassicGameStartConfig *config, AbError *error) {
    if (config->current_music->kind == AB_DATA_NIL)
        return true;
    AbSpan name;
    bool playing;
    if (!audio_name(*config->current_music, &name, error) ||
        !ab_audio_bank_is_playing(config->audio, name, &playing, error))
        return false;
    if (playing)
        return true;
    uint32_t handle;
    return ab_audio_bank_play(
        config->audio,
        (AbAudioPlay){.name = *config->current_music,
                      .volume = number(1),
                      .loop = {.kind = AB_DATA_BOOLEAN, .bits = 1},
                      .channel = number(7)},
        &handle, error);
}

static bool enable_physics(AbClassicGameStartConfig *config, float seconds,
                           AbError *error) {
    float timer = config->state->level_start_timer;
    if (!isfinite(timer))
        return ab_fail(error, "Unsupported level-start timer after callbacks");
    if (timer < 1 || timer > 6) {
        float next = timer + seconds;
        if (!isfinite(seconds) || !isfinite(next))
            return ab_fail(error, "Unsupported level-start timer arithmetic");
        config->state->level_start_timer = next;
    } else {
        *config->physics_enabled = true;
        config->state->level_start_timer = 10;
    }
    return true;
}

bool ab_classic_game_start_initialize(AbClassicGameStartConfig config,
                                      AbClassicGameStart *output, AbError *error) {
    if (!output || !config.sling || !config.audio || !config.updates || !config.state ||
        !config.release_build || !config.cheats_enabled ||
        !config.started_from_editor || !config.level_start_sound ||
        !config.main_menu_song || !config.current_music || !config.keys ||
        !config.physics_enabled)
        return ab_fail(error, "Invalid concrete game startup bindings");
    *output = (AbClassicGameStart){.config = config, .initialized = true};
    return true;
}

bool ab_classic_game_start_update(AbClassicGameStart *start, float seconds, float time,
                                  AbError *error) {
    if (!start || !start->initialized || start->updating || start->failed)
        return ab_fail(error, "Invalid, failed or reentrant concrete game startup");
    start->updating = true;
    AbClassicGameStartConfig *config = &start->config;
    config->state->right_sweep = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    config->state->left_sweep = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    float timer = config->state->quad_click_timer - seconds;
    bool okay = isfinite(config->state->quad_click_timer) && isfinite(seconds) &&
                isfinite(timer);
    if (okay)
        config->state->quad_click_timer = timer;
    else
        ab_fail(error, "Unsupported quad-click timer arithmetic");
    okay = okay && debug_input(config, error) && start_level(config, error) &&
           music(config, error) &&
           ab_classic_game_updates_call(config->updates, seconds, time, error);
    if (okay && config->level_update)
        okay = config->level_update(config->context, seconds, time, error);
    okay = okay && enable_physics(config, seconds, error);
    start->updating = false;
    start->failed = !okay;
    return okay;
}
