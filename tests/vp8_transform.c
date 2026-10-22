#include "../src/resource/vp8_transform.h"
#include "ab/base.h"
#include "vp8_transform_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "VP8 transform check failed at %d: %s\n", __LINE__,        \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    int16_t coefficients[16] = {8};
    uint8_t pixels[32] = {0};
    ab_vp8_dct_add(coefficients, pixels, 8);
    for (size_t index = 0; index < sizeof(pixels); ++index) {
        CHECK(pixels[index] == (index % 8 < 4 ? 1 : 0));
    }
    coefficients[0] = -5;
    ab_vp8_dct_add(coefficients, pixels, 8);
    for (size_t index = 0; index < sizeof(pixels); ++index) {
        CHECK(pixels[index] == 0);
    }
    /* Bound the full int16_t input domain under sanitizers, separately from
     * the native corpus's documented coefficient range. */
    for (size_t index = 0; index < 16; ++index) {
        coefficients[index] = index % 2 ? INT16_MIN : INT16_MAX;
    }
    ab_vp8_dct_add(coefficients, pixels, 8);
    for (size_t index = 0; index < sizeof(pixels); ++index) {
        CHECK(index % 8 < 4 || pixels[index] == 0);
    }
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
    }
    return fgetc(file) == '\n';
}

static void transform_case(unsigned kind, const int16_t coefficients[32],
                           uint8_t pixels[528]) {
    if (kind == 0) {
        int16_t values[16];
        ab_vp8_walsh_inverse(coefficients, values);
        for (size_t index = 0; index < 16; ++index) {
            pixels[8 + index * 32] = (uint8_t)values[index];
            pixels[9 + index * 32] = (uint8_t)((uint16_t)values[index] >> 8);
        }
    } else {
        ab_vp8_dct_add(coefficients, pixels + 72, 32);
        if (kind == 2) {
            ab_vp8_dct_add(coefficients + 16, pixels + 76, 32);
        }
    }
}

static bool reference_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_VP8_TRANSFORM_V1 4096\n") == 0);
    uint32_t selected[5] = {0};
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[3], expected_input[3], expected[133];
        int16_t coefficients[32];
        uint8_t pixels[528];
        vp8_transform_case(sample, input, coefficients, pixels);
        CHECK(read_words(file, expected_input, 3) &&
              memcmp(input, expected_input, sizeof(input)) == 0 &&
              read_words(file, expected, 133));
        unsigned kind = input[0];
        CHECK(expected[0] && (!selected[kind] || selected[kind] == expected[0]));
        selected[kind] = expected[0];
        transform_case(kind, coefficients, pixels);
        for (size_t index = 0; index < 132; ++index) {
            uint32_t actual = ab_u32le(pixels + index * 4);
            if (actual != expected[index + 1]) {
                fprintf(stderr,
                        "VP8 transform case %zu kind %u word %zu: %08" PRIx32
                        "/%08" PRIx32 "\n",
                        sample, kind, index, actual, expected[index + 1]);
                return false;
            }
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB VP8 transform reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched 4,096 original inverse Walsh/DCT calls and all output/guard bytes.");
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
    bool okay = reference_checks(file);
    if (fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
