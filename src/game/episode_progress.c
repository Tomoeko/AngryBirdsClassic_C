#include "ab/episode_progress.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
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
    return get(table, text(name));
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
    ab_fail(error, "Episode progress requires a finite number");
    return false;
}

static bool table(AbDataValue value, const AbDataTable **output, AbError *error) {
    if (value.kind == AB_DATA_TABLE && value.table) {
        *output = value.table;
        return true;
    }
    ab_fail(error, "Episode progress indexes a non-table value");
    return false;
}

static bool episode_table(const AbEpisodeProgress *progress, AbDataValue episode,
                          const AbDataTable **output, AbError *error) {
    if (!progress || !progress->episodes) {
        ab_fail(error, "Missing episode progress catalog");
        return false;
    }
    AbDataValue value = get(progress->episodes, episode);
    *output = NULL;
    return value.kind == AB_DATA_NIL || table(value, output, error);
}

static bool episode_key(AbDataValue episode, uint8_t bytes[64], AbDataValue *output,
                        AbError *error) {
    memcpy(bytes, "episode", 7);
    size_t size;
    if (episode.kind == AB_DATA_STRING) {
        size = episode.string.size;
        if (size > 57 || (size && !episode.string.data))
            return ab_fail(error, "Episode settings key exceeds catalog bounds");
        if (size)
            memcpy(bytes + 7, episode.string.data, size);
    } else {
        float identifier;
        if (!numeric(episode, &identifier, error))
            return false;
        if (identifier < 1 || identifier > 16777216 || truncf(identifier) != identifier)
            return ab_fail(error, "Episode settings requires a catalog identifier");
        int length = snprintf((char *)bytes + 7, 57, "%u", (unsigned)identifier);
        if (length < 1 || length >= 57)
            return ab_fail(error, "Cannot format episode settings key");
        size = (size_t)length;
    }
    *output = (AbDataValue){.kind = AB_DATA_STRING, .string = {bytes, 7 + size}};
    return true;
}

static bool dense_count(const AbDataTable *values, size_t *output, AbError *error) {
    if (!values || values->count > UINT32_C(16777216) ||
        (values->count && !values->entries)) {
        ab_fail(error, "Invalid episode progress array");
        return false;
    }
    for (size_t index = 1; index <= values->count; ++index)
        if (get(values, number(index)).kind == AB_DATA_NIL) {
            ab_fail(error, "Episode progress requires a dense array");
            return false;
        }
    *output = values->count;
    return true;
}

bool ab_episode_progress_has_new_levels(const AbEpisodeProgress *progress,
                                        AbDataValue episode, bool *output,
                                        AbError *error) {
    const AbDataTable *record;
    if (!output || !episode_table(progress, episode, &record, error))
        return false;
    *output = record && field(record, "new_levels_on_page").kind != AB_DATA_NIL;
    return true;
}

bool ab_episode_progress_new_page(const AbEpisodeProgress *progress,
                                  AbDataValue episode, AbDataValue *output,
                                  AbError *error) {
    const AbDataTable *record, *pages;
    if (!output || !episode_table(progress, episode, &record, error))
        return false;
    AbDataValue value = record ? field(record, "new_levels_on_page") : (AbDataValue){0};
    if (value.kind != AB_DATA_NIL) {
        float page;
        size_t count;
        if (!table(field(record, "pages"), &pages, error) ||
            !dense_count(pages, &count, error) || !numeric(value, &page, error))
            return false;
        if (page > (float)count)
            value = (AbDataValue){0};
    }
    *output = value;
    return true;
}

bool ab_episode_progress_page_visited(const AbEpisodeProgress *progress,
                                      AbDataValue episode, AbDataValue page,
                                      bool *output, AbError *error) {
    uint8_t bytes[64];
    AbDataValue key;
    const AbDataTable *root, *visited;
    if (!progress || !progress->local_settings || !output ||
        !episode_key(episode, bytes, &key, error))
        return ab_fail(error, "Invalid visited-page settings query");
    AbDataValue value = field(progress->local_settings, "levelSelectionPagesVisited");
    if (value.kind != AB_DATA_NIL) {
        if (!table(value, &root, error))
            return false;
        value = get(root, key);
    }
    bool found = false;
    if (value.kind != AB_DATA_NIL) {
        size_t count;
        if (!table(value, &visited, error) || !dense_count(visited, &count, error))
            return false;
        for (size_t index = 1; index <= count; ++index) {
            AbDataValue stored = get(visited, number(index));
            /* Actual page identifiers are numbers/strings; ab_data_get supplies
             * their target equality, including the two binary32 zero signs. */
            AbDataEntry entry = {.key = stored, .value = {.kind = AB_DATA_BOOLEAN}};
            AbDataTable equality = {.entries = &entry, .count = 1, .capacity = 1};
            if (ab_data_get(&equality, page)) {
                found = true;
                break;
            }
        }
    }
    *output = found;
    return true;
}

bool ab_episode_progress_change_seen(const AbEpisodeProgress *progress,
                                     AbDataValue episode, bool *output,
                                     AbError *error) {
    uint8_t bytes[64];
    AbDataValue key;
    const AbDataTable *record, *seen;
    if (!progress || !progress->local_settings || !output ||
        !episode_key(episode, bytes, &key, error) ||
        !episode_table(progress, episode, &record, error))
        return ab_fail(error, "Invalid episode-change settings query");
    AbDataValue version = record ? field(record, "new_indicated_episodechange_version")
                                 : (AbDataValue){0};
    if (version.kind == AB_DATA_NIL) {
        *output = true;
        return true;
    }
    AbDataValue value = field(progress->local_settings, "newEpisodeChangeVersionsSeen");
    if (value.kind != AB_DATA_NIL) {
        if (!table(value, &seen, error))
            return false;
        value = get(seen, key);
    }
    bool result = false;
    if (value.kind != AB_DATA_NIL) {
        float saved, current;
        if (!numeric(value, &saved, error) || !numeric(version, &current, error))
            return false;
        result = saved >= current;
    }
    *output = result;
    return true;
}

bool ab_episode_progress_free_eagle(const AbEpisodeProgress *progress, bool *output,
                                    AbError *error) {
    if (!progress || !progress->globals || !progress->episodes || !output)
        return ab_fail(error, "Invalid free-eagle query");
    const AbDataTable *status;
    if (!table(field(progress->globals, "g_freeEagleUnlockStatus"), &status, error) ||
        (status->count && !status->entries))
        return ab_fail(error, "Invalid free-eagle status table");
    for (size_t index = 0; index < status->count; ++index) {
        const AbDataTable *record;
        if (status->entries[index].value.kind == AB_DATA_NIL)
            continue;
        if (!table(status->entries[index].value, &record, error))
            return false;
        if (truthy(field(record, "enabled"))) {
            *output = true;
            return true;
        }
    }
    AbDataValue current = field(progress->globals, "currentEpisode");
    if (truthy(current)) {
        const AbDataTable *episode;
        if (!table(get(progress->episodes, current), &episode, error))
            return false;
        AbDataValue power = field(episode, "mightyPower");
        if (truthy(power)) {
            const AbDataTable *record;
            if (!table(power, &record, error))
                return false;
            if (truthy(field(record, "free"))) {
                AbDataValue clicked = field(progress->globals, "g_eagleClickedFrom");
                AbDataEntry entry = {.key = text("MAIN_MENU")};
                AbDataTable equality = {.entries = &entry, .count = 1, .capacity = 1};
                if (!ab_data_get(&equality, clicked)) {
                    *output = true;
                    return true;
                }
            }
        }
    }
    *output = false;
    return true;
}

bool ab_episode_progress_show_eagle(const AbEpisodeProgress *progress, bool *output,
                                    AbError *error) {
    if (!progress || !progress->globals || !progress->episodes || !output)
        return ab_fail(error, "Invalid episode eagle-display query");
    bool enabled = truthy(field(progress->globals, "hasIaps"));
    if (!enabled) {
        enabled = truthy(field(progress->globals, "Sugotoku"));
        if (!enabled) {
            if (!progress->purchases)
                return ab_fail(error, "Missing live eagle purchase table");
            enabled = truthy(field(progress->purchases, "mightyEagleEnabled"));
            if (!enabled && !ab_episode_progress_free_eagle(progress, &enabled, error))
                return false;
        }
    }
    if (!enabled)
        enabled = truthy(field(progress->globals, "g_haveEagleScores"));
    if (!enabled && !ab_episode_progress_free_eagle(progress, &enabled, error))
        return false;
    *output = enabled && !truthy(field(progress->globals, "g_is_free_version"));
    return true;
}

bool ab_episode_progress_open(const AbEpisodeProgress *progress, AbDataValue episode,
                              bool *output, AbError *error) {
    if (!progress || !progress->globals || !output)
        return ab_fail(error, "Invalid episode-open query");
    AbDataEntry always_entries[] = {{number(1), {.kind = AB_DATA_BOOLEAN}},
                                    {text("G"), {.kind = AB_DATA_BOOLEAN}}};
    AbDataTable always = {.entries = always_entries, .count = 2, .capacity = 2};
    if (ab_data_get(&always, episode)) {
        *output = true;
        return true;
    }
    if (truthy(field(progress->globals, "g_registrationEnabled"))) {
        *output = truthy(field(progress->globals, "g_isRegistered"));
        return true;
    }
    if (truthy(field(progress->globals, "gamerService")) ||
        truthy(field(progress->globals, "marketplace")))
        return ab_fail(error, "Reached platform trial service is not bound");
    *output = true;
    return true;
}
