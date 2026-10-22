#include "ab/episode_preferences.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(AbSpan name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = name};
}

static AbDataValue number(size_t index) {
    float value = (float)index;
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static AbDataValue get(const AbDataTable *table, AbDataValue key) {
    const AbDataValue *value = ab_data_get(table, key);
    return value ? *value : (AbDataValue){0};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    return get(table, text(span(name)));
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind == AB_DATA_NUMBER) {
        memcpy(output, &value.bits, sizeof(*output));
        if (isfinite(*output))
            return true;
    }
    ab_fail(error, "Episode preferences require a finite number");
    return false;
}

static bool table(AbDataValue value, const AbDataTable **output, AbError *error) {
    if (value.kind == AB_DATA_TABLE && value.table) {
        *output = value.table;
        return true;
    }
    ab_fail(error, "Episode preferences index a non-table value");
    return false;
}

static bool equal(AbDataValue first, AbDataValue second) {
    AbDataEntry entry = {first, {.kind = AB_DATA_BOOLEAN}};
    AbDataTable comparison = {.entries = &entry, .count = 1, .capacity = 1};
    return ab_data_get(&comparison, second) != NULL;
}

static bool valid(const AbEpisodePreferences *preferences, AbError *error) {
    return preferences && preferences->initialized && preferences->storage &&
                   preferences->episodes
               ? true
               : ab_fail(error, "Invalid episode preferences owner");
}

static const AbEpisodePreferenceKey *find_key(const AbEpisodePreferences *preferences,
                                              AbDataValue episode, AbError *error) {
    for (size_t index = 0; index < preferences->count; ++index)
        if (equal(preferences->keys[index].episode, episode))
            return preferences->keys + index;
    ab_fail(error, "Episode preference key is not in the bound catalog");
    return NULL;
}

static bool prepare_key(AbEpisodePreferenceKey *key, AbDataValue episode,
                        AbError *error) {
    AbSpan suffix;
    uint8_t bytes[32];
    if (episode.kind == AB_DATA_STRING) {
        suffix = episode.string;
    } else {
        float value;
        if (!numeric(episode, &value, error))
            return false;
        if (value < 1 || value > 16777216 || truncf(value) != value)
            return ab_fail(error, "Episode preferences require catalog numeric IDs");
        int count = snprintf((char *)bytes, sizeof(bytes), "%u", (unsigned)value);
        if (count < 1 || (size_t)count >= sizeof(bytes))
            return ab_fail(error, "Cannot format episode preference key");
        suffix = (AbSpan){bytes, (size_t)count};
    }
    if (suffix.size > 57 || (suffix.size && !suffix.data))
        return ab_fail(error, "Episode preference key exceeds storage");
    key->episode = episode;
    memcpy(key->bytes, "episode", 7);
    memcpy(key->bytes + 64, "pack", 4);
    if (suffix.size) {
        memcpy(key->bytes + 7, suffix.data, suffix.size);
        memcpy(key->bytes + 68, suffix.data, suffix.size);
    }
    key->episode_key = (AbSpan){key->bytes, 7 + suffix.size};
    key->pack_key = (AbSpan){key->bytes + 64, 4 + suffix.size};
    return true;
}

bool ab_episode_preferences_init(AbEpisodePreferences *preferences,
                                 AbSettingsStatistics *storage,
                                 const AbDataTable *episodes, AbError *error) {
    if (!preferences || preferences->initialized || !storage || !episodes ||
        !episodes->count || episodes->count > AB_EPISODE_PREFERENCES_CAPACITY ||
        !episodes->entries || !ab_settings_statistics_root(storage))
        return ab_fail(error, "Invalid episode preference initialization");
    for (size_t index = 0; index < episodes->count; ++index) {
        if (episodes->entries[index].value.kind != AB_DATA_TABLE ||
            !prepare_key(preferences->keys + index, episodes->entries[index].key,
                         error))
            return ab_fail(error, "Invalid episode preference catalog entry");
    }
    preferences->storage = storage;
    preferences->episodes = episodes;
    preferences->count = episodes->count;
    preferences->initialized = true;
    return true;
}

bool ab_episode_preferences_selected(const AbEpisodePreferences *preferences,
                                     AbDataValue *output, AbError *error) {
    if (!valid(preferences, error) || !output)
        return false;
    AbDataValue selected =
        field(ab_settings_statistics_root(preferences->storage), "selectedEpisode");
    *output = truthy(selected) ? selected : number(1);
    return true;
}

bool ab_episode_preferences_select(AbEpisodePreferences *preferences,
                                   AbDataValue episode, AbError *error) {
    AbSpan path = span("selectedEpisode");
    return valid(preferences, error) &&
           ab_settings_statistics_write(preferences->storage, &path, 1, episode, error);
}

static bool current_pages(AbEpisodePreferences *preferences, const AbDataTable **output,
                          AbError *error) {
    AbSpan key = span("currentLevelSelectionPages");
    AbDataValue pages =
        get(ab_settings_statistics_root(preferences->storage), text(key));
    if (pages.kind == AB_DATA_NIL) {
        if (!ab_settings_statistics_create_table(preferences->storage, &key, 1, 0,
                                                 error))
            return false;
        pages = get(ab_settings_statistics_root(preferences->storage), text(key));
    }
    return table(pages, output, error);
}

bool ab_episode_preferences_current_page(AbEpisodePreferences *preferences,
                                         AbDataValue episode, AbDataValue *output,
                                         AbError *error) {
    if (!valid(preferences, error) || !output)
        return false;
    const AbDataTable *pages;
    if (!current_pages(preferences, &pages, error))
        return false;
    const AbEpisodePreferenceKey *key = find_key(preferences, episode, error);
    if (!key)
        return false;
    AbDataValue value = get(pages, text(key->pack_key));
    *output = truthy(value) ? value : number(1);
    return true;
}

static bool page_record(const AbEpisodePreferences *preferences, AbDataValue episode,
                        AbDataValue page, const AbDataTable **output, AbError *error) {
    const AbDataTable *record, *pages;
    return table(get(preferences->episodes, episode), &record, error) &&
           table(field(record, "pages"), &pages, error) &&
           table(get(pages, page), output, error);
}

bool ab_episode_preferences_set_page(AbEpisodePreferences *preferences,
                                     AbDataValue episode, AbDataValue page,
                                     AbError *error) {
    if (!valid(preferences, error))
        return false;
    const AbDataTable *record, *pages;
    if (!page_record(preferences, episode, page, &record, error))
        return false;
    if (truthy(field(record, "do_not_restore_view_to_this_page")))
        return true;
    if (!current_pages(preferences, &pages, error))
        return false;
    const AbEpisodePreferenceKey *key = find_key(preferences, episode, error);
    if (!key)
        return false;
    AbSpan path[] = {span("currentLevelSelectionPages"), key->pack_key};
    return ab_settings_statistics_write(preferences->storage, path, 2, page, error);
}

bool ab_episode_preferences_visit_page(AbEpisodePreferences *preferences,
                                       AbDataValue episode, AbDataValue page,
                                       AbError *error) {
    if (!valid(preferences, error))
        return false;
    const AbEpisodePreferenceKey *key = find_key(preferences, episode, error);
    if (!key)
        return false;
    AbSpan path[] = {span("levelSelectionPagesVisited"), key->episode_key};
    AbDataValue root =
        get(ab_settings_statistics_root(preferences->storage), text(path[0]));
    const AbDataTable *pages = NULL, *visited = NULL;
    AbDataValue list = {0};
    if (truthy(root)) {
        if (!table(root, &pages, error))
            return false;
        list = get(pages, text(path[1]));
        if (truthy(list) && !table(list, &visited, error))
            return false;
    }
    const AbDataTable *record, *episode_pages;
    AbDataValue episode_value = get(preferences->episodes, episode);
    if (episode_value.kind == AB_DATA_NIL)
        return true;
    if (!table(episode_value, &record, error) ||
        !table(field(record, "pages"), &episode_pages, error))
        return false;
    AbDataValue page_value = get(episode_pages, page);
    if (page_value.kind == AB_DATA_NIL)
        return true;
    if (!table(page_value, &record, error))
        return false;
    if (truthy(field(record, "locked")))
        return true;
    for (size_t index = 1; visited && index <= visited->count; ++index) {
        AbDataValue value = get(visited, number(index));
        if (value.kind == AB_DATA_NIL)
            break;
        if (equal(value, page))
            return true;
    }
    if ((!truthy(root) && !ab_settings_statistics_create_table(preferences->storage,
                                                               path, 1, 0, error)) ||
        (!truthy(list) &&
         !ab_settings_statistics_create_table(preferences->storage, path, 2, 0, error)))
        return false;
    return ab_settings_statistics_append(preferences->storage, path, 2, page, error);
}

bool ab_episode_preferences_see_change(AbEpisodePreferences *preferences,
                                       AbDataValue episode, AbError *error) {
    if (!valid(preferences, error))
        return false;
    const AbDataTable *record, *versions = NULL;
    AbDataValue episode_value = get(preferences->episodes, episode);
    if (episode_value.kind == AB_DATA_NIL)
        return true;
    if (!table(episode_value, &record, error))
        return false;
    AbDataValue version = field(record, "new_indicated_episodechange_version");
    if (version.kind == AB_DATA_NIL)
        return true;
    const AbEpisodePreferenceKey *key = find_key(preferences, episode, error);
    if (!key)
        return false;
    AbSpan path[] = {span("newEpisodeChangeVersionsSeen"), key->episode_key};
    AbDataValue root =
        get(ab_settings_statistics_root(preferences->storage), text(path[0]));
    if (truthy(root)) {
        if (!table(root, &versions, error))
            return false;
        AbDataValue saved = get(versions, text(path[1]));
        if (saved.kind != AB_DATA_NIL) {
            float current, previous;
            if (!numeric(version, &current, error) || !numeric(saved, &previous, error))
                return false;
            if (previous >= current)
                return true;
        }
    } else if (!ab_settings_statistics_create_table(preferences->storage, path, 1, 0,
                                                    error)) {
        return false;
    }
    return ab_settings_statistics_write(preferences->storage, path, 2, version, error);
}
