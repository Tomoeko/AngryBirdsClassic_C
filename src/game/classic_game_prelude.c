#include "ab/classic_game_prelude.h"

#include <math.h>
#include <string.h>

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Unsupported concrete game prelude number");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite concrete game prelude input");
}

static bool tutorial(AbSettingsStatistics *settings, AbDataValue key,
                     AbDataValue *output, AbError *error) {
    AbDataValue tutorials = field(ab_settings_statistics_root(settings), "tutorials");
    if (tutorials.kind == AB_DATA_NIL) {
        AbSpan path = span("tutorials");
        if (!ab_settings_statistics_create_table(settings, &path, 1, 0, error))
            return false;
        tutorials = field(ab_settings_statistics_root(settings), "tutorials");
    }
    if (tutorials.kind != AB_DATA_TABLE || !tutorials.table)
        return ab_fail(error, "Tutorial settings are not a table");
    if (key.kind != AB_DATA_STRING || (key.string.size && !key.string.data))
        return ab_fail(error, "Unsupported concrete tutorial item key");
    const AbDataValue *entry = ab_data_get(tutorials.table, key);
    *output = entry ? *entry : (AbDataValue){0};
    return true;
}

static bool show_help(AbSettingsStatistics *settings, AbDataValue key,
                      AbDataValue *output, AbError *error) {
    if (!tutorial(settings, key, output, error))
        return false;
    if (!truthy(*output))
        return true;
    /* The original calls getTutorialsForItem a second time before indexing. */
    if (!tutorial(settings, key, output, error))
        return false;
    if (output->kind != AB_DATA_TABLE || !output->table)
        return ab_fail(error, "Tutorial showHelp indexes a non-table");
    *output = field(output->table, "showHelp");
    return true;
}

static bool update_tutorial(AbClassicGamePreludeConfig *config, float seconds,
                            AbError *error) {
    AbDataValue flying = *config->flying;
    if (flying.kind == AB_DATA_NIL)
        return true;
    AbClassicLevelView level =
        ab_classic_level_view(ab_classic_shots_level(config->shots));
    size_t index = 0;
    while (index < level.bodies->count &&
           (flying.kind != AB_DATA_TABLE ||
            flying.table != &level.properties->objects[index].physical))
        ++index;
    if (index == level.bodies->count || level.removed_objects[index])
        return ab_fail(error, "Unsupported tutorial flying physical identity");
    AbDataValue name = field(flying.table, "name");
    if (name.kind != AB_DATA_STRING)
        return ab_fail(error, "Unsupported tutorial flying name");
    AbDataValue definition;
    if (!ab_object_definition_lookup(&level.properties->lookup, name.string,
                                     &definition, error))
        return false;
    if (definition.kind == AB_DATA_NIL)
        return true;
    if (definition.kind != AB_DATA_TABLE)
        return ab_fail(error, "Tutorial definition is not a table");
    AbDataValue sprite = field(definition.table, "sprite");
    if (sprite.kind == AB_DATA_NIL)
        return true;
    AbDataValue ordinary, special = {0};
    if (!show_help(config->settings, sprite, &ordinary, error))
        return false;
    AbDataValue key = field(flying.table, "specialTutorial");
    if (truthy(key) && !show_help(config->settings, key, &special, error))
        return false;
    if (!truthy(ordinary) && !truthy(special))
        return true;
    AbDataValue *extra = &ab_classic_shots_globals(config->shots)->extra_tutorial_timer;
    float timer;
    if (!numeric(*extra, &timer, error))
        return false;
    float next = timer + seconds;
    if (!isfinite(seconds) || !isfinite(next))
        return ab_fail(error, "Unsupported extra tutorial timer arithmetic");
    *extra = (AbDataValue){.kind = AB_DATA_NUMBER};
    memcpy(&extra->bits, &next, sizeof(next));
    return true;
}

static bool cursor_coordinates(AbClassicGamePreludeConfig *config, AbError *error) {
    *config->old_cursor_world = *config->cursor_world;
    AbCameraViewport camera;
    if (!ab_camera_system_query_viewport(ab_classic_camera_system(config->camera),
                                         &camera))
        return ab_fail(error, "Game prelude requires valid camera query state");
    AbCoordinateCamera transform = camera.rendering;
    AbVec2 point;
    if (!ab_coordinate_point(&transform, AB_SPACE_SCREEN, AB_SPACE_PHYSICS,
                             *config->cursor, &point))
        return ab_fail(error, "Unsupported game cursor physics transform");
    config->cursor_physics->y = point.y;
    config->cursor_physics->x = point.x;
    if (!ab_coordinate_point(&transform, AB_SPACE_SCREEN, AB_SPACE_WORLD,
                             *config->cursor, &point))
        return ab_fail(error, "Unsupported game cursor world transform");
    config->cursor_world->y = point.y;
    config->cursor_world->x = point.x;
    return true;
}

bool ab_classic_game_prelude_initialize(AbClassicGamePreludeConfig config,
                                        AbClassicGamePrelude *output, AbError *error) {
    if (!output || !ab_classic_shots_globals(config.shots) || !config.camera ||
        !config.effects || !config.effects->initialized ||
        !ab_settings_statistics_root(config.settings) || !config.flying ||
        !config.cheats_enabled || !config.touch_count || !config.do_not_timeout ||
        !config.cursor || !config.cursor_physics || !config.cursor_world ||
        !config.old_cursor_world)
        return ab_fail(error, "Invalid concrete game prelude bindings");
    *output = (AbClassicGamePrelude){.config = config, .initialized = true};
    return true;
}

bool ab_classic_game_prelude_update(AbClassicGamePrelude *prelude, float seconds,
                                    AbDataValue time, AbError *error) {
    if (!prelude || !prelude->initialized || prelude->updating || prelude->failed)
        return ab_fail(error, "Invalid, failed or reentrant concrete game prelude");
    prelude->updating = true;
    AbClassicGamePreludeConfig *config = &prelude->config;
    AbClassicGameGlobals *globals = ab_classic_shots_globals(config->shots);
    bool okay = ab_classic_game_globals_level_time(globals, seconds, error);
    if (okay && truthy(*config->cheats_enabled) &&
        config->touch_count->kind == AB_DATA_NUMBER) {
        float count;
        memcpy(&count, &config->touch_count->bits, sizeof(count));
        if (count == 6)
            okay = config->notify_toy_activated
                       ? config->notify_toy_activated(config->context, error)
                       : ab_fail(error, "Missing actual toy-activation notification");
    }
    okay = okay && update_tutorial(config, seconds, error);
    float current_time = 0;
    okay =
        okay && numeric(time, &current_time, error) &&
        ab_classic_game_effects_update(config->effects, seconds, current_time, error) &&
        ab_classic_game_globals_timeout(globals, *config->do_not_timeout, seconds, time,
                                        error) &&
        cursor_coordinates(config, error) &&
        ab_classic_shots_refresh_globals(config->shots, error);
    prelude->updating = false;
    prelude->failed = !okay;
    return okay;
}
