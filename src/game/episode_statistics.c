#include "ab/episode_statistics.h"

#include <math.h>
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

static bool truthy(const AbDataValue *value) {
    return value && value->kind != AB_DATA_NIL &&
           (value->kind != AB_DATA_BOOLEAN || value->bits != 0);
}

static bool named(const AbDataValue *value, const char *name) {
    return value && value->kind == AB_DATA_STRING &&
           value->string.size == strlen(name) && value->string.data &&
           !memcmp(value->string.data, name, value->string.size);
}

static const AbDataValue *field(const AbDataTable *table, const char *name) {
    return ab_data_get(table, text(name));
}

static bool numeric(const AbDataValue *value, float *output, AbError *error) {
    if (!value || value->kind != AB_DATA_NUMBER) {
        ab_fail(error, "Missing or nonnumeric episode statistics value");
        return false;
    }
    memcpy(output, &value->bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite episode statistics value");
}

static bool child(const AbDataTable *table, AbDataValue key, const AbDataTable **output,
                  AbError *error) {
    const AbDataValue *value = ab_data_get(table, key);
    if (!value || value->kind != AB_DATA_TABLE || !value->table) {
        ab_fail(error, "Missing episode statistics table");
        return false;
    }
    *output = value->table;
    return true;
}

static bool array(const AbDataTable *table, size_t *output, AbError *error) {
    if (!table || table->count > UINT32_C(16777216) ||
        (table->count && !table->entries)) {
        ab_fail(error, "Invalid episode statistics array");
        return false;
    }
    for (size_t index = 1; index <= table->count; ++index)
        if (!ab_data_get(table, number(index))) {
            ab_fail(error, "Episode statistics require a dense array");
            return false;
        }
    *output = table->count;
    return true;
}

static bool level_name(const AbDataTable *level, AbSpan *output, AbError *error) {
    const AbDataValue *name = field(level, "name");
    if (!name || name->kind != AB_DATA_STRING || !name->string.data) {
        ab_fail(error, "Missing episode statistics level name");
        return false;
    }
    *output = name->string;
    return true;
}

static bool highscore(const AbEpisodeStatistics *statistics, AbSpan name, bool nil_only,
                      const AbDataTable **output, AbError *error) {
    if (!statistics || !statistics->highscores) {
        ab_fail(error, "Missing live episode highscore table");
        return false;
    }
    const AbDataValue *value = ab_data_get(
        statistics->highscores, (AbDataValue){.kind = AB_DATA_STRING, .string = name});
    if (!value || value->kind == AB_DATA_NIL || (!nil_only && !truthy(value))) {
        *output = NULL;
        return true;
    }
    if (value->kind != AB_DATA_TABLE || !value->table) {
        ab_fail(error, "Malformed episode highscore record");
        return false;
    }
    *output = value->table;
    return true;
}

static bool egg_defender_stars(const AbEpisodeStatistics *statistics,
                               const AbDataTable *level, float *output,
                               AbError *error) {
    AbSpan name;
    const AbDataTable *record, *stars;
    if (!level_name(level, &name, error) ||
        !highscore(statistics, name, false, &record, error))
        return false;
    float earned = 0;
    if (record && truthy(field(record, "stars"))) {
        if (!child(record, text("stars"), &stars, error) ||
            stars->count > UINT32_C(16777216) || (stars->count && !stars->entries))
            return ab_fail(error, "Invalid Egg Defender saved star table");
        for (size_t index = 0; index < stars->count; ++index)
            if (truthy(&stars->entries[index].value))
                earned = earned + 1;
    }
    *output = earned;
    return true;
}

static bool ordinary_level_stars(const AbEpisodeStatistics *statistics,
                                 const AbDataTable *level, float *output,
                                 AbError *error) {
    AbSpan name;
    const AbDataTable *record;
    if (!level_name(level, &name, error) ||
        !highscore(statistics, name, false, &record, error))
        return false;
    float score, limit;
    float earned = 0;
    if (record && truthy(field(record, "score"))) {
        if (!numeric(field(record, "score"), &score, error))
            return false;
        if (score > 0 && truthy(field(record, "completed"))) {
            const AbDataTable *thresholds;
            if (!child(statistics->star_table,
                       (AbDataValue){.kind = AB_DATA_STRING, .string = name},
                       &thresholds, error) ||
                !numeric(field(thresholds, "goldScore"), &limit, error))
                return false;
            if (score >= limit)
                earned = 3;
            else {
                if (!numeric(field(thresholds, "silverScore"), &limit, error))
                    return false;
                earned = score >= limit ? 2.0f : 1.0f;
            }
        }
    }
    *output = earned;
    return true;
}

bool ab_episode_statistics_level_stars(const AbEpisodeStatistics *statistics,
                                       AbDataValue episode, size_t page,
                                       const AbDataTable *level, float *output,
                                       AbError *error) {
    const AbDataTable *record, *pages, *current;
    if (!statistics || !output || !page || !level ||
        !child(statistics->episodes, episode, &record, error) ||
        !child(record, text("pages"), &pages, error) ||
        !child(pages, number(page), &current, error))
        return ab_fail(error, "Invalid level-star query context");
    const AbDataValue *override = field(current, "star_count_logic_override");
    if (!truthy(override))
        return ordinary_level_stars(statistics, level, output, error);
    return named(override, "eggDefenderStarCountLogic")
               ? egg_defender_stars(statistics, level, output, error)
               : ab_fail(error, "Unsupported episode star-count override");
}

static bool golden_stars(const AbEpisodeStatistics *statistics, bool three,
                         float *earned, float *available, AbError *error) {
    float done = 0, total = 0;
    float weight = three ? 3.0f : 1.0f;
    size_t episodes;
    if (!array(statistics->episode_ids, &episodes, error))
        return false;
    for (size_t episode = 1; episode <= episodes; ++episode) {
        const AbDataValue *identifier =
            ab_data_get(statistics->episode_ids, number(episode));
        const AbDataTable *record, *pages;
        size_t count;
        if (!child(statistics->episodes, *identifier, &record, error) ||
            !child(record, text("pages"), &pages, error) ||
            !array(pages, &count, error))
            return false;
        for (size_t page = 1; page <= count; ++page) {
            const AbDataTable *current, *levels;
            if (!child(pages, number(page), &current, error))
                return false;
            if (!truthy(field(current, "calculate_as_golden_egg_levels")))
                continue;
            size_t level_count;
            if (!child(current, text("levels"), &levels, error) ||
                !array(levels, &level_count, error))
                return false;
            for (size_t index = 1; index <= level_count; ++index) {
                const AbDataTable *level, *score;
                AbSpan name;
                if (!child(levels, number(index), &level, error) ||
                    !level_name(level, &name, error) ||
                    !highscore(statistics, name, false, &score, error))
                    return false;
                if (score && truthy(field(score, "completed")))
                    done = done + weight;
                total = total + weight;
            }
        }
    }
    *earned = done;
    *available = total;
    return true;
}

bool ab_episode_statistics_complete(const AbEpisodeStatistics *statistics,
                                    AbDataValue episode, bool *output, AbError *error) {
    const AbDataTable *record, *pages;
    size_t count;
    if (!statistics || !output ||
        !child(statistics->episodes, episode, &record, error) ||
        !child(record, text("pages"), &pages, error) || !array(pages, &count, error))
        return ab_fail(error, "Invalid episode-completion query context");
    bool completed = true;
    for (size_t page = 1; page <= count; ++page) {
        const AbDataTable *current, *levels;
        size_t level_count;
        if (!child(pages, number(page), &current, error) ||
            !child(current, text("levels"), &levels, error) ||
            !array(levels, &level_count, error))
            return false;
        for (size_t index = 1; index <= level_count; ++index) {
            const AbDataTable *level, *score;
            if (!child(levels, number(index), &level, error))
                return false;
            /* Later unmarked scores are not read once an earlier one fails. */
            bool end = truthy(field(level, "episode_end"));
            if (completed || end) {
                AbSpan name;
                if (!level_name(level, &name, error) ||
                    !highscore(statistics, name, false, &score, error))
                    return false;
                bool finished = score && truthy(field(score, "completed"));
                completed = completed && finished;
                if (end) {
                    *output = completed;
                    return true;
                }
            }
        }
    }
    *output = false;
    return true;
}

bool ab_episode_statistics_stars(const AbEpisodeStatistics *statistics,
                                 AbDataValue episode, bool golden_three, float *earned,
                                 float *available, AbError *error) {
    const AbDataTable *record;
    if (!statistics || !earned || !available ||
        !child(statistics->episodes, episode, &record, error))
        return ab_fail(error, "Invalid episode-star query context");
    if (named(&episode, "G"))
        return golden_stars(statistics, golden_three, earned, available, error);
    float done = 0, total = 0;
    if (named(&episode, "RMFDemo")) {
        *earned = *available = 0;
        return true;
    }
    const AbDataTable *pages;
    size_t count;
    if (!child(record, text("pages"), &pages, error) || !array(pages, &count, error))
        return false;
    for (size_t page = 1; page <= count; ++page) {
        const AbDataTable *current, *levels;
        if (!child(pages, number(page), &current, error))
            return false;
        if (truthy(field(current, "ignoreInThreeStarCalculations")) ||
            truthy(field(current, "locked")))
            continue;
        size_t level_count;
        if (!child(current, text("levels"), &levels, error) ||
            !array(levels, &level_count, error))
            return false;
        total = total + (float)level_count * 3.0f;
        for (size_t index = 1; index <= level_count; ++index) {
            const AbDataTable *level, *score;
            AbSpan name;
            if (!child(levels, number(index), &level, error) ||
                !level_name(level, &name, error) ||
                !highscore(statistics, name, true, &score, error))
                return false;
            if (score && truthy(field(score, "completed"))) {
                float stars;
                if (!ab_episode_statistics_level_stars(statistics, episode, page, level,
                                                       &stars, error))
                    return false;
                done = done + stars;
            }
        }
    }
    if (!isfinite(done) || !isfinite(total))
        return ab_fail(error, "Episode-star totals overflow");
    *earned = done;
    *available = total;
    return true;
}

bool ab_episode_statistics_score(const AbEpisodeStatistics *statistics,
                                 AbDataValue episode, float *output, AbError *error) {
    const AbDataTable *record, *pages;
    size_t count;
    if (!statistics || !output ||
        !child(statistics->episodes, episode, &record, error) ||
        !child(record, text("pages"), &pages, error) || !array(pages, &count, error))
        return ab_fail(error, "Invalid episode-score query context");
    float total = 0;
    for (size_t page = 1; page <= count; ++page) {
        const AbDataTable *current, *levels;
        if (!child(pages, number(page), &current, error))
            return false;
        if (truthy(field(current, "disregard_in_episode_score")))
            continue;
        size_t level_count;
        if (!child(current, text("levels"), &levels, error) ||
            !array(levels, &level_count, error))
            return false;
        for (size_t index = 1; index <= level_count; ++index) {
            const AbDataTable *level, *score;
            AbSpan name;
            if (!child(levels, number(index), &level, error) ||
                !level_name(level, &name, error) ||
                !highscore(statistics, name, true, &score, error))
                return false;
            if (score && truthy(field(score, "completed"))) {
                float value;
                if (!numeric(field(score, "score"), &value, error))
                    return false;
                total = total + value;
            }
        }
    }
    if (!isfinite(total))
        return ab_fail(error, "Episode-score total overflow");
    *output = total;
    return true;
}

bool ab_episode_statistics_eagle_score(const AbEpisodeStatistics *statistics,
                                       AbSpan name, float *output, AbError *error) {
    if (!output || (name.size && !name.data))
        return ab_fail(error, "Invalid episode eagle-score query");
    const AbDataTable *record;
    if (!highscore(statistics, name, false, &record, error))
        return false;
    const AbDataValue *score = field(record, "eagleScore");
    if (!truthy(score)) {
        *output = -1;
        return true;
    }
    return numeric(score, output, error);
}

bool ab_episode_statistics_feathers(const AbEpisodeStatistics *statistics,
                                    AbDataValue episode, float *earned,
                                    float *available, AbError *error) {
    const AbDataTable *record, *pages;
    size_t count;
    if (!statistics || !earned || !available ||
        !child(statistics->episodes, episode, &record, error) ||
        !child(record, text("pages"), &pages, error) || !array(pages, &count, error))
        return ab_fail(error, "Invalid episode-feather query context");
    float done = 0, total = 0;
    for (size_t page = 1; page <= count; ++page) {
        const AbDataTable *current, *levels;
        if (!child(pages, number(page), &current, error))
            return false;
        if (truthy(field(record, "mighty_eagle_disabled")) ||
            truthy(field(current, "mighty_eagle_disabled")) ||
            truthy(field(current, "locked")))
            continue;
        size_t level_count;
        if (!child(current, text("levels"), &levels, error) ||
            !array(levels, &level_count, error))
            return false;
        total = total + (float)level_count;
        for (size_t index = 1; index <= level_count; ++index) {
            const AbDataTable *level;
            AbSpan name;
            float score;
            if (!child(levels, number(index), &level, error) ||
                !level_name(level, &name, error) ||
                !ab_episode_statistics_eagle_score(statistics, name, &score, error))
                return false;
            if (score >= 100)
                done = done + 1;
        }
    }
    *earned = done;
    *available = total;
    return true;
}

bool ab_episode_statistics_all_complete(const AbEpisodeStatistics *statistics,
                                        const AbDataTable *episode, bool *output,
                                        AbError *error) {
    const AbDataTable *pages;
    size_t count;
    if (!statistics || !output || !child(episode, text("pages"), &pages, error) ||
        !array(pages, &count, error))
        return ab_fail(error, "Invalid menu episode-completion context");
    for (size_t page = 1; page <= count; ++page) {
        const AbDataTable *current, *levels;
        size_t level_count;
        if (!child(pages, number(page), &current, error) ||
            !child(current, text("levels"), &levels, error) ||
            !array(levels, &level_count, error))
            return false;
        for (size_t index = 1; index <= level_count; ++index) {
            const AbDataTable *level, *score;
            AbSpan name;
            if (!child(levels, number(index), &level, error) ||
                !level_name(level, &name, error) ||
                !highscore(statistics, name, false, &score, error))
                return false;
            if (!score || !truthy(field(score, "completed"))) {
                *output = false;
                return true;
            }
        }
    }
    *output = true;
    return true;
}

bool ab_episode_statistics_all_feathers(const AbEpisodeStatistics *statistics,
                                        const AbDataTable *additional_episodes,
                                        float *earned, float *available,
                                        AbError *error) {
    if (!statistics || !earned || !available)
        return ab_fail(error, "Invalid all-episode feather context");
    const AbDataTable *groups[] = {statistics->episode_ids, additional_episodes};
    float done = 0, total = 0;
    for (size_t group = 0; group < 2; ++group) {
        size_t count;
        if (group && !groups[group])
            continue;
        if (!array(groups[group], &count, error))
            return false;
        for (size_t index = 1; index <= count; ++index) {
            const AbDataValue *id = ab_data_get(groups[group], number(index));
            const AbDataTable *episode;
            if (!child(statistics->episodes, *id, &episode, error))
                return false;
            if (truthy(field(episode, "mighty_eagle_disabled")))
                continue;
            float page_done, page_total;
            if (!ab_episode_statistics_feathers(statistics, *id, &page_done,
                                                &page_total, error))
                return false;
            done = done + page_done;
            total = total + page_total;
        }
    }
    if (!isfinite(done) || !isfinite(total))
        return ab_fail(error, "All-episode feather totals overflow");
    *earned = done;
    *available = total;
    return true;
}
