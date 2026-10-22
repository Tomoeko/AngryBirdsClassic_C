#include "ab/classic_game_update.h"

#include <math.h>
#include <string.h>

bool ab_classic_game_update_initialize(AbClassicGameUpdateConfig config,
                                       AbClassicGameUpdate *output, AbError *error) {
    if (!output || !config.prelude || !config.prelude->initialized || !config.start ||
        !config.start->initialized || !config.progress || !config.powerups ||
        !config.powerups->initialized || !config.particles ||
        !config.particles->initialized || !config.input || !config.input->initialized ||
        !config.controller ||
        ab_classic_controller_input(config.controller) != config.input)
        return ab_fail(error, "Invalid composed GameUpdate phase bindings");
    AbClassicGameInputConfig *input = &config.input->config;
    AbClassicGamePreludeConfig *prelude = &config.prelude->config;
    AbClassicGameStartConfig *start = &config.start->config;
    AbClassicGamePowerupConfig *powerups = &config.powerups->config;
    if (prelude->shots != input->shots || start->sling != input->sling ||
        powerups->sling != input->sling || prelude->flying != input->flying ||
        ab_classic_camera_system(prelude->camera) != input->camera ||
        start->keys != input->pressed || powerups->released_keys != input->released ||
        prelude->cursor != &input->state->cursor ||
        prelude->cursor_physics != &input->state->cursor_physics ||
        prelude->cheats_enabled != &input->state->cheats_enabled ||
        start->cheats_enabled != &input->state->cheats_enabled ||
        powerups->cheats_enabled != &input->state->cheats_enabled ||
        prelude->touch_count != &input->state->touch_count ||
        powerups->skip_input != &input->state->skip_input ||
        start->physics_enabled != config.particles->config.physics_enabled)
        return ab_fail(error, "Composed GameUpdate requires canonical shared state");
    *output = (AbClassicGameUpdate){.config = config, .initialized = true};
    return true;
}

bool ab_classic_game_update(AbClassicGameUpdate *game, AbClassicGameTouchInput touches,
                            float seconds, AbDataValue time, AbCameraSystemInput camera,
                            AbError *error) {
    if (!game || !game->initialized || game->updating || game->failed ||
        time.kind != AB_DATA_NUMBER || !isfinite(seconds))
        return ab_fail(error, "Invalid, failed or reentrant composed GameUpdate");
    float now = 0;
    memcpy(&now, &time.bits, sizeof(now));
    if (!isfinite(now))
        return ab_fail(error, "Nonfinite composed GameUpdate time");
    game->updating = true;
    AbClassicGameUpdateConfig *config = &game->config;
    bool okay =
        ab_classic_game_prelude_update(config->prelude, seconds, time, error) &&
        ab_classic_game_start_update(config->start, seconds, now, error) &&
        ab_classic_game_progress_update(config->progress, seconds, error) &&
        ab_classic_game_powerups_update(config->powerups, seconds, error) &&
        ab_classic_game_level_particles_update(config->particles, seconds, now,
                                               error) &&
        ab_classic_game_input_update(config->input, touches, seconds, time, error);
    if (okay) {
        camera.shake = config->prelude->config.effects->config.shake->offset;
        okay = ab_classic_controller_update(config->controller, seconds, now, &camera,
                                            error);
    }
    game->updating = false;
    game->failed = !okay;
    return okay;
}
