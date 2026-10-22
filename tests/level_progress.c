#include "ab/level_progress.h"

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

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

typedef struct {
    AbDataEntry level_fields[4][6], page_levels[2][2], page_fields[2], pages[2],
        episode_field, episode, scores[2], score_fields[2], purchased_field;
    AbDataTable levels[4], level_arrays[2], page_tables[2], page_array, episode_table,
        episodes, highscores, score, purchased;
    AbLevelProgress progress;
    size_t purchase_queries;
    bool purchase_failure;
} Fixture;

static bool purchased(void *context, AbDataValue episode, const AbDataTable **output,
                      AbError *error) {
    Fixture *fixture = context;
    ++fixture->purchase_queries;
    if (episode.kind != AB_DATA_NUMBER || episode.bits != number(1).bits)
        return ab_fail(error, "Purchased-level query received wrong episode");
    if (fixture->purchase_failure)
        return ab_fail(error, "Controlled purchased-level service failure");
    *output = &fixture->purchased;
    return true;
}

static void initialize(Fixture *fixture) {
    static const char *const names[] = {"A", "B", "C", "D"};
    *fixture = (Fixture){0};
    for (size_t index = 0; index < 4; ++index) {
        size_t page = index / 2, offset = index % 2;
        AbDataEntry *fields = fixture->level_fields[index];
        fields[0] = (AbDataEntry){text("name"), text(names[index])};
        fields[1] = (AbDataEntry){text("episode"), number(1)};
        fields[2] = (AbDataEntry){text("page"), number(page + 1)};
        fields[3] = (AbDataEntry){text("index"), number(offset + 1)};
        fields[4] = (AbDataEntry){text("index_in_episode"), number(index + 1)};
        fields[5] = (AbDataEntry){text("unlocked"), {.kind = AB_DATA_BOOLEAN}};
        fixture->levels[index] = (AbDataTable){fields, 6, 6, NULL};
        fixture->page_levels[page][offset] =
            (AbDataEntry){number(offset + 1), table(fixture->levels + index)};
    }
    for (size_t page = 0; page < 2; ++page) {
        fixture->level_arrays[page] =
            (AbDataTable){fixture->page_levels[page], 2, 2, NULL};
        fixture->page_fields[page] =
            (AbDataEntry){text("levels"), table(fixture->level_arrays + page)};
        fixture->page_tables[page] =
            (AbDataTable){fixture->page_fields + page, 1, 1, NULL};
        fixture->pages[page] =
            (AbDataEntry){number(page + 1), table(fixture->page_tables + page)};
    }
    fixture->page_array = (AbDataTable){fixture->pages, 2, 2, NULL};
    fixture->episode_field = (AbDataEntry){text("pages"), table(&fixture->page_array)};
    fixture->episode_table = (AbDataTable){&fixture->episode_field, 1, 1, NULL};
    fixture->episode = (AbDataEntry){number(1), table(&fixture->episode_table)};
    fixture->episodes = (AbDataTable){&fixture->episode, 1, 1, NULL};
    fixture->score_fields[0] =
        (AbDataEntry){text("completed"), {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    fixture->score_fields[1] =
        (AbDataEntry){text("cleared"), {.kind = AB_DATA_BOOLEAN}};
    fixture->score = (AbDataTable){fixture->score_fields, 2, 2, NULL};
    fixture->scores[0] = (AbDataEntry){text("A"), table(&fixture->score)};
    fixture->scores[1] = (AbDataEntry){text("B"), table(&fixture->score)};
    fixture->highscores = (AbDataTable){fixture->scores, 0, 2, NULL};
    fixture->purchased_field = (AbDataEntry){number(1), text("B")};
    fixture->purchased = (AbDataTable){&fixture->purchased_field, 1, 1, NULL};
    fixture->progress = (AbLevelProgress){.episodes = &fixture->episodes,
                                          .highscores = &fixture->highscores,
                                          .release_build = true,
                                          .context = fixture,
                                          .purchased_levels = purchased};
}

static bool check(Fixture *fixture, size_t index, bool previous, bool expected,
                  AbError *error) {
    bool output = !expected;
    bool okay =
        previous ? ab_level_progress_previous(&fixture->progress,
                                              fixture->levels + index, &output, error)
                 : ab_level_progress_grid_open(&fixture->progress,
                                               fixture->levels + index, &output, error);
    return (okay && output == expected) ||
           (okay ? ab_fail(error, "Level gate differs at fixture %zu", index) : false);
}

static bool gates(Fixture *fixture, AbError *error) {
    if (!check(fixture, 0, true, false, error) ||
        !check(fixture, 0, false, true, error) ||
        !check(fixture, 1, false, false, error) ||
        !check(fixture, 2, false, false, error))
        return false;
    fixture->highscores.count = 1;
    if (!check(fixture, 1, false, true, error) ||
        !check(fixture, 2, false, false, error))
        return false;
    fixture->highscores.count = 2;
    if (!check(fixture, 2, true, true, error) ||
        !check(fixture, 3, false, false, error))
        return false;
    fixture->score_fields[0].value = number(0);
    if (!check(fixture, 1, false, true, error))
        return false;
    fixture->score_fields[0].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    fixture->level_fields[1][5].value = number(0);
    if (!check(fixture, 1, false, true, error))
        return false;
    fixture->level_fields[1][5].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    fixture->progress.release_build = false;
    if (!check(fixture, 1, false, true, error))
        return false;
    fixture->scores[0].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    bool output = true;
    if (ab_level_progress_grid_open(&fixture->progress, fixture->levels + 1, &output,
                                    error) ||
        !output)
        return ab_fail(error, "Debug gate bypassed the reached invalid highscore");
    return true;
}

static bool china(Fixture *fixture, AbError *error) {
    initialize(fixture);
    fixture->progress.freemium_china = true;
    fixture->highscores.count = 2;
    if (!check(fixture, 1, true, false, error))
        return false;
    fixture->score_fields[1].value = number(0);
    if (!check(fixture, 1, true, true, error) ||
        !check(fixture, 0, false, false, error) ||
        !check(fixture, 1, false, true, error) || fixture->purchase_queries != 2)
        return false;
    fixture->purchase_failure = true;
    bool output = true;
    if (ab_level_progress_grid_open(&fixture->progress, fixture->levels + 1, &output,
                                    error) ||
        !output || fixture->purchase_queries != 3)
        return ab_fail(error, "Purchase failure did not preserve output/call prefix");
    fixture->progress.purchased_levels = NULL;
    if (ab_level_progress_grid_open(&fixture->progress, fixture->levels + 1, &output,
                                    error))
        return ab_fail(error, "China gate invented an absent purchase response");
    return true;
}

int main(void) {
    Fixture fixture;
    AbError error = {0};
    initialize(&fixture);
    if (!gates(&fixture, &error) || !china(&fixture, &error)) {
        fprintf(stderr, "%s\n", error.message);
        return 1;
    }
    puts("Level predecessor/page boundary, release/unlocked gates, source truthiness "
         "and reached China purchase service pass.");
    return 0;
}
