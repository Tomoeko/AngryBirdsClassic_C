#include "ab/classic_level_finish.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataEntry entry;
    AbDataTable table;
} FinishEvent;

struct AbClassicLevelFinish {
    AbClassicLevelFinishConfig config;
    AbClassicLevelView level;
    FinishEvent *events;
    AbDataValue selected_audio;
    size_t used, bytes;
    bool calling, failed;
};

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static bool current_bird(AbClassicLevelFinish *finish, AbError *error) {
    AbClassicLevelFinishConfig *config = &finish->config;
    AbShotState *shot = ab_classic_shots_state(config->shots);
    AbLevelCompleteState *state = &config->state->complete;
    state->current_bird_index = ab_classic_sling_view(config->sling).current_bird_index;
    state->birds_shot = shot->birds_shot;
    state->current_bird_present = shot->current_bird_name != NULL;
    state->current_bird_shot_literal_true = false;
    if (!shot->current_bird_name)
        return true;
    for (size_t index = 0; index < finish->level.bodies->count; ++index) {
        AbSpan name = finish->level.bodies->objects[index].name;
        if (name.size != strlen(shot->current_bird_name) ||
            memcmp(name.data, shot->current_bird_name, name.size))
            continue;
        if (finish->level.removed_objects[index])
            return ab_fail(error, "Removed completion current bird");
        const AbDataValue *value = ab_data_get(
            &finish->level.properties->objects[index].physical, text("shot"));
        state->current_bird_shot_literal_true =
            value && value->kind == AB_DATA_BOOLEAN && value->bits;
        return true;
    }
    return ab_fail(error, "Unknown completion current bird");
}

static bool host(void *context, AbLevelCompleteCall *call, AbError *error) {
    AbClassicLevelFinish *finish = context;
    AbClassicLevelFinishConfig *config = &finish->config;
    switch (call->operation) {
        case AB_LEVEL_COMPLETE_NOTIFY: {
            if (finish->used == config->notifications)
                return ab_fail(error, "Completion notification capacity exhausted");
            FinishEvent *event = finish->events + finish->used++;
            event->entry = (AbDataEntry){text("id"), text("EID_LEVEL_COMPLETE_INIT")};
            event->table = (AbDataTable){&event->entry, 1, 1, NULL};
            return ab_event_manager_notify(config->events, &event->table, error);
        }
        case AB_LEVEL_COMPLETE_CANCEL_DRAG:
            if (!ab_classic_sling_cancel_drag(config->sling, error) ||
                !ab_classic_shots_refresh_globals(config->shots, error))
                return false;
            /* The next source assignment precedes the current-bird table
             * reads. Publish it before resolving the helper's metadata view. */
            config->state->complete.completed = true;
            return current_bird(finish, error);
        case AB_LEVEL_COMPLETE_QUERY_BIRD: {
            size_t object;
            if (!ab_classic_sling_next_bird(config->sling, call->bird_index, &object,
                                            error))
                return false;
            call->bird_present = object != SIZE_MAX;
            return true;
        }
        case AB_LEVEL_COMPLETE_CAMERA:
            return ab_classic_camera_start_sequence(config->camera, "LevelEndSequence",
                                                    error);
        case AB_LEVEL_COMPLETE_RESOLVE_AUDIO:
            return ab_audio_bank_name_value(config->audio, *config->clear_sound,
                                            &finish->selected_audio, error);
        case AB_LEVEL_COMPLETE_PLAY_AUDIO: {
            uint32_t handle;
            return ab_audio_bank_play(
                config->audio,
                (AbAudioPlay){
                    .name = finish->selected_audio,
                    .volume = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)},
                    .loop = {.kind = AB_DATA_BOOLEAN}},
                &handle, error);
        }
        case AB_LEVEL_COMPLETE_SCRIPT:
            return config->level_complete &&
                   config->level_complete(config->context, error);
    }
    return ab_fail(error, "Unknown owned completion operation");
}

bool ab_classic_level_finish_open(AbClassicLevelFinishConfig config,
                                  AbClassicLevelFinish **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing owned level-finish output");
    *output = NULL;
    AbClassicLevelView level =
        ab_classic_level_view(ab_classic_shots_level(config.shots));
    if (!level.bodies || !level.properties || !config.sling || !config.camera ||
        !config.audio || !config.events || !config.state || !config.clear_sound ||
        !config.failed_sound || config.storage_bytes < sizeof(AbClassicLevelFinish) ||
        config.notifications >
            (config.storage_bytes - sizeof(AbClassicLevelFinish)) / sizeof(FinishEvent))
        return ab_fail(error, "Invalid owned level-finish bindings");
    AbClassicLevelFinish *finish = calloc(1, sizeof(*finish));
    if (!finish)
        return ab_fail(error, "Cannot allocate owned level finish");
    finish->config = config;
    finish->level = level;
    finish->bytes = sizeof(*finish) + config.notifications * sizeof(FinishEvent);
    if (config.notifications)
        finish->events = calloc(config.notifications, sizeof(*finish->events));
    if (config.notifications && !finish->events) {
        ab_classic_level_finish_close(finish);
        return ab_fail(error, "Cannot allocate completion notifications");
    }
    *output = finish;
    return true;
}

void ab_classic_level_finish_close(AbClassicLevelFinish *finish) {
    if (finish) {
        free(finish->events);
        free(finish);
    }
}

size_t ab_classic_level_finish_bytes(const AbClassicLevelFinish *finish) {
    return finish ? finish->bytes : 0;
}

static bool enter(AbClassicLevelFinish *finish, AbError *error) {
    if (!finish || finish->calling || finish->failed || finish->level.world->failed ||
        finish->level.world->locked)
        return ab_fail(error, "Invalid, failed or reentrant owned level finish");
    finish->calling = true;
    return true;
}

bool ab_classic_level_finish_complete(AbClassicLevelFinish *finish, AbError *error) {
    if (!enter(finish, error))
        return false;
    AbClassicLevelFinishConfig *config = &finish->config;
    config->state->complete.has_level_hook = config->level_complete != NULL;
    bool okay = ab_level_complete_begin(&config->state->complete, host, finish, error);
    finish->failed = !okay;
    finish->calling = false;
    return okay;
}

static bool fail_level(AbClassicLevelFinish *finish, float seconds, AbError *error) {
    AbClassicLevelFinishConfig *config = &finish->config;
    AbClassicLevelFinishState *state = config->state;
    float timer = state->failed_timer + seconds;
    if (!isfinite(state->failed_timer) || !isfinite(seconds) || !isfinite(timer))
        return ab_fail(error, "Unsupported level-failed timer arithmetic");
    state->failed_timer = timer;
    if (timer <= 1.5f || state->complete.eagle_launched)
        return true;
    state->complete.ending_timer = 0.5f;
    state->complete.completing = true;
    AbDataValue selected;
    if (!ab_audio_bank_name_value(config->audio, *config->failed_sound, &selected,
                                  error))
        return false;
    if (selected.kind != AB_DATA_STRING)
        return ab_fail(error, "Unsupported direct level-failed audio name");
    uint32_t handle;
    return ab_audio_bank_play_resource(config->audio, selected.string, 1, false, 0,
                                       &handle, error);
}

bool ab_classic_level_finish_failed(AbClassicLevelFinish *finish, float seconds,
                                    AbError *error) {
    if (!enter(finish, error))
        return false;
    bool okay = fail_level(finish, seconds, error);
    finish->failed = !okay;
    finish->calling = false;
    return okay;
}
