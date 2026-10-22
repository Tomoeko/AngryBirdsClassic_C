#include "ab/classic_game_mode.h"

#include <stdlib.h>

struct AbClassicGameMode {
    AbClassicGameModeConfig config;
    AbClassicModeFunction *paused;
    size_t paused_count, depth, bytes;
    bool calling, failed;
};

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool enter(AbClassicGameMode *mode, AbError *error) {
    if (!mode || mode->failed || mode->depth == mode->config.depth_limit)
        return ab_fail(error, "Invalid or exhausted game-mode invocation");
    ++mode->depth;
    return true;
}

static bool leave(AbClassicGameMode *mode, bool okay) {
    --mode->depth;
    mode->failed |= !okay;
    return okay;
}

static bool invoke(AbClassicModeFunction *function, const AbDataValue *arguments,
                   size_t count, AbError *error) {
    return function && function->call
               ? function->call(function->context, arguments, count, error)
               : ab_fail(error, "Missing reached actual game-mode function");
}

static bool paused_draw(void *context, const AbDataValue *arguments, size_t count,
                        AbError *error) {
    (void)arguments;
    (void)count;
    AbClassicGameMode *mode = context;
    return invoke(mode->config.draw_game, NULL, 0, error);
}

bool ab_classic_game_mode_open(AbClassicGameModeConfig config,
                               AbClassicGameMode **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing game-mode owner output");
    *output = NULL;
    if (!config.current || !config.update_game || !config.draw_game ||
        !config.play_mode || !config.museum_mode || !config.accelerometer_control ||
        !config.physics_enabled || !config.depth_limit || config.depth_limit > 64 ||
        config.paused_functions >
            (SIZE_MAX - sizeof(AbClassicGameMode)) / sizeof(AbClassicModeFunction))
        return ab_fail(error, "Invalid game-mode owner bindings");
    size_t bytes = sizeof(AbClassicGameMode) +
                   config.paused_functions * sizeof(AbClassicModeFunction);
    if (bytes > config.storage_bytes)
        return ab_fail(error, "Game-mode function storage exceeds bound");
    AbClassicGameMode *mode = calloc(1, bytes);
    if (!mode)
        return ab_fail(error, "Cannot allocate game-mode owner");
    mode->config = config;
    mode->paused = (AbClassicModeFunction *)(mode + 1);
    mode->bytes = bytes;
    *output = mode;
    return true;
}

void ab_classic_game_mode_close(AbClassicGameMode *mode) {
    free(mode);
}

size_t ab_classic_game_mode_bytes(const AbClassicGameMode *mode) {
    return mode ? mode->bytes : 0;
}

size_t ab_classic_game_mode_paused_count(const AbClassicGameMode *mode) {
    return mode ? mode->paused_count : 0;
}

bool ab_classic_game_mode_set(AbClassicGameMode *mode, AbClassicModeFunction *function,
                              AbError *error) {
    if (!enter(mode, error))
        return false;
    *mode->config.current = function;
    bool okay = true;
    if (truthy(*mode->config.accelerometer_control))
        okay = mode->config.motion_cursor
                   ? mode->config.motion_cursor(mode->config.context,
                                                function != mode->config.update_game,
                                                error)
                   : ab_fail(error, "Missing reached actual motion-cursor boundary");
    return leave(mode, okay);
}

bool ab_classic_game_mode_in_level(AbClassicGameMode *mode, AbDataValue *result,
                                   AbError *error) {
    if (!result || !enter(mode, error))
        return ab_fail(error, "Invalid game-mode level query");
    AbClassicPlayMode *play = *mode->config.play_mode;
    bool okay;
    if (!play)
        okay = ab_fail(error, "Missing reached actual play-mode instance");
    else if (play->kind == AB_CLASSIC_PLAY_MODE_MUSEUM)
        okay = ab_classic_museum_in_level(play->menus, result, error);
    else if (play->kind == AB_CLASSIC_PLAY_MODE_OTHER)
        okay = play->is_in_level
                   ? play->is_in_level(play->context, result, error)
                   : ab_fail(error, "Missing reached actual play-mode level query");
    else
        okay = ab_fail(error, "Unresolved reached play-mode class");
    return leave(mode, okay);
}

static bool physics(AbClassicGameMode *mode, bool enabled, AbError *error) {
    *mode->config.physics_enabled = enabled;
    return !mode->config.observe_physics ||
           mode->config.observe_physics(mode->config.context, enabled, error);
}

bool ab_classic_game_mode_pause(AbClassicGameMode *mode, AbDataValue paused,
                                AbError *error) {
    if (!enter(mode, error))
        return false;
    AbDataValue in_level;
    bool okay = ab_classic_game_mode_in_level(mode, &in_level, error);
    if (okay &&
        (truthy(in_level) || *mode->config.play_mode != *mode->config.museum_mode)) {
        if (truthy(paused)) {
            okay = physics(mode, false, error);
            if (okay && mode->paused_count == mode->config.paused_functions)
                okay = ab_fail(error, "Paused game-mode function storage exhausted");
            if (okay) {
                AbClassicModeFunction *function = mode->paused + mode->paused_count++;
                *function = (AbClassicModeFunction){mode, paused_draw};
                okay = ab_classic_game_mode_set(mode, function, error);
            }
        } else
            okay = ab_classic_game_mode_set(mode, mode->config.update_game, error) &&
                   physics(mode, true, error);
    }
    return leave(mode, okay);
}

bool ab_classic_game_mode_set_paused(void *context, bool paused, AbError *error) {
    return ab_classic_game_mode_pause(
        context, (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = paused}, error);
}

bool ab_classic_game_mode_call(AbClassicGameMode *mode, const AbDataValue *arguments,
                               size_t count, AbError *error) {
    if (!mode || mode->calling || (count && !arguments) || !enter(mode, error))
        return ab_fail(error, "Invalid or reentrant current game-mode call");
    mode->calling = true;
    AbClassicModeFunction *function = *mode->config.current;
    bool okay = invoke(function, arguments, count, error);
    mode->calling = false;
    return leave(mode, okay);
}

bool ab_classic_game_mode_root(void *context, const AbDataValue *arguments,
                               size_t count, AbDataValue *result, AbError *error) {
    bool okay = ab_classic_game_mode_call(context, arguments, count, error);
    if (okay && result)
        *result = (AbDataValue){0};
    return okay;
}
