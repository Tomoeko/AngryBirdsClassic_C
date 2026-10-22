#include "ab/classic_level_prepare.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataTable table;
    AbDataEntry fields[9];
} PrepareNotice;

typedef struct {
    AbClassicLevelPrepare *owner;
    const AbDataTable *metadata;
    AbClassicLevelRequest resolved;
    PrepareNotice golden, started, preinit, init, loading;
    AbClassicLoaderCall load;
    AbClassicLoaderCall *call_items[2];
    AbClassicLoaderCalls calls;
    AbDataEntry item;
    AbDataTable items;
    char folder[1024], path[2048];
} PrepareRequest;

struct AbClassicLevelPrepare {
    AbClassicLevelPrepareConfig config;
    PrepareRequest *records;
    AbDataEntry group_fields[2];
    AbDataTable groups;
    size_t count;
    bool preparing, loading, failed;
};

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
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

static void put(PrepareNotice *notice, const char *name, AbDataValue value) {
    if (value.kind != AB_DATA_NIL)
        notice->fields[notice->table.count++] = (AbDataEntry){text(name), value};
}

static void notice(AbClassicLevelPrepare *owner, PrepareNotice *output,
                   const char *id) {
    output->table = (AbDataTable){output->fields, 0, 9, NULL};
    put(output, "id", field(owner->config.event_ids, id));
}

static bool notify(AbClassicLevelPrepare *owner, PrepareNotice *event, AbError *error) {
    return ab_event_manager_notify(owner->config.events, &event->table, error);
}

static bool copy_folder(PrepareRequest *request, AbError *error) {
    AbDataValue folder = field(request->resolved.page, "folder_name");
    if (folder.kind != AB_DATA_STRING || !folder.string.data ||
        folder.string.size >= sizeof(request->folder))
        return ab_fail(error, "Prepared level folder exceeds owned storage");
    memcpy(request->folder, folder.string.data, folder.string.size);
    request->folder[folder.string.size] = '/';
    request->owner->config.state->level_folder = (AbDataValue){
        .kind = AB_DATA_STRING,
        .string = {(const uint8_t *)request->folder, folder.string.size + 1}};
    return true;
}

static bool load_call(void *context, AbError *error) {
    PrepareRequest *request = context;
    AbClassicLevelPrepare *owner = request->owner;
    AbClassicLevelPrepareConfig *config = &owner->config;
    if (owner->failed || owner->preparing || owner->loading)
        return ab_fail(error, "Invalid/reentrant prepared level load");
    owner->loading = true;
    bool okay = config->release_assets
                    ? config->release_assets(config->context, &owner->groups, error)
                    : ab_fail(error, "Level load requires the real asset release host");
    AbDataValue before = field(request->metadata, "onBeforeLoadLevelInternal");
    if (okay && truthy(before))
        okay =
            config->before_load
                ? config->before_load(config->context, before, error)
                : ab_fail(error, "Level load requires its actual before-load method");
    if (okay) {
        AbDataValue folder = config->state->level_folder;
        AbDataValue name = config->state->level_name;
        if (folder.kind != AB_DATA_STRING || name.kind != AB_DATA_STRING ||
            (folder.string.size && !folder.string.data) ||
            (name.string.size && !name.string.data) ||
            folder.string.size > sizeof(request->path) ||
            name.string.size > sizeof(request->path) - folder.string.size) {
            okay = ab_fail(error, "Unsupported live prepared level path");
        } else {
            if (folder.string.size)
                memcpy(request->path, folder.string.data, folder.string.size);
            if (name.string.size)
                memcpy(request->path + folder.string.size, name.string.data,
                       name.string.size);
            AbDataValue path = {.kind = AB_DATA_STRING,
                                .string = {(const uint8_t *)request->path,
                                           folder.string.size + name.string.size}};
            okay =
                config->load_level_internal
                    ? config->load_level_internal(config->context, path, error)
                    : ab_fail(error, "Level load requires the selected mode factory");
        }
    }
    owner->loading = false;
    owner->failed = !okay;
    return okay;
}

static bool started(PrepareRequest *request, AbDataValue from, AbError *error) {
    AbClassicLevelPrepare *owner = request->owner;
    AbClassicLevelPrepareConfig *config = &owner->config;
    AbClassicLevelPrepareState *state = config->state;
    float stars = 0, attempt = 0;
    if (truthy(get(config->statistics->highscores, state->level_name))) {
        float page = 0;
        memcpy(&page, &request->resolved.page_number.bits, sizeof(page));
        if (!ab_episode_statistics_level_stars(
                config->statistics, request->resolved.episode_id, (size_t)page,
                request->resolved.level, &stars, error))
            return false;
    }
    PrepareNotice *event = &request->started;
    notice(owner, event, "EID_LEVEL_STARTED");
    put(event, "levelName", state->level_name);
    put(event, "currentWorldNumber", state->current_world_number);
    put(event, "currentLevelNumberInTheme", state->current_level_number_in_theme);
    put(event, "from", from);
    if (!ab_settings_statistics_level_started_count(
            config->settings, state->level_name.string, &attempt, error))
        return false;
    put(event, "attempt", number(attempt));
    put(event, "wasRestarted", (AbDataValue){.kind = AB_DATA_BOOLEAN});
    put(event, "previousStars", number(stars));
    if (!ab_settings_statistics_level_started_count(
            config->settings, state->level_name.string, &attempt, error))
        return false;
    put(event, "firstTime",
        (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = attempt == 1});
    return notify(owner, event, error);
}

static bool prepare(PrepareRequest *request, AbDataValue from, AbDataValue minimum,
                    AbError *error) {
    AbClassicLevelPrepare *owner = request->owner;
    AbClassicLevelPrepareConfig *config = &owner->config;
    AbClassicLevelPrepareState *state = config->state;
    *config->editing = *config->physics_enabled = false;
    if (!ab_classic_level_request_resolve(config->episodes, request->metadata,
                                          &request->resolved, error))
        return false;
    AbClassicLevelRequest *resolved = &request->resolved;
    state->current_level_number = number(resolved->current_level_number);
    state->current_level_number_in_theme =
        number(resolved->current_level_number_in_theme);
    state->current_theme_number = resolved->episode_id;
    state->current_world_number = resolved->world_number;
    state->current_page_number = resolved->page_number;
    if (!copy_folder(request, error))
        return false;
    state->level_name = resolved->name;
    if (!truthy(field(request->metadata, "doNotResetAttemptCounter")))
        state->number_of_attempts = number(1);
    state->current_episode = resolved->episode_id;
    if (!ab_settings_statistics_start_level(
            config->settings, resolved->name.string, &state->level_started_table,
            &state->current_level_started_count, error) ||
        !ab_episode_preferences_set_page(config->preferences,
                                         state->current_theme_number,
                                         state->current_page_number, error))
        return false;
    if (truthy(*config->giftbox_enabled) &&
        (!config->giftbox || config->giftbox->settings != config->settings ||
         !ab_giftbox_session_increase_attempt(config->giftbox, error)))
        return error && error->message[0]
                   ? false
                   : ab_fail(error,
                             "Level preparation requires the real giftbox service");
    bool golden = truthy(field(resolved->episode, "no_level_progression")) ||
                  truthy(field(resolved->page, "no_level_progression")) ||
                  truthy(field(resolved->page, "golden_egg_end_screen")) ||
                  truthy(field(resolved->page, "calculate_as_golden_egg_levels"));
    if (golden) {
        if (!config->golden_egg_played ||
            !config->golden_egg_played(config->context, state->level_name, error))
            return error && error->message[0]
                       ? false
                       : ab_fail(error,
                                 "Level preparation requires real golden-egg storage");
        notice(owner, &request->golden, "EID_GE_LEVEL_STARTED");
        put(&request->golden, "levelName", state->level_name);
        if (!notify(owner, &request->golden, error))
            return false;
    }
    if (!started(request, from, error))
        return false;
    notice(owner, &request->preinit, "EID_LEVEL_LOADING_PREINIT");
    notice(owner, &request->init, "EID_LEVEL_LOADING_INIT");
    if (!notify(owner, &request->preinit, error) ||
        !notify(owner, &request->init, error) ||
        (config->log_load_time &&
         !config->log_load_time(config->context, minimum, error)))
        return false;
    request->load = (AbClassicLoaderCall){request, load_call};
    request->call_items[0] = &request->load;
    request->calls = (AbClassicLoaderCalls){request->call_items, 2};
    request->item =
        (AbDataEntry){number(1), {.kind = AB_DATA_OBJECT, .object = &request->load}};
    request->items = (AbDataTable){&request->item, 1, 1, NULL};
    notice(owner, &request->loading, "EID_DO_LOADING");
    put(&request->loading, "items",
        (AbDataValue){.kind = AB_DATA_TABLE, .table = &request->items});
    put(&request->loading, "completion_call",
        (AbDataValue){.kind = AB_DATA_OBJECT, .object = config->start_level});
    put(&request->loading, "min_display_time", minimum);
    if (!notify(owner, &request->loading, error))
        return false;
    AbSpan last = {(const uint8_t *)"lastPlayedLevel", 15};
    return ab_settings_statistics_write(config->settings, &last, 1, state->level_name,
                                        error);
}

bool ab_classic_level_prepare_open(AbClassicLevelPrepareConfig config,
                                   AbClassicLevelPrepare **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing level-prepare owner output");
    *output = NULL;
    if (!config.episodes || !config.event_ids || !config.statistics ||
        !config.settings || !config.preferences || !config.events || !config.state ||
        !config.editing || !config.physics_enabled || !config.giftbox_enabled ||
        !config.start_level || !config.requests ||
        config.storage_bytes < sizeof(AbClassicLevelPrepare) ||
        config.requests > (config.storage_bytes - sizeof(AbClassicLevelPrepare)) /
                              sizeof(PrepareRequest))
        return ab_fail(error, "Invalid level-prepare configuration/storage");
    const char *const ids[] = {"EID_GE_LEVEL_STARTED", "EID_LEVEL_STARTED",
                               "EID_LEVEL_LOADING_PREINIT", "EID_LEVEL_LOADING_INIT",
                               "EID_DO_LOADING"};
    for (size_t index = 0; index < sizeof(ids) / sizeof(ids[0]); ++index)
        if (field(config.event_ids, ids[index]).kind == AB_DATA_NIL)
            return ab_fail(error, "Missing level-prepare source event definition");
    AbClassicLevelPrepare *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate level-prepare owner");
    owner->records = calloc(config.requests, sizeof(*owner->records));
    if (!owner->records) {
        free(owner);
        return ab_fail(error, "Cannot allocate retained level-prepare records");
    }
    owner->config = config;
    owner->group_fields[0] = (AbDataEntry){number(1), text("LEVELSELECTION")};
    owner->group_fields[1] = (AbDataEntry){number(2), text("BACKGROUNDS")};
    owner->groups = (AbDataTable){owner->group_fields, 2, 2, NULL};
    *output = owner;
    return true;
}

void ab_classic_level_prepare_close(AbClassicLevelPrepare *owner) {
    if (!owner)
        return;
    free(owner->records);
    free(owner);
}

bool ab_classic_level_prepare(AbClassicLevelPrepare *owner, const AbDataTable *metadata,
                              AbDataValue from, AbDataValue minimum, AbError *error) {
    if (!owner || owner->failed || owner->preparing || owner->loading || !metadata)
        return ab_fail(error, "Invalid/reentrant level preparation");
    if (owner->count == owner->config.requests) {
        owner->failed = true;
        return ab_fail(error, "Retained level-prepare request capacity exhausted");
    }
    owner->preparing = true;
    PrepareRequest *request = owner->records + owner->count++;
    request->owner = owner;
    request->metadata = metadata;
    bool okay = prepare(request, from, minimum, error);
    owner->preparing = false;
    owner->failed = !okay;
    return okay;
}

bool ab_classic_level_prepare_loader(const AbClassicLevelPrepare *owner,
                                     const AbDataTable *event,
                                     AbClassicLoaderRequest *output, AbError *error) {
    if (!owner || owner->failed || !event || !output)
        return ab_fail(error, "Invalid level-prepare Loader adaptation");
    for (size_t index = 0; index < owner->count; ++index) {
        PrepareRequest *request = owner->records + index;
        if (event != &request->loading.table)
            continue;
        *output = (AbClassicLoaderRequest){
            .id = field(event, "id"),
            .minimum_display_time = field(event, "min_display_time"),
            .items = &request->calls,
            .completion_call = owner->config.start_level};
        return true;
    }
    return ab_fail(error, "Loader event is not owned by level preparation");
}
