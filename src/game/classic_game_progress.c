#include "ab/classic_game_progress.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataEntry entry;
    AbDataTable table;
} ProgressEvent;

struct AbClassicGameProgress {
    AbClassicGameProgressConfig config;
    ProgressEvent *events;
    size_t used, bytes;
    bool updating, failed;
};

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool cheat_completion(const AbClassicGameProgressConfig *config) {
    bool quad = config->quad_click->kind == AB_DATA_BOOLEAN && config->quad_click->bits;
    return (truthy(*config->key_c) || truthy(*config->key_gaming_a) || quad) &&
           truthy(*config->cheats_enabled);
}

static bool notify(AbClassicGameProgress *progress, AbError *error) {
    AbClassicGameProgressConfig *config = &progress->config;
    *config->level_will_end_triggered =
        (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    if (progress->used == config->notifications)
        return ab_fail(error, "Game progress notification capacity exhausted");
    ProgressEvent *event = progress->events + progress->used++;
    event->entry = (AbDataEntry){text("id"), text("EID_LEVEL_WILL_END")};
    event->table = (AbDataTable){&event->entry, 1, 1, NULL};
    return ab_event_manager_notify(config->events, &event->table, error);
}

static bool update(AbClassicGameProgress *progress, float seconds, AbError *error) {
    AbClassicGameProgressConfig *config = &progress->config;
    float timer = config->finish_state->complete.ending_timer;
    if (!isfinite(timer))
        return ab_fail(error, "Unsupported game progress ending timer");
    if (timer > 0)
        return ab_classic_level_ending_update(config->ending, seconds, error);
    bool win, lose;
    if (!ab_classic_character_services_win(config->conditions, &win, error))
        return false;
    bool announce = false;
    if (win || cheat_completion(config)) {
        if (!ab_classic_level_finish_complete(config->finish, error))
            return false;
        announce = true;
    }
    /* This is a second independent query after completion callbacks. Its
     * announcement assignment deliberately replaces the earlier win decision. */
    if (!ab_classic_character_services_lose(config->conditions, &lose, error))
        return false;
    if (lose) {
        announce = !truthy(*config->level_will_end_triggered);
        if (!ab_classic_level_finish_failed(config->finish, seconds, error))
            return false;
    }
    return !announce || notify(progress, error);
}

bool ab_classic_game_progress_open(AbClassicGameProgressConfig config,
                                   AbClassicGameProgress **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing concrete game progress output");
    *output = NULL;
    if (!config.finish || !config.ending || !config.finish_state ||
        !config.conditions || !config.conditions->initialized || !config.events ||
        !config.cheats_enabled || !config.key_c || !config.key_gaming_a ||
        !config.quad_click || !config.level_will_end_triggered ||
        config.storage_bytes < sizeof(AbClassicGameProgress) ||
        config.notifications > (config.storage_bytes - sizeof(AbClassicGameProgress)) /
                                   sizeof(ProgressEvent))
        return ab_fail(error, "Invalid concrete game progress bindings");
    AbClassicGameProgress *progress = calloc(1, sizeof(*progress));
    if (!progress)
        return ab_fail(error, "Cannot allocate concrete game progress");
    progress->config = config;
    progress->bytes = sizeof(*progress) + config.notifications * sizeof(ProgressEvent);
    if (config.notifications)
        progress->events = calloc(config.notifications, sizeof(*progress->events));
    if (config.notifications && !progress->events) {
        ab_classic_game_progress_close(progress);
        return ab_fail(error, "Cannot allocate game progress notifications");
    }
    *output = progress;
    return true;
}

void ab_classic_game_progress_close(AbClassicGameProgress *progress) {
    if (progress) {
        free(progress->events);
        free(progress);
    }
}

size_t ab_classic_game_progress_bytes(const AbClassicGameProgress *progress) {
    return progress ? progress->bytes : 0;
}

bool ab_classic_game_progress_update(AbClassicGameProgress *progress, float seconds,
                                     AbError *error) {
    if (!progress || progress->updating || progress->failed)
        return ab_fail(error, "Invalid, failed or reentrant concrete game progress");
    progress->updating = true;
    bool okay = update(progress, seconds, error);
    progress->updating = false;
    progress->failed = !okay;
    return okay;
}
