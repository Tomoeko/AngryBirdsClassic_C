#include "ab/episode_catalog.h"
#include "ab/episode_progress.h"
#include "ab/episode_statistics.h"
#include "ab/hash.h"
#include "ab/target_data.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static AbDataValue get(const AbDataTable *table, AbDataValue key) {
    const AbDataValue *value = ab_data_get(table, key);
    return value ? *value : (AbDataValue){0};
}

static const AbDataTable *child(const AbDataTable *table, AbDataValue key) {
    AbDataValue value = get(table, key);
    return value.kind == AB_DATA_TABLE ? value.table : NULL;
}

static bool check_queries(const AbEpisodeStatistics *statistics,
                          const AbEpisodeProgress *progress, bool populated,
                          AbError *error) {
    size_t count = statistics->episode_ids->count;
    for (size_t index = 1; index <= count; ++index) {
        AbDataValue id = get(statistics->episode_ids, number((float)index));
        float earned, total, score;
        bool complete, fresh, visited, seen;
        AbDataValue page;
        if (!ab_episode_statistics_stars(statistics, id, false, &earned, &total,
                                         error) ||
            earned != (populated ? total : 0) ||
            !ab_episode_statistics_feathers(statistics, id, &earned, &total, error) ||
            earned != (populated ? total : 0) ||
            !ab_episode_statistics_score(statistics, id, &score, error) ||
            (populated ? score <= 0 : score != 0) ||
            !ab_episode_statistics_complete(statistics, id, &complete, error) ||
            (!populated && complete) ||
            !ab_episode_progress_has_new_levels(progress, id, &fresh, error) ||
            !ab_episode_progress_new_page(progress, id, &page, error) ||
            !ab_episode_progress_page_visited(progress, id, number(1), &visited,
                                              error) ||
            visited || !ab_episode_progress_change_seen(progress, id, &seen, error))
            return ab_fail(error, "Catalog statistics/progress query differs at %zu",
                           index);
    }
    return true;
}

static bool scores(const AbEpisodeStatistics *statistics, AbDataTable *highscores,
                   AbDataEntry *entries, AbDataTable *records,
                   AbDataEntry fields[640][4], AbError *error) {
    static AbDataEntry egg_values[] = {
        {.key = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)},
         .value = {.kind = AB_DATA_BOOLEAN, .bits = 1}},
        {.key = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40000000)},
         .value = {.kind = AB_DATA_BOOLEAN, .bits = 1}},
        {.key = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40400000)},
         .value = {.kind = AB_DATA_BOOLEAN, .bits = 1}}};
    static AbDataTable egg_stars = {.entries = egg_values, .count = 3, .capacity = 3};
    size_t written = 0, levels_seen = 0;
    for (size_t episode = 0; episode < statistics->episodes->count; ++episode) {
        const AbDataTable *record = statistics->episodes->entries[episode].value.table;
        const AbDataTable *pages = child(record, text("pages"));
        for (size_t page = 1; pages && page <= pages->count; ++page) {
            const AbDataTable *current = child(pages, number((float)page));
            const AbDataTable *levels = child(current, text("levels"));
            for (size_t item = 1; levels && item <= levels->count; ++item) {
                const AbDataTable *level = child(levels, number((float)item));
                AbDataValue name = get(level, text("name"));
                ++levels_seen;
                if (ab_data_get(highscores, name))
                    continue;
                if (written == 640)
                    return ab_fail(error, "Excess catalog highscore test records");
                const AbDataTable *limits = child(statistics->star_table, name);
                AbDataValue score = get(limits, text("goldScore"));
                /* Controlled completion fixtures for non-star-limit modes only.
                 * These values never enter production game state or a save. */
                if (score.kind == AB_DATA_NIL)
                    score = number(1000);
                fields[written][0] = (AbDataEntry){text("score"), score};
                fields[written][1] = (AbDataEntry){
                    text("completed"), {.kind = AB_DATA_BOOLEAN, .bits = 1}};
                fields[written][2] = (AbDataEntry){text("eagleScore"), number(100)};
                fields[written][3] = (AbDataEntry){
                    text("stars"), {.kind = AB_DATA_TABLE, .table = &egg_stars}};
                records[written] = (AbDataTable){
                    .entries = fields[written], .count = 4, .capacity = 4};
                entries[written] = (AbDataEntry){
                    name, {.kind = AB_DATA_TABLE, .table = records + written}};
                highscores->count = ++written;
            }
        }
    }
    return levels_seen == 597 || ab_fail(error, "Wrong target level-entry count");
}

static bool first_level(const AbEpisodeStatistics *statistics, AbDataTable *highscores,
                        AbError *error) {
    bool completed;
    if (!ab_episode_statistics_complete(statistics, number(1), &completed, error) ||
        !completed)
        return ab_fail(error, "Completed first episode did not reach its end marker");
    const AbDataTable *pages =
        child(child(statistics->episodes, number(1)), text("pages"));
    const AbDataTable *levels = child(child(pages, number(1)), text("levels"));
    const AbDataTable *level = child(levels, number(1));
    const AbDataTable *limits = child(statistics->star_table, text("Level1"));
    AbDataValue record_value = get(highscores, text("Level1"));
    AbDataTable *record = record_value.table;
    AbDataValue original = record->entries[0].value;
    AbDataValue samples[] = {number(0), number(1), get(limits, text("silverScore")),
                             get(limits, text("goldScore"))};
    for (size_t index = 0; index < 4; ++index) {
        record->entries[0].value = samples[index];
        float earned;
        if (!ab_episode_statistics_level_stars(statistics, number(1), 1, level, &earned,
                                               error) ||
            earned != index)
            return ab_fail(error, "Real Level1 threshold boundary differs");
    }
    record->entries[1].value.bits = 0;
    if (!ab_episode_statistics_complete(statistics, number(1), &completed, error) ||
        completed)
        return ab_fail(error, "Missing first level incorrectly completes the episode");
    float earned;
    if (!ab_episode_statistics_level_stars(statistics, number(1), 1, level, &earned,
                                           error) ||
        earned != 0)
        return ab_fail(error, "Incomplete level incorrectly earns stars");
    record->entries[0].value = original;
    record->entries[1].value.bits = 1;
    return true;
}

static bool progress_cases(AbEpisodeProgress *progress, AbError *error) {
    AbDataEntry visited_values[] = {{number(1), number(3)}};
    AbDataTable visited = {.entries = visited_values, .count = 1, .capacity = 1};
    AbDataEntry episode_entry = {text("episode1"),
                                 {.kind = AB_DATA_TABLE, .table = &visited}};
    AbDataTable episode_records = {
        .entries = &episode_entry, .count = 1, .capacity = 1};
    AbDataEntry local_entry = {text("levelSelectionPagesVisited"),
                               {.kind = AB_DATA_TABLE, .table = &episode_records}};
    AbDataTable locals = {.entries = &local_entry, .count = 1, .capacity = 1};
    const AbDataTable *previous = progress->local_settings;
    progress->local_settings = &locals;
    bool result;
    bool okay = ab_episode_progress_page_visited(progress, number(1), number(3),
                                                 &result, error) &&
                result &&
                ab_episode_progress_page_visited(progress, number(1), text("3"),
                                                 &result, error) &&
                !result;
    local_entry.value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    okay = okay && !ab_episode_progress_page_visited(progress, number(1), number(3),
                                                     &result, NULL);
    progress->local_settings = previous;
    return okay || ab_fail(error, "Visited-page equality/nil-only checks differ");
}

static bool eagle_cases(const AbDataTable *episodes, AbError *error) {
    AbDataTable empty = {0};
    AbDataEntry globals_entries[] = {
        {text("hasIaps"), {.kind = AB_DATA_BOOLEAN, .bits = 1}},
        {text("g_is_free_version"), {.kind = AB_DATA_BOOLEAN}},
        {text("g_freeEagleUnlockStatus"), {.kind = AB_DATA_TABLE, .table = &empty}},
        {text("g_haveEagleScores"), {.kind = AB_DATA_BOOLEAN}}};
    AbDataTable globals = {.entries = globals_entries, .count = 4, .capacity = 4};
    AbEpisodeProgress progress = {.episodes = episodes, .globals = &globals};
    bool result;
    if (!ab_episode_progress_show_eagle(&progress, &result, error) || !result)
        return ab_fail(error, "Eagle IAP short circuit differs");
    globals_entries[1].value.bits = 1;
    if (!ab_episode_progress_show_eagle(&progress, &result, error) || result)
        return ab_fail(error, "Free-edition eagle display gate differs");
    globals_entries[0].value.bits = globals_entries[1].value.bits = 0;
    progress.purchases = &empty;
    if (!ab_episode_progress_show_eagle(&progress, &result, error) || result)
        return ab_fail(error, "Empty eagle state incorrectly enables display");
    globals_entries[3].value.bits = 1;
    if (!ab_episode_progress_show_eagle(&progress, &result, error) || !result)
        return ab_fail(error, "Saved eagle scores do not enable display");
    /* isEagleEnabled reaches free state before the saved-score shortcut. */
    globals_entries[2].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    if (ab_episode_progress_show_eagle(&progress, &result, NULL))
        return ab_fail(error, "Eagle query skipped a reached malformed state");
    AbDataEntry purchase_entry = {text("mightyEagleEnabled"), number(0)};
    AbDataTable purchases = {.entries = &purchase_entry, .count = 1, .capacity = 1};
    progress.purchases = &purchases;
    if (!ab_episode_progress_show_eagle(&progress, &result, error) || !result)
        return ab_fail(error, "Truthy zero purchase state did not short circuit");
    return true;
}

#include "episode_preferences_cases.h"

static bool reward_completion(AbError *error) {
    AbDataEntry first_fields[] = {
        {text("name"), text("first")},
        {text("episode_end"), {.kind = AB_DATA_BOOLEAN, .bits = 1}}};
    AbDataEntry second_field = {text("name"), text("second")};
    AbDataTable first = {first_fields, 2, 2, NULL};
    AbDataTable second = {&second_field, 1, 1, NULL};
    AbDataEntry level_entries[] = {
        {number(1), {.kind = AB_DATA_TABLE, .table = &first}},
        {number(2), {.kind = AB_DATA_TABLE, .table = &second}}};
    AbDataTable levels = {level_entries, 2, 2, NULL};
    AbDataEntry level_field = {text("levels"),
                               {.kind = AB_DATA_TABLE, .table = &levels}};
    AbDataTable page = {&level_field, 1, 1, NULL};
    AbDataEntry page_entry = {number(1), {.kind = AB_DATA_TABLE, .table = &page}};
    AbDataTable pages = {&page_entry, 1, 1, NULL};
    AbDataEntry page_field = {text("pages"), {.kind = AB_DATA_TABLE, .table = &pages}};
    AbDataTable episode = {&page_field, 1, 1, NULL};
    AbDataEntry episode_entry = {number(1), {.kind = AB_DATA_TABLE, .table = &episode}};
    AbDataTable episodes = {&episode_entry, 1, 1, NULL};
    AbDataEntry completed = {text("completed"), {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    AbDataTable score = {&completed, 1, 1, NULL};
    AbDataEntry score_entry = {text("first"), {.kind = AB_DATA_TABLE, .table = &score}};
    AbDataTable highscores = {&score_entry, 1, 1, NULL};
    AbEpisodeStatistics statistics = {.episodes = &episodes, .highscores = &highscores};
    bool result;
    if (!ab_episode_statistics_complete(&statistics, number(1), &result, error) ||
        !result ||
        !ab_episode_statistics_all_complete(&statistics, &episode, &result, error) ||
        result)
        return ab_fail(error, "Reward completion stopped at the earlier end marker");
    pages.count = 0;
    return ab_episode_statistics_all_complete(&statistics, &episode, &result, error) &&
           result;
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    AbError error = {0};
    AbBuffer apk = {0};
    AbZip zip = {0};
    AbEpisodeCatalog *catalog = NULL;
    AbTargetData limits = {0};
    uint8_t key[32], digest[32];
    char hash[65];
    AbDataEntry entries[640], fields[640][4];
    AbDataTable records[640];
    AbDataTable highscores = {.entries = entries, .capacity = 640}, empty = {0};
    bool okay = false;
    if (!ab_file_read(argv[1], 64 * 1024 * 1024, &apk, &error) ||
        !ab_sha256((AbSpan){apk.data, apk.size}, digest, &error))
        goto release;
    ab_digest_hex(digest, hash);
    if (strcmp(hash,
               "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad") ||
        !ab_zip_open((AbSpan){apk.data, apk.size}, &zip, &error) ||
        !ab_target_resource_key(&zip, 16 * 1024 * 1024, key, &error) ||
        !ab_episode_catalog_open(
            &zip, (AbSpan){key, sizeof(key)},
            (AbEpisodeCatalogConfig){.device_model = text("android").string},
            2 * 1024 * 1024, &catalog, &error) ||
        !ab_target_data_open(&zip, "assets/data/scripts/starLimits.lua",
                             (AbSpan){key, sizeof(key)}, 3 * 1024 * 1024, NULL, &limits,
                             &error))
        goto release;
    AbEpisodeStatistics statistics = {.episodes = ab_episode_catalog_episodes(catalog),
                                      .episode_ids = ab_episode_catalog_ids(catalog),
                                      .highscores = &highscores,
                                      .star_table = limits.data.globals};
    AbEpisodeProgress progress = {.episodes = statistics.episodes,
                                  .local_settings = &empty};
    if (!check_queries(&statistics, &progress, false, &error) ||
        !scores(&statistics, &highscores, entries, records, fields, &error) ||
        !check_queries(&statistics, &progress, true, &error) ||
        !first_level(&statistics, &highscores, &error) ||
        !progress_cases(&progress, &error) ||
        !eagle_cases(statistics.episodes, &error) ||
        !preference_cases(statistics.episodes, &error) || !preference_gates(&error))
        goto release;
    float feathers, available;
    if (!reward_completion(&error) ||
        !ab_episode_statistics_all_feathers(&statistics,
                                            ab_episode_catalog_feather_ids(catalog),
                                            &feathers, &available, &error) ||
        feathers < 200 || feathers != available)
        goto release;
    puts("597 catalog entries: real star limits, empty/completed statistics, Egg "
         "Defender/Golden Egg modes and visited-page queries.");
    okay = true;
release:
    if (!okay)
        fprintf(stderr, "%s\n", error.message);
    ab_target_data_close(&limits);
    memset(key, 0, sizeof(key));
    ab_episode_catalog_close(catalog);
    ab_zip_close(&zip);
    ab_buffer_free(&apk);
    return okay ? 0 : 1;
}
