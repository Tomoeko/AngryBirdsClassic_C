#include "ab/classic_game_powerups.h"

#include <math.h>
#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool list_length(const AbClassicPowerupUpdates *list, size_t *length,
                        AbError *error) {
    if (!list || (list->capacity && !list->items))
        return ab_fail(error, "Missing actual powerup list storage");
    size_t count = 0;
    while (count < list->capacity && list->items[count])
        ++count;
    for (size_t index = count; index < list->capacity; ++index)
        if (list->items[index])
            return ab_fail(error, "Unsupported sparse powerup update list");
    *length = count;
    return true;
}

static bool released(AbClassicGamePowerupConfig *config, const char *key) {
    AbDataValue name = {.kind = AB_DATA_STRING,
                        .string = {(const uint8_t *)key, strlen(key)}};
    const AbDataValue *value = ab_data_get(config->released_keys, name);
    return value && truthy(*value);
}

bool ab_classic_game_powerups_transitioning(void *context, AbDataValue *output,
                                            AbError *error) {
    AbClassicGamePowerups *powerups = context;
    if (!powerups || !powerups->initialized || powerups->failed || !output)
        return ab_fail(error, "Invalid or stopped powerup transition query");
    size_t count = 0;
    if (!list_length(powerups->config.active, &count, error))
        return false;
    *output = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = count ? 1 : 0};
    return true;
}

static bool cheats(AbClassicGamePowerupConfig *config, AbError *error) {
    if (!truthy(*config->cheats_enabled))
        return true;
    static const char *const keys[] = {"1", "2", "3", "4", "5", "6", "7"};
    static const char *const names[] = {"powerpotion", "kingsling", "slingscope",
                                        "birdquake",   "hominghat", "boombox",
                                        "shockwave"};
    for (size_t index = 0; index < sizeof(keys) / sizeof(*keys); ++index) {
        if (!released(config, keys[index]))
            continue;
        if (!index) {
            size_t object;
            float current = ab_classic_sling_view(config->sling).current_bird_index;
            if (!ab_classic_sling_next_bird(config->sling, current, &object, error))
                return false;
            if (object == SIZE_MAX)
                continue;
        }
        if (!config->activate)
            return ab_fail(error, "Missing actual powerup activation service");
        if (!config->activate(config->context, names[index], error))
            return false;
    }
    return true;
}

static bool update_list(AbClassicPowerupUpdates *list, float seconds, AbError *error) {
    size_t ignored;
    if (!list_length(list, &ignored, error))
        return false;
    AbClassicPowerupUpdates captured = *list;
    for (size_t index = 0; index < captured.capacity; ++index) {
        AbClassicPowerupUpdate *powerup = captured.items[index];
        if (!powerup)
            break;
        if (!powerup->update)
            return ab_fail(error, "Missing actual active/passive powerup update");
        if (!powerup->update(powerup->context, seconds, error))
            return false;
    }
    return true;
}

static bool update_manager(AbClassicGamePowerupConfig *config, float seconds,
                           AbError *error) {
    if (!cheats(config, error))
        return false;
    size_t count = 0;
    if (!list_length(config->active, &count, error))
        return false;
    if (count && !ab_classic_sling_cancel_drag(config->sling, error))
        return false;
    return update_list(config->active, seconds, error) &&
           update_list(config->passive, seconds, error);
}

bool ab_classic_game_powerups_initialize(AbClassicGamePowerupConfig config,
                                         AbClassicGamePowerups *output,
                                         AbError *error) {
    if (!output || !config.sling || !config.active || !config.passive ||
        !config.enabled || !config.cheats_enabled || !config.released_keys ||
        !config.physics_speed_factor || !config.skip_input || !config.game_timer)
        return ab_fail(error, "Invalid concrete game powerup bindings");
    *output = (AbClassicGamePowerups){.config = config, .initialized = true};
    return true;
}

bool ab_classic_game_powerups_update(AbClassicGamePowerups *powerups, float seconds,
                                     AbError *error) {
    if (!powerups || !powerups->initialized || powerups->updating || powerups->failed)
        return ab_fail(error, "Invalid, failed or reentrant concrete game powerups");
    powerups->updating = true;
    AbClassicGamePowerupConfig *config = &powerups->config;
    bool okay = true;
    if (truthy(*config->enabled)) {
        size_t count = 0;
        okay = list_length(config->active, &count, error);
        if (okay && count) {
            float speed = *config->physics_speed_factor;
            if (!isfinite(speed))
                okay = ab_fail(error, "Unsupported active powerup physics speed");
            else if (speed > 0)
                *config->skip_input = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
        }
        okay = okay && update_manager(config, seconds, error);
    }
    if (okay) {
        float timer = *config->game_timer + seconds;
        if (!isfinite(*config->game_timer) || !isfinite(seconds) || !isfinite(timer))
            okay = ab_fail(error, "Unsupported gameTimer arithmetic");
        else
            *config->game_timer = timer;
    }
    powerups->updating = false;
    powerups->failed = !okay;
    return okay;
}

static bool draw_list(AbClassicPowerupUpdates *list, float zoom, AbDrawList *commands,
                      AbError *error) {
    size_t ignored;
    if (!list_length(list, &ignored, error))
        return false;
    AbClassicPowerupUpdates captured = *list;
    for (size_t index = 0; index < captured.capacity; ++index) {
        AbClassicPowerupUpdate *powerup = captured.items[index];
        if (!powerup)
            break;
        if (powerup->draw && !powerup->draw(powerup->context, zoom, commands, error))
            return false;
    }
    return true;
}

bool ab_classic_game_powerups_draw(AbClassicGamePowerups *powerups, float zoom,
                                   AbDrawList *list, AbError *error) {
    if (!powerups || !powerups->initialized || powerups->updating || powerups->failed ||
        !list)
        return ab_fail(error, "Invalid or reentrant concrete powerup draw");
    powerups->updating = true;
    bool okay = draw_list(powerups->config.passive, zoom, list, error) &&
                draw_list(powerups->config.active, zoom, list, error);
    powerups->updating = false;
    powerups->failed = !okay;
    return okay;
}
