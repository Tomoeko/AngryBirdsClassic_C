#include "ab/ui_game_hud_services.h"
#include "ui_game_hud_internal.h"

#include <string.h>

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue boolean(bool value) {
    return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = value ? 1 : 0};
}

static AbDataValue get(const AbDataTable *table, AbDataValue key) {
    const AbDataValue *value = ab_data_get(table, key);
    return value ? *value : (AbDataValue){0};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    return get(table, text(name));
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool literal_false(AbDataValue value) {
    return value.kind == AB_DATA_BOOLEAN && !value.bits;
}

static bool same(AbDataValue left, AbDataValue right) {
    AbDataEntry entry = {left, boolean(true)};
    AbDataTable table = {&entry, 1, 1, NULL};
    return ab_data_get(&table, right) != NULL;
}

static bool table(AbDataValue value, const AbDataTable **output, AbError *error) {
    if (value.kind != AB_DATA_TABLE || !value.table ||
        value.table->count > value.table->capacity ||
        (value.table->count && !value.table->entries))
        return ab_fail(error, "GameHud query indexes a non-table source value");
    *output = value.table;
    return true;
}

static bool flag(AbUiGameHudServices *services, const char *container, const char *name,
                 bool synced, AbDataValue *output, AbError *error) {
    const AbDataTable *settings = services->eagle->settings;
    if (!settings)
        return ab_fail(error, "Missing actual GameHud settings table");
    AbDataValue value = field(settings, container);
    if (truthy(value)) {
        const AbDataTable *flags = NULL;
        if (!table(value, &flags, error))
            return false;
        value = field(flags, name);
        if (synced) {
            *output = boolean(truthy(value));
            return true;
        }
        if (value.kind != AB_DATA_NIL) {
            *output = value;
            return true;
        }
    }
    *output = boolean(false);
    return true;
}

static bool hiding(AbUiGameHudControls *controls, bool *output, AbError *error) {
    if (truthy(field(controls->host.globals, "eagleBaitLaunched"))) {
        *output = true;
        return true;
    }
    AbUiPointerValue ending;
    if (!ab_ui_game_hud_eagle_query(controls, AB_UI_EAGLE_LEVEL_ENDING, &ending, error))
        return false;
    *output = ab_ui_pointer_value_truthy(&ending);
    return true;
}

static bool current_level(AbUiGameHudControls *controls, AbError *error) {
    const AbClassicLevelRequest *level = controls->host.level;
    return (level && level->episode && level->page && level->level &&
            same(level->name, field(controls->host.globals, "levelName"))) ||
           ab_fail(error, "GameHud query has a stale canonical level request");
}

static bool powerup_visible(AbUiGameHudServices *services,
                            AbUiGameHudControls *controls, AbDataValue *output,
                            AbError *error) {
    const AbDataTable *globals = controls->host.globals;
    AbDataValue enabled = field(globals, "g_powerups_enabled");
    if (!truthy(enabled)) {
        *output = enabled;
        return true;
    }
    bool hide = false;
    if (!hiding(controls, &hide, error))
        return false;
    if (!hide && !truthy(field(globals, "g_enable_powerups_in_golden_egg_levels"))) {
        if (!current_level(controls, error))
            return false;
        const AbDataTable *page = controls->host.level->page;
        hide = truthy(field(page, "golden_egg_end_screen")) ||
               truthy(field(page, "calculate_as_golden_egg_levels"));
    }
    if (hide || ab_ui_pointer_value_truthy(&controls->hide_powerups)) {
        *output = boolean(false);
        return true;
    }
    AbDataValue episode = field(globals, "currentEpisode");
    if (!truthy(episode)) {
        *output = boolean(true);
        return true;
    }
    const AbDataTable *record = NULL;
    if (!table(get(services->eagle->progress.episodes, episode), &record, error))
        return false;
    *output = boolean(!truthy(field(record, "powerup_button_disabled")));
    return true;
}

static bool rmf_visible(AbUiGameHudServices *services, AbUiGameHudControls *controls,
                        AbDataValue *output, AbError *error) {
    if (!current_level(controls, error))
        return false;
    const AbClassicLevelRequest *level = controls->host.level;
    AbDataValue episode = field(level->episode, "rmf_button_enabled");
    AbDataValue page = field(level->page, "rmf_button_enabled");
    bool allowed = !literal_false(episode) && !literal_false(page);
    bool forced = truthy(episode) || truthy(page);
    bool hide = false;
    if (!hiding(controls, &hide, error))
        return false;
    *output = boolean(false);
    if (hide || !allowed ||
        !truthy(field(controls->host.globals, "g_powerups_enabled")))
        return true;
    AbDataValue purchased;
    if (!flag(services, "flagTable", "redsMightyFeathersPurchased", false, &purchased,
              error))
        return false;
    if (!truthy(purchased) && !forced)
        return true;
    AbDataValue model = field(controls->host.globals, "deviceModel");
    *output = boolean(same(model, text("iphone")) || same(model, text("ipad")));
    return true;
}

static bool query(AbUiGameHudServices *services, AbUiGameHudControls *controls,
                  AbUiGameHudQuery kind, AbDataValue *output, AbError *error) {
    switch (kind) {
        case AB_UI_HUD_PREMIUM:
            if (!services->eagle->progress.purchases)
                return ab_fail(error, "Missing actual GameHud purchases table");
            *output = field(services->eagle->progress.purchases, "isPremium");
            return true;
        case AB_UI_HUD_POWERUP_VISIBLE:
            return powerup_visible(services, controls, output, error);
        case AB_UI_HUD_OFFER_ON_START:
            return services->offer_on_start
                       ? services->offer_on_start(services->context, output, error)
                       : ab_fail(error, "Missing actual ShopOffer start query");
        case AB_UI_HUD_CHILDLOCK:
            if (!services->eagle->settings)
                return ab_fail(error, "Missing actual GameHud childlock settings");
            *output = field(services->eagle->settings, "shopChildlockOn");
            if (!truthy(*output))
                *output = boolean(false);
            return true;
        case AB_UI_HUD_TRIAL:
            return ab_ui_eagle_services_trial(services->eagle, output, error);
        case AB_UI_HUD_RMF_PURCHASED:
            return flag(services, "flagTable", "redsMightyFeathersPurchased", false,
                        output, error);
        case AB_UI_HUD_RMF_VISIBLE:
            return rmf_visible(services, controls, output, error);
        case AB_UI_HUD_SHOCKWAVE_VISIBLE:
            if (!flag(services, "syncedFlagTable", "shockwavePowerupUnlocked", true,
                      output, error))
                return false;
            return !truthy(*output) ||
                   powerup_visible(services, controls, output, error);
    }
    return ab_fail(error, "Unknown GameHud service query");
}

bool ab_ui_game_hud_services_query(void *context, AbUiGameHudControls *controls,
                                   AbUiGameHudQuery kind, AbUiPointerValue *output,
                                   AbError *error) {
    AbUiGameHudServices *services = context;
    if (!services || !services->eagle || !controls || !output ||
        !controls->host.globals ||
        controls->host.globals != services->eagle->progress.globals)
        return ab_fail(error, "Missing or inconsistent canonical GameHud services");
    AbDataValue result;
    if (!query(services, controls, kind, &result, error))
        return false;
    return ab_ui_game_hud_project(result, output, error);
}

bool ab_ui_game_hud_services_audio(void *context, bool *output, AbError *error) {
    AbUiGameHudServices *services = context;
    if (!services || !services->eagle || !services->local_settings || !output)
        return ab_fail(error, "Missing actual GameHud local audio settings owner");
    const AbDataTable *root = ab_settings_statistics_root(services->local_settings);
    if (!root || root != services->eagle->progress.local_settings)
        return ab_fail(error, "Inconsistent canonical GameHud local settings root");
    AbDataValue enabled = field(root, "audioEnabled");
    if (enabled.kind == AB_DATA_NIL) {
        AbSpan path = text("audioEnabled").string;
        if (!ab_settings_statistics_write(services->local_settings, &path, 1,
                                          boolean(true), error))
            return false;
    }
    *output = truthy(field(root, "audioEnabled"));
    return true;
}
