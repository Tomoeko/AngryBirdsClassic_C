#include "ab/classic_game_mode_bindings.h"

#include <math.h>
#include <string.h>

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric reached actual game-mode argument");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite actual game-mode argument");
}

static bool update(void *context, const AbDataValue *arguments, size_t count,
                   AbError *error) {
    AbClassicGameModeBindings *owner = context;
    float seconds;
    if (count != 2 || !arguments || !numeric(arguments[0], &seconds, error))
        return ab_fail(error, "Invalid actual updateGame mode arguments");
    return ab_classic_game_update(owner->config.game, *owner->config.touches, seconds,
                                  arguments[1], *owner->config.camera, error);
}

static bool draw(void *context, const AbDataValue *arguments, size_t count,
                 AbError *error) {
    (void)arguments;
    AbClassicGameModeBindings *owner = context;
    float time;
    if (count || !numeric(owner->config.globals->time, &time, error))
        return ab_fail(error, "Invalid actual draw-only mode arguments or time");
    return ab_classic_game_draw(owner->config.draw, *owner->config.width,
                                *owner->config.height, time, owner->config.draws,
                                error);
}

bool ab_classic_game_mode_bindings_initialize(AbClassicGameModeBindingConfig config,
                                              AbClassicGameModeBindings *output,
                                              AbError *error) {
    if (!output || !config.game || !config.game->initialized || !config.draw ||
        !config.draw->initialized || !config.touches || !config.camera ||
        !config.globals || !config.width || !config.height || !config.draws ||
        ab_classic_shots_level(config.game->config.input->config.shots) !=
            config.draw->config.level ||
        ab_classic_shots_globals(config.game->config.input->config.shots) !=
            config.globals)
        return ab_fail(error, "Invalid actual game-mode update/draw bindings");
    *output = (AbClassicGameModeBindings){
        .config = config, .update = {output, update}, .draw = {output, draw}};
    return true;
}
