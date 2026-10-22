#include "../src/resource/vp8_coefficients.h"
#include "vp8_coefficient_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "VP8 coefficient check failed at %d: %s\n", __LINE__,      \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    AbVp8Boolean reader = {0};
    int16_t coefficients[16] = {1234};
    int32_t quantizers[2] = {4, 4};
    uint8_t probabilities[264] = {0};
    unsigned end = 99;
    CHECK(!ab_vp8_coefficients_read(&reader, probabilities, 3, quantizers, 0,
                                    coefficients, &end, NULL) &&
          !ab_vp8_coefficients_read(&reader, probabilities, 0, quantizers, 2,
                                    coefficients, &end, NULL) &&
          !ab_vp8_coefficients_read(&reader, probabilities, 0, quantizers, 0,
                                    coefficients, &end, NULL) &&
          coefficients[0] == 1234 && end == 99);
    uint8_t bytes[4] = {0};
    memset(probabilities, 128, sizeof(probabilities));
    CHECK(ab_vp8_boolean_init((AbSpan){bytes, sizeof(bytes)}, &reader, NULL) &&
          ab_vp8_coefficients_read(&reader, probabilities, 0, quantizers, 1,
                                   coefficients, &end, NULL) &&
          end == 1 && coefficients[0] == 1234);
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
    }
    return fgetc(file) == '\n';
}

static bool compare_block(FILE *file, size_t sample, size_t block, AbVp8Boolean *reader,
                          const uint8_t probabilities[264], const uint32_t input[13]) {
    int16_t values[20] = {0};
    values[0] = values[1] = values[18] = values[19] = 12345;
    unsigned first = input[5 + block] & 1;
    unsigned context = input[5 + block] >> 1;
    if (first) {
        values[2] = -1234;
    }
    int32_t quantizers[2] = {(int32_t)input[3], (int32_t)input[4]};
    unsigned end;
    AbError error = {0};
    if (!ab_vp8_coefficients_read(reader, probabilities, context, quantizers, first,
                                  values + 2, &end, &error)) {
        fprintf(stderr, "VP8 coefficient case %zu block %zu: %s\n", sample, block,
                error.message);
        return false;
    }
    uint32_t expected[14],
        actual[14] = {end, reader->range,
                      (uint32_t)((reader->offset - 2) * 8 + reader->shifted), 0};
    for (size_t index = 0; index < 10; ++index) {
        actual[4 + index] = (uint32_t)(uint16_t)values[index * 2] |
                            ((uint32_t)(uint16_t)values[index * 2 + 1] << 16);
    }
    CHECK(read_words(file, expected, 14));
    for (size_t index = 0; index < 14; ++index) {
        if (actual[index] != expected[index]) {
            fprintf(stderr,
                    "VP8 coefficient case %zu block %zu field %zu: %08" PRIx32
                    "/%08" PRIx32 "\n",
                    sample, block, index, actual[index], expected[index]);
            return false;
        }
    }
    return true;
}

static bool reference_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_VP8_COEFFICIENTS_V1 4096\n") == 0);
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[13], expected_input[13];
        uint8_t bytes[8192], probabilities[264];
        vp8_coefficient_case(sample, input, bytes, probabilities);
        CHECK(read_words(file, expected_input, 13) &&
              memcmp(input, expected_input, sizeof(input)) == 0);
        AbVp8Boolean reader;
        CHECK(ab_vp8_boolean_init((AbSpan){bytes, sizeof(bytes)}, &reader, NULL));
        for (size_t block = 0; block < 8; ++block) {
            CHECK(compare_block(file, sample, block, &reader, probabilities, input));
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB VP8 coefficient reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts(
        "Matched 32,768 original VP8 coefficient blocks, arithmetic state and guards.");
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
