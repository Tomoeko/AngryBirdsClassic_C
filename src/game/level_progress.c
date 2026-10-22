#include "ab/level_progress.h"

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

static const AbDataValue *field(const AbDataTable *table, const char *name) {
    return ab_data_get(table, text(name));
}

static bool truthy(const AbDataValue *value) {
    return value && value->kind != AB_DATA_NIL &&
           (value->kind != AB_DATA_BOOLEAN || value->bits != 0);
}

static bool index_value(const AbDataValue *value, size_t *output, AbError *error) {
    if (!value || value->kind != AB_DATA_NUMBER)
        return ab_fail(error, "Missing numeric level progression metadata");
    float index;
    memcpy(&index, &value->bits, sizeof(index));
    if (!isfinite(index) || index < 0 || index > 16777216 || floorf(index) != index)
        return ab_fail(error, "Unsupported level progression index");
    *output = (size_t)index;
    return true;
}

static bool child(const AbDataTable *parent, AbDataValue key,
                  const AbDataTable **output, AbError *error) {
    const AbDataValue *value = ab_data_get(parent, key);
    if (!value || value->kind != AB_DATA_TABLE || !value->table)
        return ab_fail(error, "Missing level progression catalog table");
    *output = value->table;
    return true;
}

static bool dense(const AbDataTable *array, size_t *output, AbError *error) {
    if (!array || array->count > array->capacity || array->count > UINT32_C(16777216) ||
        (array->count && !array->entries))
        return ab_fail(error, "Invalid level progression array");
    for (size_t index = 1; index <= array->count; ++index)
        if (!ab_data_get(array, number(index)))
            return ab_fail(error, "Level progression requires a dense array");
    *output = array->count;
    return true;
}

static bool previous(const AbLevelProgress *progress, const AbDataTable *level,
                     bool *output, AbError *error) {
    size_t page_index = 0, index = 0;
    const AbDataValue *id = field(level, "episode");
    const AbDataTable *episode = NULL, *pages = NULL, *current = NULL, *levels = NULL;
    if (!id)
        return ab_fail(error, "Missing level progression episode identity");
    if (!index_value(field(level, "page"), &page_index, error) ||
        !index_value(field(level, "index"), &index, error))
        return false;
    if (!page_index || !index)
        return ab_fail(error, "Level progression requires positive page/index");
    if (!child(progress->episodes, *id, &episode, error) ||
        !child(episode, text("pages"), &pages, error) ||
        !child(pages, number(page_index), &current, error))
        return false;
    --index;
    if (!index) {
        --page_index;
        const AbDataValue *prior = ab_data_get(pages, number(page_index));
        if (!truthy(prior)) {
            *output = false;
            return true;
        }
        if (prior->kind != AB_DATA_TABLE || !prior->table)
            return ab_fail(error, "Invalid previous level page");
        current = prior->table;
        if (!child(current, text("levels"), &levels, error) ||
            !dense(levels, &index, error))
            return false;
    }
    if (!child(current, text("levels"), &levels, error))
        return false;
    const AbDataValue *prior = ab_data_get(levels, number(index));
    if (!truthy(prior)) {
        *output = false;
        return true;
    }
    if (prior->kind != AB_DATA_TABLE || !prior->table)
        return ab_fail(error, "Invalid preceding level record");
    const AbDataValue *name = field(prior->table, "name");
    if (!name || name->kind != AB_DATA_STRING)
        return ab_fail(error, "Missing preceding level name");
    const AbDataValue *score = ab_data_get(progress->highscores, *name);
    if (!score || score->kind == AB_DATA_NIL) {
        *output = false;
        return true;
    }
    if (score->kind != AB_DATA_TABLE || !score->table)
        return ab_fail(error, "Invalid preceding level highscore record");
    *output =
        truthy(field(score->table, progress->freemium_china ? "cleared" : "completed"));
    return true;
}

bool ab_level_progress_previous(const AbLevelProgress *progress,
                                const AbDataTable *level, bool *output,
                                AbError *error) {
    if (!progress || !progress->episodes || !progress->highscores || !level || !output)
        return ab_fail(error, "Missing level progression owner/input");
    bool result = false;
    if (!previous(progress, level, &result, error))
        return false;
    *output = result;
    return true;
}

bool ab_level_progress_grid_open(const AbLevelProgress *progress,
                                 const AbDataTable *level, bool *output,
                                 AbError *error) {
    if (!progress || !progress->episodes || !progress->highscores || !level || !output)
        return ab_fail(error, "Missing grid-level progression owner/input");
    size_t index = 0;
    if (!index_value(field(level, "index_in_episode"), &index, error))
        return false;
    bool open = (index == 1 && !progress->freemium_china) ||
                (index < 6 && progress->freemium_china) ||
                (truthy(field(level, "unlocked")) && !progress->freemium_china);
    if (!open) {
        if (!previous(progress, level, &open, error))
            return false;
        open = open || !progress->release_build;
    }
    if (open && progress->freemium_china) {
        const AbDataValue *id = field(level, "episode"), *name = field(level, "name");
        const AbDataTable *purchased = NULL;
        if (!id || !name || name->kind != AB_DATA_STRING || !progress->purchased_levels)
            return ab_fail(error, "Missing reached China purchased-level service");
        if (!progress->purchased_levels(progress->context, *id, &purchased, error))
            return false;
        size_t count = 0;
        if (!dense(purchased, &count, error))
            return false;
        open = false;
        for (size_t item = 1; item <= count; ++item) {
            const AbDataValue *value = ab_data_get(purchased, number(item));
            if (value->kind == AB_DATA_STRING &&
                value->string.size == name->string.size &&
                !memcmp(value->string.data, name->string.data, name->string.size)) {
                open = true;
                break;
            }
        }
    }
    *output = open;
    return true;
}
