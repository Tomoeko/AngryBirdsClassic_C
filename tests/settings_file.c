#include "ab/aes.h"
#include "ab/settings_file.h"
#include "console_common/support/regular_file.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Settings-file check failed at %d: %s (%s)\n", __LINE__,   \
                    #condition, error.message);                                        \
            goto release;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue get(const AbDataTable *table, const char *name) {
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)});
    return value ? *value : (AbDataValue){0};
}

static bool open_storage(AbSettingsStatistics **output, AbError *error) {
    return ab_settings_statistics_open((AbSettingsStatisticsConfig){span("fixture"),
                                                                    span("fixture"), 16,
                                                                    32, 1024 * 1024},
                                       output, error);
}

static bool close_fixture(FILE **fixture) {
    FILE *file = *fixture;
    *fixture = NULL;
    return fclose(file) == 0;
}

static bool same_file(const char *path, const AbBuffer *expected) {
    AbBuffer actual = {0};
    bool okay = cc_regular_file_read_bytes(path, 1, 65536, &actual.data,
                                           &actual.size) == CC_REGULAR_FILE_OK &&
                actual.size == expected->size &&
                !memcmp(actual.data, expected->data, actual.size);
    ab_buffer_free(&actual);
    return okay;
}

static bool roundtrip(const char *path) {
    AbSettingsStatistics *settings = NULL;
    AbSettingsFile *file = NULL;
    AbError error = {0};
    uint8_t key_bytes[32] = {0}; /* Explicit synthetic fixture key. */
    AbSpan key = {key_bytes, sizeof(key_bytes)};
    AbSettingsFileLimits limits = {4096, 65536};
    AbBuffer saved = {0}, encrypted = {0};
    FILE *fixture = NULL;
    bool okay = false, owned_path = false;
    AbSpan watched[] = {span("cutscenesWatched"), span("pack1_intro")};
    AbSpan visited[] = {span("visitedPages"), span("1")};
    AbSpan page[] = {span("selectedPage"), span("1")};
    AbSpan text = span("escaped");
    const char *escaped = ("quote\" and slash\\ and line\n\001"
                           "9\303\251");

    /* Reserve only an absent fixture path; never overwrite an unrelated file. */
    fixture = fopen(path, "wbx");
    CHECK(fixture);
    owned_path = true;
    CHECK(close_fixture(&fixture));
    fixture = NULL;
    CHECK(remove(path) == 0);
    CHECK(open_storage(&settings, &error) &&
          ab_settings_file_open(path, key, limits, &file, &error) &&
          !ab_settings_file_loaded(file));
    CHECK(!ab_settings_file_save(file, settings, NULL));
    CHECK(ab_settings_file_restore(file, settings, &error) &&
          !ab_settings_file_restore(file, settings, NULL));
    CHECK(
        ab_settings_statistics_write(settings, watched, 2,
                                     (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1},
                                     &error) &&
        ab_settings_statistics_write(settings, page, 2, number(2), &error) &&
        ab_settings_statistics_append(settings, visited, 2, number(1), &error) &&
        ab_settings_statistics_append(settings, visited, 2, number(2), &error) &&
        ab_settings_statistics_write(
            settings, &text, 1,
            (AbDataValue){.kind = AB_DATA_STRING, .string = span(escaped)}, &error) &&
        ab_settings_statistics_set_local(settings, span("counter"), number(7),
                                         &error) &&
        ab_settings_file_save(file, settings, &error) && ab_settings_file_loaded(file));
    CHECK(cc_regular_file_read_bytes(path, 16, 65536, &saved.data, &saved.size) ==
          CC_REGULAR_FILE_OK);
    ab_settings_statistics_close(settings);
    settings = NULL;
    ab_settings_file_close(file);
    file = NULL;

    CHECK(open_storage(&settings, &error) &&
          ab_settings_file_open(path, key, limits, &file, &error) &&
          ab_settings_file_loaded(file) &&
          ab_settings_file_restore(file, settings, &error));
    const AbDataTable *root = ab_settings_statistics_root(settings);
    AbDataValue value = get(get(root, "cutscenesWatched").table, "pack1_intro");
    CHECK(value.kind == AB_DATA_BOOLEAN && value.bits == 1);
    value = get(get(root, "selectedPage").table, "1");
    CHECK(value.kind == AB_DATA_NUMBER && value.bits == number(2).bits);
    value = get(get(root, "visitedPages").table, "1");
    CHECK(value.kind == AB_DATA_TABLE && value.table->count == 2 &&
          value.table->entries[1].value.bits == number(2).bits);
    value = get(root, "escaped");
    CHECK(value.kind == AB_DATA_STRING && value.string.size == strlen(escaped) &&
          !memcmp(value.string.data, escaped, value.string.size));
    CHECK(ab_settings_statistics_get_local(settings, span("counter"), &value, &error) &&
          value.kind == AB_DATA_NUMBER && value.bits == number(7).bits);
    CHECK(ab_settings_statistics_write(settings, page, 2, number(3), &error) &&
          ab_settings_file_save(file, settings, &error) && !same_file(path, &saved));
    ab_buffer_free(&saved);
    CHECK(cc_regular_file_read_bytes(path, 16, 65536, &saved.data, &saved.size) ==
          CC_REGULAR_FILE_OK);
    ab_settings_statistics_close(settings);
    settings = NULL;
    ab_settings_file_close(file);
    file = NULL;

    /* Capacity/parse failures leave the existing ciphertext unchanged. */
    CHECK(!ab_settings_file_open(path, key, (AbSettingsFileLimits){1, 65536}, &file,
                                 NULL) &&
          !file && same_file(path, &saved));
    CHECK(!ab_settings_file_open(path, key, (AbSettingsFileLimits){4096, 1}, &file,
                                 NULL) &&
          !file && same_file(path, &saved));
    CHECK(open_storage(&settings, &error) &&
          ab_settings_file_open(path, key, limits, &file, &error) &&
          ab_settings_file_restore(file, settings, &error));
    value = get(get(ab_settings_statistics_root(settings), "selectedPage").table, "1");
    CHECK(value.bits == number(3).bits);
    ab_settings_statistics_close(settings);
    settings = NULL;
    ab_settings_file_close(file);
    file = NULL;

    /* Valid encryption around unsupported literal syntax must still reject. */
    CHECK(ab_asset_encrypt(span("unsafe = function() end\n"), key, &encrypted, &error));
    fixture = fopen(path, "wb");
    CHECK(fixture &&
          fwrite(encrypted.data, 1, encrypted.size, fixture) == encrypted.size);
    CHECK(close_fixture(&fixture));
    fixture = NULL;
    CHECK(!ab_settings_file_open(path, key, limits, &file, NULL) && !file &&
          same_file(path, &encrypted));
    puts("Owned encrypted settings: absent/restore, scalar/dense-array round trips, "
         "atomic replacement and bounded malformed-input rejection passed.");
    okay = true;
release:
    if (fixture)
        (void)fclose(fixture);
    ab_settings_statistics_close(settings);
    ab_settings_file_close(file);
    ab_buffer_free(&saved);
    ab_buffer_free(&encrypted);
    if (owned_path && remove(path) && okay)
        okay = false;
    return okay;
}

static bool campaign_roundtrip(const char *path) {
    AbSettingsStatistics *settings = NULL;
    AbSettingsFile *file = NULL;
    AbError error = {0};
    uint8_t key_bytes[32] = {0};
    AbSpan key = {key_bytes, sizeof(key_bytes)};
    AbSettingsStatisticsConfig config = {span("fixture"), span("fixture"), 8, 8,
                                         2 * 1024 * 1024};
    size_t capacities[] = {16, 1024, 1024, 8, 8, 8, 8, 8};
    size_t bad[] = {16, 3};
    char names[600][32];
    FILE *fixture = NULL;
    bool okay = false, owned_path = false;
    CHECK(!ab_settings_statistics_open_capacities(config, bad, 2, &settings, NULL) &&
          !settings &&
          !ab_settings_statistics_open_capacities(config, NULL, 1, &settings, NULL) &&
          !settings);
    fixture = fopen(path, "wbx");
    CHECK(fixture);
    owned_path = true;
    CHECK(close_fixture(&fixture));
    fixture = NULL;
    CHECK(remove(path) == 0);
    AbSettingsFileLimits limits = {65536, 1024 * 1024};
    size_t planned[8] = {0}, count = 99;
    CHECK(ab_settings_file_open(path, key, limits, &file, &error) &&
          ab_settings_file_capacities(file, 8, planned, 8, &count, &error) &&
          count == 1 && planned[0] == 8 &&
          ab_settings_statistics_open_capacities(config, capacities, 8, &settings,
                                                 &error) &&
          ab_settings_statistics_bytes(settings) < 1024 * 1024 &&
          ab_settings_file_restore(file, settings, &error));
    for (size_t index = 0; index < 600; ++index) {
        int length =
            snprintf(names[index], sizeof(names[index]), "fixture_level_%zu", index);
        CHECK(length > 0 && (size_t)length < sizeof(names[index]));
        AbDataValue previous = {0};
        const AbDataTable *local = NULL;
        CHECK(ab_settings_statistics_start_level(settings, span(names[index]), &local,
                                                 &previous, &error) &&
              previous.kind == AB_DATA_NIL);
        if (index % 2)
            CHECK(ab_settings_statistics_start_level(settings, span(names[index]),
                                                     &local, &previous, &error) &&
                  previous.bits == number(1).bits);
    }
    CHECK(ab_settings_file_save(file, settings, &error) &&
          !ab_settings_file_capacities(file, 8, planned, 8, &count, NULL));
    ab_settings_statistics_close(settings);
    settings = NULL;
    ab_settings_file_close(file);
    file = NULL;
    count = 99;
    CHECK(ab_settings_file_open(path, key, limits, &file, &error) &&
          !ab_settings_file_capacities(file, 8, planned, 2, &count, NULL) &&
          count == 99 &&
          ab_settings_file_capacities(file, 8, planned, 8, &count, &error) &&
          count == 3 && planned[0] == 8 && planned[1] == 8 && planned[2] == 1024 &&
          ab_settings_statistics_open_capacities(config, planned, count, &settings,
                                                 &error) &&
          ab_settings_file_restore(file, settings, &error));
    for (size_t index = 0; index < 600; ++index) {
        float actual = 0;
        CHECK(ab_settings_statistics_level_started_count(settings, span(names[index]),
                                                         &actual, &error) &&
              actual == (index % 2 ? 2 : 1));
    }
    puts(
        "Campaign settings: 600 distinct level counters survive encrypted save/restore "
        "with planned table capacities and less than 1 MiB of reserved settings "
        "storage.");
    okay = true;
release:
    if (fixture)
        (void)fclose(fixture);
    ab_settings_statistics_close(settings);
    ab_settings_file_close(file);
    if (owned_path && remove(path) && okay)
        okay = false;
    return okay;
}

int main(int argc, char **argv) {
    return argc == 2 ? (roundtrip(argv[1]) && campaign_roundtrip(argv[1]) ? 0 : 1) : 2;
}
