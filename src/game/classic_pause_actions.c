#include "ab/classic_pause_actions.h"

#include <string.h>

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static bool valid(AbClassicPauseActions *actions, AbError *error) {
    return (actions && actions->initialized && !actions->busy && !actions->failed) ||
           ab_fail(error, "Invalid, active or failed common-game pause actions");
}

bool ab_classic_pause_actions_init(AbClassicPauseActions *actions,
                                   AbClassicPauseActionConfig config, AbError *error) {
    if (!actions || actions->initialized || !config.restarted_from ||
        !config.zoom_level || !config.sotred_zoom_level ||
        config.restarted_from == config.zoom_level ||
        config.restarted_from == config.sotred_zoom_level ||
        config.zoom_level == config.sotred_zoom_level ||
        (config.notice_capacity && !config.notices))
        return ab_fail(error, "Invalid canonical common-game pause globals/storage");
    *actions = (AbClassicPauseActions){
        .config = config, .scaling_enabled = true, .initialized = true};
    return true;
}

bool ab_classic_pause_show(void *context, AbError *error) {
    AbClassicPauseActions *actions = context;
    if (!valid(actions, error))
        return false;
    actions->busy = true;
    *actions->config.restarted_from = text("pause menu");
    const char *const names[] = {"wood_rolling", "rock_rolling", "light_rolling"};
    bool okay = actions->config.audio != NULL;
    if (!okay)
        ab_fail(error, "Missing actual rolling-audio owner for showPauseMenu");
    for (size_t index = 0; okay && index < 3; ++index)
        okay =
            ab_audio_bank_stop(actions->config.audio, text(names[index]).string, error);
    if (okay) {
        const AbDataValue *id =
            ab_data_get(actions->config.event_ids, text("EID_ABOUT_MENU_OPENED"));
        if (!id || !actions->config.events ||
            actions->notice_count >= actions->config.notice_capacity) {
            okay = ab_fail(error, "Missing actual pause event ID/owner/notice storage");
        } else {
            AbClassicPauseNotice *notice =
                actions->config.notices + actions->notice_count++;
            notice->id = (AbDataEntry){text("id"), *id};
            notice->table = (AbDataTable){&notice->id, 1, 1, NULL};
            okay =
                ab_event_manager_notify(actions->config.events, &notice->table, error);
        }
    }
    actions->busy = false;
    actions->failed = !okay;
    return okay;
}

bool ab_classic_pause_scaling(void *context, bool enabled, AbError *error) {
    AbClassicPauseActions *actions = context;
    if (!valid(actions, error))
        return false;
    if (actions->scaling_enabled && !enabled) {
        actions->stored_zoom = *actions->config.zoom_level;
        actions->scaling_enabled = false;
    } else if (enabled && !actions->scaling_enabled) {
        actions->scaling_enabled = true;
        *actions->config.zoom_level = actions->stored_zoom;
        *actions->config.sotred_zoom_level = (AbDataValue){0};
    }
    return true;
}
