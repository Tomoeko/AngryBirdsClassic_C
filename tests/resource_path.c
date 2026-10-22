#include "ab/asset_groups.h"
#include "resource_path_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Resource key check failed at %d: %s\n", __LINE__,         \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_RESOURCE_PATH_V1 4096\n") == 0);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        char path[256], key[256];
        size_t size = resource_path_case(sample, path), length = 0;
        uint32_t input[2], count;
        CHECK(fscanf(file, "%" SCNx32 " %" SCNx32, input, input + 1) == 2 &&
              fgetc(file) == '\n' && input[0] == size &&
              input[1] == resource_path_hash(path, size) &&
              fscanf(file, "%" SCNx32, &count) == 1 && count <= 128);
        CHECK(ab_asset_sheet_key((AbSpan){(const uint8_t *)path, size}, key,
                                 sizeof(key), &length, NULL));
        char expected[129];
        for (uint32_t byte = 0; byte < count; ++byte) {
            uint32_t value;
            CHECK(fscanf(file, "%" SCNx32, &value) == 1 && value <= 255);
            expected[byte] = (char)value;
        }
        expected[count] = 0;
        CHECK(fgetc(file) == '\n');
        if (length != count || memcmp(key, expected, count) != 0) {
            fprintf(stderr,
                    "Original sheet key sample %" PRIu32 ": '%s' -> '%s', "
                    "expected '%s'\n",
                    sample, path, key, expected);
            return false;
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB resource path reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched 4,096 original two-pass PathName resource keys, including "
         "dot normalization, extension stripping, empty keys, case and spaces");
    return true;
}

static bool edge_checks(void) {
    char key[64] = "unchanged";
    size_t length = 99;
    const char path[] = "assets/data/images/base/INGAME_BLOCKS_1.dat";
    CHECK(ab_asset_sheet_key((AbSpan){(const uint8_t *)path, sizeof(path) - 1}, key,
                             sizeof(key), &length, NULL) &&
          length == 15 && strcmp(key, "INGAME_BLOCKS_1") == 0);
    CHECK(!ab_asset_sheet_key((AbSpan){(const uint8_t *)path, sizeof(path) - 1}, key, 1,
                              &length, NULL) &&
          length == 15 && strcmp(key, "INGAME_BLOCKS_1") == 0);
    CHECK(!ab_asset_sheet_key((AbSpan){(const uint8_t *)"a", SIZE_MAX}, key,
                              sizeof(key), &length, NULL));
    CHECK(!ab_asset_sheet_key((AbSpan){(const uint8_t *)"a\\b", 3}, key, sizeof(key),
                              &length, NULL));
    CHECK(!ab_asset_sheet_key((AbSpan){(const uint8_t *)"C:b", 3}, key, sizeof(key),
                              &length, NULL));
    CHECK(!ab_asset_sheet_key((AbSpan){(const uint8_t *)"a\0b", 3}, key, sizeof(key),
                              &length, NULL));
    CHECK(ab_asset_sheet_key((AbSpan){0}, key, sizeof(key), &length, NULL) && !length &&
          !key[0]);
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
