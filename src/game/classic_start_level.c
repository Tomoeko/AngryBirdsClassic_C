#include "ab/classic_start_level.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataTable table;
    AbDataEntry id;
} StartNotice;

struct AbClassicStartLevel {
    AbClassicStartLevelConfig config;
    StartNotice *notices;
    AbDataValue completion_flag;
    size_t count;
    bool busy, failed;
};

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static AbDataValue get(const AbDataTable *table, AbDataValue key) {
    const AbDataValue *value = ab_data_get(table, key);
    return value ? *value : (AbDataValue){0};
}

bool ab_classic_start_level_open(AbClassicStartLevelConfig config,
                                 AbClassicStartLevel **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing start-level owner output");
    *output = NULL;
    if (!config.camera || !config.input || !config.shared_input || !config.shake ||
        !config.events || !config.event_ids || !config.level_scripts ||
        !config.promo_campaigns || !config.touch_gestures ||
        !config.started_from_editor || !config.level_name || !config.starts ||
        config.starts >
            (SIZE_MAX - sizeof(AbClassicStartLevel)) / (3 * sizeof(StartNotice)))
        return ab_fail(error, "Invalid start-level bindings/capacity");
    size_t bytes =
        sizeof(AbClassicStartLevel) + config.starts * 3 * sizeof(StartNotice);
    if (bytes > config.storage_bytes)
        return ab_fail(error, "Start-level records exceed byte bound");
    const char *const ids[] = {"EID_CAMERAS_INITIALISED", "EID_LEVEL_LOADING_DONE",
                               "EID_GOTO_GAME"};
    for (size_t index = 0; index < 3; ++index)
        if (get(config.event_ids, text(ids[index])).kind == AB_DATA_NIL)
            return ab_fail(error, "Missing start-level event definition");
    AbClassicStartLevel *start = calloc(1, bytes);
    if (!start)
        return ab_fail(error, "Cannot allocate start-level owner");
    start->config = config;
    start->notices = (StartNotice *)(start + 1);
    *output = start;
    return true;
}

void ab_classic_start_level_close(AbClassicStartLevel *start) {
    free(start);
}

static bool notify(AbClassicStartLevel *start, StartNotice *record, const char *name,
                   AbError *error) {
    record->id = (AbDataEntry){text("id"), get(start->config.event_ids, text(name))};
    record->table = (AbDataTable){.entries = &record->id, .count = 1, .capacity = 1};
    return ab_event_manager_notify(start->config.events, &record->table, error);
}

static bool camera_start(AbClassicStartLevel *start, AbDataValue restart,
                         StartNotice *record, AbError *error) {
    AbClassicStartLevelConfig *config = &start->config;
    if (!truthy(*config->touch_gestures))
        return ab_fail(error, "Start-level PC camera input class remains unsupported");
    AbCameraSystem *camera = ab_classic_camera_system(config->camera);
    if (!ab_camera_touch_init(config->input, config->shared_input) ||
        !ab_camera_system_input_enabled(camera, true) ||
        !ab_camera_system_movement_tweens(camera, AB_CAMERA_TWEEN_CUBIC_IN_OUT,
                                          AB_CAMERA_TWEEN_CUBIC_IN_OUT) ||
        !ab_camera_shake_remove(config->shake))
        return ab_fail(error, "Could not initialize source camera startup state");
    if (!notify(start, record, "EID_CAMERAS_INITIALISED", error))
        return false;
    return ab_classic_camera_start_sequence(config->camera,
                                            truthy(restart)
                                                ? "RestartLevelCameraSequence"
                                                : "DefaultLevelStartCameraSequence",
                                            error);
}

static bool level_hook(AbClassicStartLevel *start, AbError *error) {
    AbClassicStartLevelConfig *config = &start->config;
    AbDataValue record = get(config->level_scripts, *config->level_name);
    if (!truthy(record))
        return true;
    if (record.kind != AB_DATA_TABLE || !record.table)
        return ab_fail(error, "Reached start-level script record requires a table");
    AbDataValue hook = get(record.table, text("onStartLevel"));
    if (!truthy(hook))
        return true;
    if (hook.kind != AB_DATA_OBJECT || !hook.object)
        return ab_fail(error, "Reached start-level hook requires its owned C callable");
    const AbClassicLoaderCall *call = hook.object;
    return call->call ? call->call(call->context, error)
                      : ab_fail(error, "Missing reached start-level hook operation");
}

bool ab_classic_start_level(AbClassicStartLevel *start, AbDataValue restart,
                            AbError *error) {
    if (!start || start->failed || start->busy)
        return ab_fail(error, "Invalid or stopped start-level owner");
    if (start->count == start->config.starts) {
        start->failed = true;
        return ab_fail(error, "Retained start-level capacity exhausted");
    }
    start->busy = true;
    StartNotice *records = start->notices + start->count++ * 3;
    AbClassicStartLevelConfig *config = &start->config;
    start->completion_flag = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    bool okay = camera_start(start, restart, records, error) &&
                notify(start, records + 1, "EID_LEVEL_LOADING_DONE", error);
    if (okay && !truthy(*config->started_from_editor))
        okay = notify(start, records + 2, "EID_GOTO_GAME", error);
    if (okay)
        okay =
            !config->promo_campaigns->count ||
            ab_fail(error, "Start-level nonempty promo campaigns remain unsupported");
    if (okay)
        okay = level_hook(start, error);
    start->busy = false;
    start->failed = !okay;
    return okay;
}

bool ab_classic_start_level_call(void *context, AbError *error) {
    return ab_classic_start_level(context, (AbDataValue){0}, error);
}
