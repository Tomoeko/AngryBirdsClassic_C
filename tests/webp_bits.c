#include "../src/resource/webp_internal.h"
#include "webp_bits_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "WebP bits check failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    uint8_t bytes[4] = {0x71, 0x28, 0xee, 0x19};
    AbWebpBits reader;
    uint32_t value = 99;
    CHECK(ab_webp_bits_init((AbSpan){bytes, sizeof(bytes)}, &reader, NULL) &&
          ab_webp_bits_read(&reader, 3, &value, NULL) && value == 1 &&
          ab_webp_bits_read(&reader, 24, &value, NULL) && value == 0x3dc50e &&
          ab_webp_bits_read(&reader, 5, &value, NULL) && value == 3 &&
          reader.offset == 4 && !reader.bit);
    AbWebpBits snapshot = reader;
    value = 99;
    CHECK(!ab_webp_bits_read(&reader, 1, &value, NULL) && value == 99 &&
          memcmp(&snapshot, &reader, sizeof(reader)) == 0 &&
          !ab_webp_bits_read(&reader, 25, &value, NULL) && value == 99 &&
          ab_webp_bits_read(&reader, 0, &value, NULL) && !value);
    CHECK(ab_webp_bits_init((AbSpan){0}, &reader, NULL) &&
          ab_webp_bits_read(&reader, 0, &value, NULL) &&
          !ab_webp_bits_read(&reader, 1, &value, NULL) &&
          !ab_webp_bits_init((AbSpan){NULL, 1}, &reader, NULL));
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
    }
    return fgetc(file) == '\n';
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_WEBP_BITS_V1 4096\n") == 0);
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint8_t bytes[512];
        uint32_t input[34], generated[34], expected[32];
        webp_bits_case(sample, generated, bytes);
        CHECK(read_words(file, input, 34) &&
              memcmp(input, generated, sizeof(input)) == 0 &&
              read_words(file, expected, 32));
        AbWebpBits reader;
        CHECK(ab_webp_bits_init((AbSpan){bytes, input[0]}, &reader, NULL));
        for (size_t index = 0; index < 32; ++index) {
            uint32_t actual;
            CHECK(ab_webp_bits_read(&reader, input[index + 2], &actual, NULL) &&
                  actual == expected[index]);
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB WebP bits reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched 131,072 original VP8L fields of widths zero through 24.");
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
