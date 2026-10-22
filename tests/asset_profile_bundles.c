#include "ab/asset_profile.h"
#include "ab/asset_bundles.h"
#include "asset_profile_bundle_names.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Android bundle check failed at %d: %s\n", __LINE__,       \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(uint32_t index) {
    float value = (float)index;
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(result.bits));
    return result;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (fscanf(file, "%" SCNx32, output + index) != 1) {
            return false;
        }
    }
    return fgetc(file) == '\n';
}

static uint32_t member_id(AbSpan member) {
    for (uint32_t index = 0; index < 13; ++index) {
        if (member.size == strlen(profile_bundle_members[index]) &&
            memcmp(member.data, profile_bundle_members[index], member.size) == 0) {
            return index + 1;
        }
    }
    return 0;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_PROFILE_BUNDLES_V1 4\n") == 0);
    for (uint32_t sample = 0; sample < 4; ++sample) {
        uint32_t flags[2];
        CHECK(read_words(file, flags, 2) && flags[0] == (sample & 1u) &&
              flags[1] == sample >> 1);
        AbAndroidAssetBundles owner;
        CHECK(ab_android_asset_bundles(flags[0] != 0, flags[1] != 0, &owner) &&
              owner.profiles.count == (flags[0] ? 6u : 2u));
        for (size_t key = 0; key < 6; ++key) {
            uint32_t actual[4] = {0}, expected[4];
            CHECK(read_words(file, expected, 4));
            const AbDataValue *bundle =
                ab_data_get(&owner.profiles, text(profile_bundle_keys[key]));
            if (bundle) {
                CHECK(bundle->kind == AB_DATA_TABLE && bundle->table &&
                      bundle->table->count <= 3);
                actual[0] = 1;
                for (uint32_t member = 1; member <= bundle->table->count; ++member) {
                    const AbDataValue *value =
                        ab_data_get(bundle->table, number(member));
                    CHECK(value && value->kind == AB_DATA_STRING);
                    actual[member] = member_id(value->string);
                    CHECK(actual[member]);
                }
            }
            CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB Android profile bundles reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched all Android profile bundle members across four China/Sugotoku "
         "configurations, including override precedence");
    return true;
}

static bool visit(void *context, AbSpan name, bool loading, AbError *error) {
    (void)error;
    size_t *count = context;
    CHECK(loading && member_id(name));
    ++*count;
    return true;
}

static bool edge_checks(void) {
    CHECK(!ab_android_asset_bundles(false, false, NULL));
    AbAndroidAssetBundles owner;
    CHECK(ab_android_asset_bundles(false, false, &owner));
    AbDataEntry entries[6];
    for (uint32_t index = 0; index < 6; ++index) {
        entries[index] =
            (AbDataEntry){number(index + 1), text(profile_bundle_keys[index])};
    }
    AbDataTable requested = {entries, 6, 6, NULL};
    size_t count = 0;
    CHECK(ab_asset_bundle_dispatch(&requested, &owner.profiles, true, visit, &count,
                                   NULL) &&
          count == 8);
    CHECK(ab_android_asset_bundles(true, true, &owner));
    count = 0;
    CHECK(ab_asset_bundle_dispatch(&requested, &owner.profiles, true, visit, &count,
                                   NULL) &&
          count == 11);
    CHECK(!ab_data_get(&owner.profiles, text("selectAssetProfile")));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
