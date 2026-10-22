#include "ab/classic_game_effects.h"

bool ab_classic_game_effects_initialize(AbClassicGameEffectConfig config,
                                        AbClassicGameEffects *output, AbError *error) {
    if (!output || !config.shake || !config.random || !config.hero || !config.burn ||
        !ab_classic_trajectories_bytes(config.trajectories))
        return ab_fail(error, "Invalid concrete game effects bindings");
    *output = (AbClassicGameEffects){.config = config, .initialized = true};
    return true;
}

bool ab_classic_game_effects_update(AbClassicGameEffects *effects, float seconds,
                                    float time, AbError *error) {
    if (!effects || !effects->initialized || effects->updating || effects->failed)
        return ab_fail(error, "Invalid, failed or reentrant early game effects");
    effects->updating = true;
    AbClassicGameEffectConfig *config = &effects->config;
    bool okay = ab_camera_shake_update(config->shake, seconds, config->random);
    if (!okay)
        ab_fail(error, "Unsupported actual camera shake update");
    okay = okay && ab_hero_screen_tinter_update(config->hero, seconds, error) &&
           ab_classic_trajectories_update(config->trajectories, error) &&
           ab_burn_manager_update(config->burn, seconds, time, error);
    effects->failed = !okay;
    effects->updating = false;
    return okay;
}
