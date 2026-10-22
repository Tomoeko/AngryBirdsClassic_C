#include "ab/settings_golden_eggs.h"

#include <string.h>

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue get(const AbDataTable *table, AbSpan name) {
    const AbDataValue *found =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = name});
    return found ? *found : (AbDataValue){0};
}

bool ab_settings_golden_egg(AbSettingsStatistics *highscores, AbSpan name,
                            AbDataValue *output, AbError *error) {
    const AbDataTable *root = ab_settings_statistics_root(highscores);
    if (!root || !output || (name.size && !name.data))
        return ab_fail(error, "Invalid golden-egg highscore binding");
    AbSpan path[] = {span("openGoldenEggLevels"), name, span("unlocked")};
    AbDataValue rows = get(root, path[0]);
    if (rows.kind == AB_DATA_NIL) {
        if (!ab_settings_statistics_create_table(highscores, path, 1, 0, error))
            return false;
        rows = get(root, path[0]);
    }
    if (rows.kind != AB_DATA_TABLE || !rows.table)
        return ab_fail(error, "Golden-egg lookup indexes non-table progress");
    AbDataValue row = get(rows.table, name);
    if (row.kind == AB_DATA_NIL) {
        AbDataValue disabled = {.kind = AB_DATA_BOOLEAN};
        if (!ab_settings_statistics_create_table(highscores, path, 2, 2, error) ||
            !ab_settings_statistics_write(highscores, path, 3, disabled, error))
            return false;
        path[2] = span("opened");
        if (!ab_settings_statistics_write(highscores, path, 3, disabled, error))
            return false;
        row = get(rows.table, name);
    }
    *output = row;
    return true;
}

bool ab_settings_golden_egg_unlocked(AbSettingsStatistics *highscores, AbSpan name,
                                     AbDataValue *output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing golden-egg unlock query output");
    AbDataValue row = {0};
    if (!ab_settings_golden_egg(highscores, name, &row, error))
        return false;
    if (row.kind != AB_DATA_TABLE || !row.table)
        return ab_fail(error, "Golden-egg query indexes a non-table record");
    *output = get(row.table, span("unlocked"));
    return true;
}

bool ab_settings_golden_egg_unlock(AbSettingsStatistics *highscores, AbSpan name,
                                   AbError *error) {
    AbDataValue row = {0};
    if (!ab_settings_golden_egg(highscores, name, &row, error))
        return false;
    if (row.kind != AB_DATA_TABLE || !row.table)
        return ab_fail(error, "Golden-egg unlock indexes a non-table record");
    AbSpan path[] = {span("openGoldenEggLevels"), name, span("unlocked")};
    return ab_settings_statistics_write(
        highscores, path, 3, (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error);
}
