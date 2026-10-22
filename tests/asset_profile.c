#include "ab/asset_profile.h"
#include "ab/target_data.h"

#include "asset_profile_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Asset profile check failed at %d: %s\n", __LINE__,        \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static float number(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool same(AbSpan value, const char *expected) {
    return value.size == strlen(expected) &&
           (!value.size || memcmp(value.data, expected, value.size) == 0);
}

static bool edge_checks(void) {
    AbDataEntry member = {{.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)},
                          text("STATIC")};
    AbDataTable base = {.entries = &member, .count = 1, .capacity = 1}, empty = {0};
    AbDataEntry groups[] = {
        {text("base"), {.kind = AB_DATA_TABLE, .table = &base}},
        {text("adventure_ingame"), {.kind = AB_DATA_TABLE, .table = &empty}},
        {text("adventure_1024x768"), {.kind = AB_DATA_TABLE, .table = &empty}},
        {text("adventure_themes"), {.kind = AB_DATA_TABLE, .table = &empty}}};
    AbDataTable catalog = {.entries = groups, .count = 4, .capacity = 4};
    AbAssetProfileSource source = {.asset_groups = &catalog,
                                   .screen_width = 480,
                                   .splash_profile = text("chosen_splash").string};
    AbSpan output = text("unchanged").string;
    CHECK(ab_android_asset_profile(&source, text("STATIC").string, &output, NULL) &&
          same(output, "base"));
    CHECK(ab_android_asset_profile(&source, text("MENU_CHINA").string, &output, NULL) &&
          same(output, "480x320_china"));
    source.screen_width = number(UINT32_C(0x43f00001));
    CHECK(ab_android_asset_profile(&source, text("MENU_CHINA").string, &output, NULL) &&
          same(output, "1024x768_china"));
    source.asset_groups = NULL;
    CHECK(
        ab_android_asset_profile(&source, text("THEME_CHINA").string, &output, NULL) &&
        same(output, "themes_android"));
    CHECK(ab_android_asset_profile(&source, text("CUTSCENES").string, &output, NULL) &&
          same(output, "480x320"));
    CHECK(ab_android_asset_profile(&source, text("SPLASHES").string, &output, NULL) &&
          output.data == source.splash_profile.data);
    output = text("unchanged").string;
    CHECK(!ab_android_asset_profile(&source, text("MENU").string, &output, NULL) &&
          same(output, "unchanged"));
    source.asset_groups = &catalog;
    source.adventure_mode = true;
    member.value = text("invalid_member");
    CHECK(ab_android_asset_profile(&source, text("STATIC").string, &output, NULL) &&
          same(output, "1024x768"));
    source.screen_width = NAN;
    AbVec2 dimensions = {7, 8};
    CHECK(!ab_android_asset_profile(&source, text("THEME").string, &output, NULL) &&
          !ab_android_default_profile(NAN, &dimensions) && dimensions.x == 7 &&
          dimensions.y == 8);
    CHECK(ab_android_default_profile(480, &dimensions) && dimensions.x == 480 &&
          dimensions.y == 320);
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    return fgetc(file) == '\n';
}

static bool native_checks(FILE *file, const AbDataTable *groups) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_ASSET_PROFILE_V1 4096\n") == 0);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[5], generated[5], expected[5];
        asset_profile_case(sample, generated);
        CHECK(read_words(file, input, 5) &&
              memcmp(input, generated, sizeof(input)) == 0 &&
              read_words(file, expected, 5));
        AbAssetProfileSource source = {
            .asset_groups = groups,
            .screen_width = number(input[1]),
            .adventure_mode = input[2] && input[3],
            .splash_profile = text(asset_profile_splashes[(sample >> 11) & 1u]).string};
        AbSpan directory;
        AbVec2 logical;
        CHECK(ab_android_asset_profile(&source,
                                       text(asset_profile_groups[input[4]]).string,
                                       &directory, NULL) &&
              ab_android_default_profile(source.screen_width, &logical));
        uint32_t hash = UINT32_C(2166136261);
        for (size_t index = 0; index < directory.size; ++index) {
            hash = (hash ^ directory.data[index]) * UINT32_C(16777619);
        }
        uint32_t actual[] = {hash, (uint32_t)directory.size,
                             directory.data == source.splash_profile.data,
                             bits(logical.x), bits(logical.y)};
        for (size_t field = 0; field < 5; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native asset profile %" PRIu32 " field %zu: "
                        "%08" PRIx32 " != %08" PRIx32 "\n",
                        sample, field, actual[field], expected[field]);
                return false;
            }
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB asset profile reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched 4,096 original Android profile/default-size selections: "
         "APK group membership, adventure gates, splash delegation, theme prefixes, "
         "cutscene thresholds and China suffixes");
    return true;
}

static bool native_apk_checks(FILE *file, const char *apk) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbTargetData data = {0};
    uint8_t key[32] = {0};
    AbError error = {0};
    bool okay = false;
    if (!ab_file_read(apk, 128 * 1024 * 1024, &bytes, &error) ||
        !ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) ||
        !ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) ||
        !ab_target_data_open(&archive, "assets/data/scripts/platform/asset_groups.lua",
                             (AbSpan){key, sizeof(key)}, 16 * 1024 * 1024, NULL, &data,
                             &error)) {
        fprintf(stderr, "%s\n", error.message);
        goto release_data;
    }
    const AbDataValue *groups = ab_data_get(data.data.globals, text("AssetGroup"));
    okay =
        groups && groups->kind == AB_DATA_TABLE && native_checks(file, groups->table);

release_data:
    memset(key, 0, sizeof(key));
    ab_target_data_close(&data);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 3) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        return 1;
    }
    bool okay = native_apk_checks(file, argv[2]);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
