#include "ab/settings_statistics.h"
#include "settings_statistics_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Statistics check failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue value(uint32_t kind, uint32_t bits) {
    if (kind == 4)
        return (AbDataValue){.kind = AB_DATA_STRING, .string = span("bad")};
    return (AbDataValue){.kind = kind == 3 ? AB_DATA_NUMBER : (AbDataKind)kind,
                         .bits = bits};
}

static bool prepare(AbSettingsStatistics *statistics, uint32_t sample) {
    for (uint32_t counter = 0; counter < 2; ++counter) {
        uint32_t variant = counter ? sample / 8 : sample % 8;
        AbSpan path[] = {span(statistics_counters[counter]), span("device_fixture"),
                         span("installationId_fixture")};
        if (variant == 0)
            continue;
        if (variant < 4) {
            CHECK(ab_settings_statistics_write(statistics, path, 1,
                                               value(variant == 1   ? 1
                                                     : variant == 2 ? 3
                                                                    : 4,
                                                     0),
                                               NULL));
            continue;
        }
        CHECK(ab_settings_statistics_set_local(statistics, path[0], value(0, 0), NULL));
        if (variant < 7) {
            CHECK(ab_settings_statistics_write(statistics, path, 2,
                                               value(variant == 4   ? 0
                                                     : variant == 5 ? 1
                                                                    : 3,
                                                     0),
                                               NULL));
            continue;
        }
        uint32_t seed = statistics_seed_values[sample % 4];
        CHECK(ab_settings_statistics_set_local(statistics, path[0], value(3, seed),
                                               NULL));
        path[2] = span("installationId_prior_a");
        CHECK(ab_settings_statistics_write(statistics, path, 3, value(3, 0x4cbebc20),
                                           NULL));
        path[2] = span("installationId_prior_b");
        CHECK(ab_settings_statistics_write(statistics, path, 3, value(3, 0xccbebc20),
                                           NULL));
        path[2] = span("installationId_prior_c");
        CHECK(ab_settings_statistics_write(statistics, path, 3, value(3, 0x3f800000),
                                           NULL));
        path[1] = span("device_other");
        CHECK(ab_settings_statistics_write(statistics, path, 3, value(3, 0x447a0000),
                                           NULL));
    }
    return true;
}

static void result_words(bool okay, AbDataValue result, uint32_t output[3]) {
    output[0] = okay;
    output[1] = okay ? result.kind == AB_DATA_NUMBER   ? 3u
                       : result.kind == AB_DATA_STRING ? 4u
                                                       : (uint32_t)result.kind
                     : 0;
    output[2] = okay && result.kind == AB_DATA_NUMBER ? result.bits : 0;
}

static void query(const AbSettingsStatistics *statistics, AbSpan counter, bool sum,
                  uint32_t output[3]) {
    AbDataValue result = {0};
    bool okay;
    if (sum) {
        float number;
        okay = ab_settings_statistics_device_sum(statistics, counter, &number, NULL);
        result.kind = AB_DATA_NUMBER;
        if (okay)
            memcpy(&result.bits, &number, sizeof(number));
    } else {
        okay = ab_settings_statistics_get_local(statistics, counter, &result, NULL);
    }
    result_words(okay, result, output);
}

static void action(AbSettingsStatistics *statistics, StatisticsAction action,
                   uint32_t output[3]) {
    AbSpan counter = span(statistics_counters[action.counter]);
    if (action.operation == STATISTICS_GET_LOCAL ||
        action.operation == STATISTICS_GET_SUM) {
        query(statistics, counter, action.operation == STATISTICS_GET_SUM, output);
        return;
    }
    bool okay = action.operation == STATISTICS_SET_LOCAL
                    ? ab_settings_statistics_set_local(
                          statistics, counter, value(action.kind, action.bits), NULL)
                    : ab_settings_statistics_increment(statistics, counter, NULL);
    result_words(okay, (AbDataValue){0}, output);
}

static bool compare(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_SETTINGS_STATISTICS_V1 64 16\n"));
    for (uint32_t sample = 0; sample < STATISTICS_CASES; ++sample) {
        uint32_t expected_sample;
        CHECK(fscanf(file, "%8" SCNx32, &expected_sample) == 1 &&
              sample == expected_sample);
        AbSettingsStatistics *statistics = NULL;
        CHECK(ab_settings_statistics_open(
            (AbSettingsStatisticsConfig){span("fixture"), span("fixture"), 16, 16,
                                         256 * 1024},
            &statistics, NULL));
        CHECK(prepare(statistics, sample));
        for (uint32_t step = 0; step < STATISTICS_ACTIONS; ++step) {
            uint32_t actual[15] = {0}, expected[15];
            action(statistics, statistics_actions[step], actual);
            for (uint32_t counter = 0; counter < 2; ++counter) {
                query(statistics, span(statistics_counters[counter]), false,
                      actual + 3 + 6 * counter);
                query(statistics, span(statistics_counters[counter]), true,
                      actual + 6 + 6 * counter);
            }
            for (size_t word = 0; word < 15; ++word) {
                CHECK(fscanf(file, "%8" SCNx32, expected + word) == 1);
                if (actual[word] != expected[word]) {
                    fprintf(stderr,
                            "Statistics native mismatch sample %u step %u word %zu: "
                            "%08x != %08x\n",
                            sample, step, word, actual[word], expected[word]);
                    ab_settings_statistics_close(statistics);
                    return false;
                }
            }
        }
        ab_settings_statistics_close(statistics);
    }
    CHECK(fgetc(file) == '\n' && fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB settings statistics reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    puts("Owned statistics match 1024 original actions and 4096 live local/device "
         "queries, including malformed storage and binary32 installation sums.");
    return true;
}

static bool storage_guards(void) {
    AbSettingsStatistics *statistics = NULL;
    AbSettingsStatisticsConfig config = {span("fixture"), span("fixture"), 3, 2, 65536};
    CHECK(ab_settings_statistics_open(config, &statistics, NULL));
    CHECK(ab_settings_statistics_increment(statistics, span("birdsShooted"), NULL));
    size_t bytes = ab_settings_statistics_bytes(statistics);
    CHECK(bytes && ab_settings_statistics_root(statistics)->count == 1);
    CHECK(!ab_settings_statistics_increment(statistics, span("backwardsBirdCount"),
                                            NULL));
    /* The nil-only root creation failed before a leaf; the first counter lives. */
    float sum;
    CHECK(ab_settings_statistics_device_sum(statistics, span("birdsShooted"), &sum,
                                            NULL) &&
          sum == 1);
    ab_settings_statistics_close(statistics);
    config.storage_bytes = bytes - 1;
    CHECK(!ab_settings_statistics_open(config, &statistics, NULL) && !statistics);
    config.storage_bytes = bytes;
    CHECK(ab_settings_statistics_open(config, &statistics, NULL));
    ab_settings_statistics_close(statistics);
    return true;
}

static bool tutorial_guards(void) {
    AbSettingsStatistics *settings = NULL;
    CHECK(ab_settings_statistics_open(
        (AbSettingsStatisticsConfig){span("fixture"), span("fixture"), 8, 8, 65536},
        &settings, NULL));
    AbDataValue item = {.kind = AB_DATA_STRING, .string = span("bird")};
    CHECK(ab_settings_statistics_extra_tutorial(settings, item, NULL));
    const AbDataTable *root = ab_settings_statistics_root(settings);
    const AbDataValue *tutorials = ab_data_get(
        root, (AbDataValue){.kind = AB_DATA_STRING, .string = span("tutorials")});
    CHECK(tutorials && tutorials->kind == AB_DATA_TABLE && !tutorials->table->count);
    AbSpan path[] = {span("tutorials"), span("bird"), span("showHelp")};
    CHECK(ab_settings_statistics_write(settings, path, 2, value(1, 0), NULL) &&
          ab_settings_statistics_extra_tutorial(settings, item, NULL) &&
          ab_data_get(tutorials->table, item)->kind == AB_DATA_BOOLEAN &&
          !ab_data_get(tutorials->table, item)->bits &&
          ab_settings_statistics_write(settings, path, 2, value(3, 0), NULL) &&
          !ab_settings_statistics_extra_tutorial(settings, item, NULL) &&
          ab_data_get(tutorials->table, item)->kind == AB_DATA_NUMBER &&
          ab_settings_statistics_write(settings, path, 2, value(0, 0), NULL) &&
          ab_settings_statistics_write(settings, path, 3, value(3, 0), NULL));
    AbDataTable *record = ab_data_get(tutorials->table, item)->table;
    CHECK(ab_settings_statistics_extra_tutorial(settings, item, NULL) &&
          ab_data_get(tutorials->table, item)->table == record);
    const AbDataValue *show = ab_data_get(
        record, (AbDataValue){.kind = AB_DATA_STRING, .string = span("showHelp")});
    CHECK(show && show->kind == AB_DATA_BOOLEAN && !show->bits &&
          ab_settings_statistics_write(settings, path, 2, value(0, 0), NULL) &&
          ab_settings_statistics_extra_tutorial(settings, item, NULL) &&
          ab_data_get(tutorials->table, item)->kind == AB_DATA_NIL &&
          show->kind == AB_DATA_BOOLEAN && !show->bits &&
          ab_settings_statistics_write(settings, path, 1, value(1, 0), NULL) &&
          !ab_settings_statistics_extra_tutorial(settings, item, NULL) &&
          tutorials->kind == AB_DATA_BOOLEAN && !tutorials->bits);
    ab_settings_statistics_close(settings);
    return true;
}

static bool dictionary_snapshot(void) {
    AbSettingsStatistics *settings = NULL;
    CHECK(ab_settings_statistics_open(
        (AbSettingsStatisticsConfig){span("fixture"), span("fixture"), 4, 4, 65536},
        &settings, NULL));
    const char *names[] = {"a", "b", "c"};
    for (size_t index = 0; index < 3; ++index) {
        AbSpan path[] = {span("tutorials"), span(names[index])};
        CHECK(ab_settings_statistics_write(
            settings, path, 2,
            (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = index != 1}, NULL));
    }
    AbSpan root = span("tutorials");
    AbDataEntry entries[4];
    size_t count = 99;
    CHECK(!ab_settings_statistics_pairs(settings, &root, 1, entries, 2, &count, NULL) &&
          count == 99);
    CHECK(ab_settings_statistics_pairs(settings, &root, 1, entries, 4, &count, NULL) &&
          count == 3 && entries[0].key.string.size == 1 &&
          entries[0].key.string.data[0] == 'a' &&
          entries[1].key.string.data[0] == 'c' &&
          entries[2].key.string.data[0] == 'b' && !entries[2].value.bits);
    const AbDataTable *table =
        ab_data_get(ab_settings_statistics_root(settings),
                    (AbDataValue){.kind = AB_DATA_STRING, .string = root})
            ->table;
    size_t cursor = 0;
    AbDataEntry row = {0};
    bool present = false;
    for (size_t index = 0; index < count; ++index) {
        CHECK(ab_settings_statistics_table_next(settings, table, &cursor, &row,
                                                &present, NULL) &&
              present && row.key.string.data == entries[index].key.string.data &&
              row.value.bits == entries[index].value.bits);
    }
    CHECK(ab_settings_statistics_table_next(settings, table, &cursor, &row, &present,
                                            NULL) &&
          !present);
    AbDataTable foreign = {0};
    CHECK(!ab_settings_statistics_table_next(settings, &foreign, &cursor, &row,
                                             &present, NULL));
    AbSpan removed[] = {root, span("c")};
    CHECK(ab_settings_statistics_write(settings, removed, 2, (AbDataValue){0}, NULL) &&
          ab_settings_statistics_pairs(settings, &root, 1, entries, 4, &count, NULL) &&
          count == 2 && entries[0].key.string.data[0] == 'a' &&
          entries[1].key.string.data[0] == 'b');
    AbSpan array = span("visited");
    CHECK(ab_settings_statistics_append(settings, &array, 1,
                                        (AbDataValue){.kind = AB_DATA_BOOLEAN}, NULL) &&
          !ab_settings_statistics_pairs(settings, &array, 1, entries, 4, &count, NULL));
    table = ab_data_get(ab_settings_statistics_root(settings),
                        (AbDataValue){.kind = AB_DATA_STRING, .string = array})
                ->table;
    cursor = 0;
    CHECK(ab_settings_statistics_table_next(settings, table, &cursor, &row, &present,
                                            NULL) &&
          present && cursor == 1 && row.key.kind == AB_DATA_NUMBER &&
          row.key.bits == UINT32_C(0x3f800000) && row.value.kind == AB_DATA_BOOLEAN &&
          !row.value.bits &&
          ab_settings_statistics_table_next(settings, table, &cursor, &row, &present,
                                            NULL) &&
          !present);
    ab_settings_statistics_close(settings);
    return true;
}

static bool level_started_records(void) {
    AbSettingsStatistics *settings = NULL;
    CHECK(ab_settings_statistics_open(
        (AbSettingsStatisticsConfig){span("device"), span("fixture"), 8, 16, 65536},
        &settings, NULL));
    const AbDataTable *local = NULL;
    AbDataValue previous = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x42c60000)};
    float count = 99;
    CHECK(ab_settings_statistics_level_started_count(settings, span("Level1"), &count,
                                                     NULL) &&
          count == 0 &&
          ab_settings_statistics_start_level(settings, span("Level1"), &local,
                                             &previous, NULL) &&
          local && previous.kind == AB_DATA_NIL &&
          ab_settings_statistics_start_level(settings, span("Level1"), &local,
                                             &previous, NULL) &&
          previous.kind == AB_DATA_NUMBER && previous.bits == UINT32_C(0x3f800000));
    AbSpan path[] = {span("levelStarted"), span("installationId_prior"),
                     span("Level1")};
    CHECK(ab_settings_statistics_write(settings, path, 3, value(3, 0x40e00000), NULL));
    path[2] = span("OtherLevel");
    CHECK(ab_settings_statistics_write(
              settings, path, 3,
              (AbDataValue){.kind = AB_DATA_STRING, .string = span("ignored")}, NULL) &&
          ab_settings_statistics_level_started_count(settings, span("Level1"), &count,
                                                     NULL) &&
          count == 9);
    AbDataValue root = *ab_data_get(
        ab_settings_statistics_root(settings),
        (AbDataValue){.kind = AB_DATA_STRING, .string = span("levelStarted")});
    const AbDataValue *installation = ab_data_get(
        root.table, (AbDataValue){.kind = AB_DATA_STRING,
                                  .string = span("installationId_fixture")});
    CHECK(installation && installation->kind == AB_DATA_TABLE &&
          installation->table == local &&
          !ab_data_get(root.table, (AbDataValue){.kind = AB_DATA_STRING,
                                                 .string = span("device_device")}));
    path[1] = span("installationId_fixture");
    path[2] = span("Level1");
    CHECK(
        ab_settings_statistics_write(settings, path, 3, value(1, 0), NULL) &&
        ab_settings_statistics_start_level(settings, path[2], &local, &previous,
                                           NULL) &&
        previous.kind == AB_DATA_BOOLEAN && !previous.bits &&
        ab_settings_statistics_level_started_count(settings, path[2], &count, NULL) &&
        count == 8 &&
        ab_settings_statistics_write(settings, path, 3, value(4, 0), NULL) &&
        !ab_settings_statistics_start_level(settings, path[2], &local, &previous,
                                            NULL) &&
        previous.kind == AB_DATA_STRING &&
        !ab_settings_statistics_level_started_count(settings, path[2], &count, NULL) &&
        count == 8);
    CHECK(ab_settings_statistics_write(settings, path, 1, value(1, 0), NULL) &&
          !ab_settings_statistics_start_level(settings, path[2], &local, &previous,
                                              NULL) &&
          previous.kind == AB_DATA_STRING);
    ab_settings_statistics_close(settings);
    return true;
}

int main(int argc, char **argv) {
    if (!storage_guards() || !tutorial_guards() || !dictionary_snapshot() ||
        !level_started_records())
        return 1;
    if (argc == 1)
        return 0;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = compare(file);
    if (fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
