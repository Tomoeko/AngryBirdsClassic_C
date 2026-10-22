#ifndef AB_EPISODE_PREFERENCES_CASES_H
#define AB_EPISODE_PREFERENCES_CASES_H

#include "ab/episode_preferences.h"

static bool preference_round_trip(AbEpisodePreferences *preferences, AbError *error) {
    AbDataValue value;
    if (!ab_episode_preferences_selected(preferences, &value, error) ||
        value.kind != AB_DATA_NUMBER || value.bits != number(1).bits ||
        !ab_episode_preferences_select(preferences, number(0), error) ||
        !ab_episode_preferences_selected(preferences, &value, error) ||
        value.kind != AB_DATA_NUMBER || value.bits != 0 ||
        !ab_episode_preferences_select(preferences, text("SURF"), error) ||
        !ab_episode_preferences_selected(preferences, &value, error) ||
        value.kind != AB_DATA_STRING || value.string.size != 4 ||
        memcmp(value.string.data, "SURF", 4) ||
        !ab_episode_preferences_current_page(preferences, number(1), &value, error) ||
        value.kind != AB_DATA_NUMBER || value.bits != number(1).bits ||
        !ab_episode_preferences_set_page(preferences, number(1), number(2), error) ||
        !ab_episode_preferences_current_page(preferences, number(1), &value, error) ||
        value.kind != AB_DATA_NUMBER || value.bits != number(2).bits ||
        !ab_episode_preferences_visit_page(preferences, number(1), number(2), error) ||
        !ab_episode_preferences_visit_page(preferences, number(1), number(2), error))
        return ab_fail(error, "Episode preference selection/page round trip differs");
    const AbDataTable *root = ab_settings_statistics_root(preferences->storage);
    const AbDataTable *visits = child(root, text("levelSelectionPagesVisited"));
    const AbDataTable *first = child(visits, text("episode1"));
    bool visited;
    AbEpisodeProgress progress = {.episodes = preferences->episodes,
                                  .local_settings = root};
    if (!first || first->count != 1 ||
        !ab_episode_progress_page_visited(&progress, number(1), number(2), &visited,
                                          error) ||
        !visited)
        return ab_fail(error, "Duplicate episode visit or live progress graph differs");
    AbSpan path[] = {text("levelSelectionPagesVisited").string, text("episode1").string,
                     text("bad").string};
    if (ab_settings_statistics_write(preferences->storage, path, 3, number(5), NULL) ||
        first->count != 1)
        return ab_fail(error,
                       "String mutation entered an owned numeric settings array");
    for (size_t index = 0; index < preferences->episodes->count; ++index) {
        const AbDataEntry *episode = preferences->episodes->entries + index;
        if (get(episode->value.table, text("new_indicated_episodechange_version"))
                .kind == AB_DATA_NIL)
            continue;
        bool seen;
        if (!ab_episode_progress_change_seen(&progress, episode->key, &seen, error) ||
            seen ||
            !ab_episode_preferences_see_change(preferences, episode->key, error) ||
            !ab_episode_progress_change_seen(&progress, episode->key, &seen, error) ||
            !seen ||
            !ab_episode_preferences_see_change(preferences, episode->key, error))
            return ab_fail(error, "Episode version mark/query round trip differs");
    }
    return true;
}

static bool preference_cases(const AbDataTable *episodes, AbError *error) {
    AbSettingsStatistics *storage = NULL;
    AbEpisodePreferences preferences = {0};
    bool okay =
        ab_settings_statistics_open((AbSettingsStatisticsConfig){text("fixture").string,
                                                                 text("fixture").string,
                                                                 64, 32, 1024 * 1024},
                                    &storage, error) &&
        ab_episode_preferences_init(&preferences, storage, episodes, error) &&
        preference_round_trip(&preferences, error);
    ab_settings_statistics_close(storage);
    return okay;
}

static bool preference_gates(AbError *error) {
    /* Controlled source fields exercise gates absent from an initial live save. */
    AbDataEntry fields[] = {{text("locked"), {.kind = AB_DATA_BOOLEAN, .bits = 1}},
                            {text("do_not_restore_view_to_this_page"),
                             {.kind = AB_DATA_BOOLEAN, .bits = 1}}};
    AbDataTable page = {.entries = fields, .count = 2, .capacity = 2};
    AbDataEntry page_entry = {number(1), {.kind = AB_DATA_TABLE, .table = &page}};
    AbDataTable pages = {.entries = &page_entry, .count = 1, .capacity = 1};
    AbDataEntry record_fields[] = {
        {text("pages"), {.kind = AB_DATA_TABLE, .table = &pages}},
        {text("new_indicated_episodechange_version"), number(3)}};
    AbDataTable record = {.entries = record_fields, .count = 2, .capacity = 2};
    AbDataEntry entry = {number(1), {.kind = AB_DATA_TABLE, .table = &record}};
    AbDataTable episodes = {.entries = &entry, .count = 1, .capacity = 1};
    AbSettingsStatistics *storage = NULL;
    AbEpisodePreferences preferences = {0};
    AbDataValue result;
    bool okay = false;
    if (!ab_settings_statistics_open(
            (AbSettingsStatisticsConfig){text("fixture").string, text("fixture").string,
                                         16, 8, 128 * 1024},
            &storage, error) ||
        !ab_episode_preferences_init(&preferences, storage, &episodes, error) ||
        !ab_episode_preferences_visit_page(&preferences, number(1), number(1), error) ||
        !ab_episode_preferences_set_page(&preferences, number(1), number(1), error) ||
        ab_settings_statistics_root(storage)->count)
        goto release;
    fields[0].value.bits = fields[1].value.bits = 0;
    AbSpan root = text("levelSelectionPagesVisited").string;
    if (!ab_settings_statistics_write(storage, &root, 1,
                                      (AbDataValue){.kind = AB_DATA_BOOLEAN}, error) ||
        !ab_episode_preferences_visit_page(&preferences, number(1), number(1), error))
        goto release;
    root = text("currentLevelSelectionPages").string;
    if (!ab_settings_statistics_write(storage, &root, 1,
                                      (AbDataValue){.kind = AB_DATA_BOOLEAN}, error) ||
        ab_episode_preferences_current_page(&preferences, number(1), &result, NULL) ||
        get(ab_settings_statistics_root(storage), text("currentLevelSelectionPages"))
                .kind != AB_DATA_BOOLEAN)
        goto release;
    AbSpan version[] = {text("newEpisodeChangeVersionsSeen").string,
                        text("episode1").string};
    if (!ab_settings_statistics_write(storage, version, 2, number(5), error) ||
        !ab_episode_preferences_see_change(&preferences, number(1), error))
        goto release;
    result = get(child(ab_settings_statistics_root(storage),
                       text("newEpisodeChangeVersionsSeen")),
                 text("episode1"));
    okay = result.kind == AB_DATA_NUMBER && result.bits == number(5).bits;
release:
    if (!okay && (!error || !error->message[0]))
        ab_fail(error, "Episode preference locked/nil/version gates differ");
    ab_settings_statistics_close(storage);
    return okay;
}

#endif
