#include "ab/utf8.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UTF8 failed at line %d: %s\n", __LINE__, #condition);     \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    uint8_t bytes[] = {0xc0, 0x41, 0xff, 0xf7, 0xbf, 0xbf, 0xbf, 0xe2};
    size_t consumed = 19;
    uint32_t code = 17;
    CHECK(ab_utf8_decode((AbSpan){bytes, 2}, &consumed, &code));
    CHECK(consumed == 2 && code == 1);
    CHECK(ab_utf8_decode((AbSpan){bytes + 3, 4}, &consumed, &code));
    CHECK(consumed == 4 && code == 0x1fffff);
    CHECK(!ab_utf8_decode((AbSpan){bytes + 7, 1}, &consumed, &code));
    CHECK(consumed == 1 && code == 0x1fffff);
    CHECK(!ab_utf8_decode((AbSpan){NULL, 0}, &consumed, &code));
    CHECK(consumed == 0 && code == 0x1fffff);
    consumed = 19;
    CHECK(!ab_utf8_decode((AbSpan){NULL, 1}, &consumed, &code));
    CHECK(consumed == 19 && code == 0x1fffff);
    uint32_t output[8];
    size_t count = 77;
    CHECK(ab_utf8_to_codes((AbSpan){bytes, sizeof(bytes)}, output, 8, &count));
    CHECK(count == 2 && output[0] == 1 && output[1] == 0x1fffff);
    count = 77;
    CHECK(!ab_utf8_to_codes((AbSpan){bytes, sizeof(bytes)}, output, 1, &count));
    CHECK(count == 77);
    CHECK(ab_utf8_to_codes((AbSpan){bytes + 2, 1}, NULL, 0, &count));
    CHECK(count == 0);
    CHECK(!ab_utf8_to_codes((AbSpan){bytes, 1}, NULL, 1, &count));
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB_ANDROID_UTF8_V1 65536\n") == 0);
    size_t decoded_count = 0, whole_values = 0;
    for (size_t item = 0; item < 65536; ++item) {
        uint32_t input[3], expected[3];
        CHECK(read_words(file, input, 3) && read_words(file, expected, 3));
        CHECK(input[0] <= 8 && expected[0] <= 1);
        uint8_t bytes[8];
        for (size_t byte = 0; byte < 8; ++byte) {
            bytes[byte] = (uint8_t)(input[1 + byte / 4] >> (byte % 4 * 8));
        }
        size_t consumed = UINT32_C(0x5a5a5a5a);
        uint32_t code = UINT32_C(0x12345678);
        bool decoded = ab_utf8_decode((AbSpan){bytes, input[0]}, &consumed, &code);
        if ((uint32_t)decoded != expected[0] || consumed != expected[1] ||
            code != expected[2]) {
            fprintf(stderr, "Native UTF8 step %zu differs\n", item);
            return false;
        }
        decoded_count += decoded;
        if (item % 16 == 0) {
            uint32_t length;
            CHECK(fscanf(file, "%8" SCNx32, &length) == 1 && length <= 8);
            uint32_t expected_codes[8], actual_codes[8];
            for (uint32_t index = 0; index < length; ++index) {
                CHECK(fscanf(file, "%8" SCNx32, expected_codes + index) == 1);
            }
            CHECK(fgetc(file) == '\n');
            size_t count;
            CHECK(ab_utf8_to_codes((AbSpan){bytes, input[0]}, actual_codes, 8, &count));
            CHECK(count == length &&
                  memcmp(actual_codes, expected_codes, count * sizeof(uint32_t)) == 0);
            whole_values += count;
        }
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB UTF8 reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 65536 native UTF8 steps match (%zu decoded); 4096 native string "
           "conversions match %zu code values.\n",
           decoded_count, whole_values);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_utf8_tests [NATIVE_UTF8_TRACE]\n");
        return 2;
    }
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror(argv[1]);
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
