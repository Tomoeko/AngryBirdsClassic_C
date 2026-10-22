#include "../src/resource/vp8_boolean.h"
#include "vp8_boolean_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "VP8 boolean check failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    const uint8_t bytes[2] = {64, 128};
    AbVp8Boolean reader;
    uint32_t value = 99;
    CHECK(ab_vp8_boolean_init((AbSpan){bytes, 2}, &reader, NULL) &&
          ab_vp8_boolean_value(&reader, 0, &value, NULL) && !value &&
          ab_vp8_boolean_bit(&reader, 128, &value, NULL) && !value);
    AbVp8Boolean snapshot = reader;
    value = 99;
    CHECK(!ab_vp8_boolean_value(&reader, 24, &value, NULL) && value == 99 &&
          memcmp(&reader, &snapshot, sizeof(reader)) == 0 &&
          !ab_vp8_boolean_value(&reader, 25, &value, NULL) && value == 99);
    CHECK(ab_vp8_boolean_init((AbSpan){bytes, 2}, &reader, NULL) &&
          ab_vp8_boolean_bit(&reader, 0, &value, NULL) && value == 1);
    CHECK(ab_vp8_boolean_init((AbSpan){bytes, 2}, &reader, NULL) &&
          ab_vp8_boolean_bit(&reader, 255, &value, NULL) && value == 0);
    const uint8_t outside[2] = {255, 0};
    CHECK(!ab_vp8_boolean_init((AbSpan){outside, 2}, &reader, NULL) &&
          !ab_vp8_boolean_init((AbSpan){bytes, 1}, &reader, NULL));
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
          strcmp(line, "AB_ANDROID_VP8_BOOLEAN_V1 4096\n") == 0);
    size_t signed_fields = 0;
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint8_t bytes[512];
        uint32_t input[34], generated[34], expected[32];
        vp8_boolean_case(sample, generated, bytes);
        CHECK(read_words(file, input, 34) &&
              memcmp(input, generated, sizeof(input)) == 0 &&
              read_words(file, expected, 32));
        AbVp8Boolean reader;
        CHECK(ab_vp8_boolean_init((AbSpan){bytes, input[0]}, &reader, NULL));
        for (size_t index = 0; index < 32; ++index) {
            uint32_t command = input[index + 2], actual;
            unsigned count = command & 31u;
            if (command & 32u) {
                int32_t value;
                CHECK(ab_vp8_boolean_signed(&reader, count, &value, NULL));
                actual = (uint32_t)value;
                ++signed_fields;
            } else {
                CHECK(ab_vp8_boolean_value(&reader, count, &actual, NULL));
            }
            if (actual != expected[index]) {
                fprintf(stderr,
                        "Original VP8 field sample %zu operation %zu differs: "
                        "%08" PRIx32 "/%08" PRIx32 "\n",
                        sample, index, actual, expected[index]);
                return false;
            }
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB VP8 boolean reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Matched 131,072 original VP8 arithmetic fields, including %zu signed "
           "fields and following zero-magnitude sign bits.\n",
           signed_fields);
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
