#include "../src/resource/vp8_predict.h"
#include "vp8_predict_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "VP8 predictor check failed at %d: %s\n", __LINE__,        \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    uint8_t pixels[640];
    memset(pixels, 77, sizeof(pixels));
    CHECK(!ab_vp8_predict_large(AB_VP8_PRED_DC, 4, true, true, pixels + 72, 32, NULL) &&
          !ab_vp8_predict_small((AbVp8Prediction)10, pixels + 72, 32, NULL) &&
          !ab_vp8_predict_small(AB_VP8_PRED_DC, pixels + 72, 4, NULL));
    for (size_t index = 0; index < sizeof(pixels); ++index) {
        CHECK(pixels[index] == 77);
    }
    CHECK(
        ab_vp8_predict_large(AB_VP8_PRED_DC, 16, false, false, pixels + 72, 32, NULL));
    for (size_t row = 0; row < 16; ++row) {
        for (size_t column = 0; column < 16; ++column) {
            CHECK(pixels[72 + row * 32 + column] == 128);
        }
    }
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
    }
    return fgetc(file) == '\n';
}

static bool predict_case(const uint32_t input[3], uint8_t pixels[640]) {
    if (input[0] == 2) {
        static const AbVp8Prediction modes[10] = {AB_VP8_PRED_DC,
                                                  AB_VP8_PRED_TM,
                                                  AB_VP8_PRED_VERTICAL,
                                                  AB_VP8_PRED_HORIZONTAL,
                                                  AB_VP8_PRED_DOWN_RIGHT,
                                                  AB_VP8_PRED_VERTICAL_RIGHT,
                                                  AB_VP8_PRED_DOWN_LEFT,
                                                  AB_VP8_PRED_VERTICAL_LEFT,
                                                  AB_VP8_PRED_HORIZONTAL_DOWN,
                                                  AB_VP8_PRED_HORIZONTAL_UP};
        return ab_vp8_predict_small(modes[input[1]], pixels + 72, 32, NULL);
    }
    unsigned mode = input[1];
    bool above = mode != 4 && mode != 6;
    bool left = mode != 5 && mode != 6;
    return ab_vp8_predict_large(mode < 4 ? (AbVp8Prediction)mode : AB_VP8_PRED_DC,
                                input[0] == 0 ? 16 : 8, above, left, pixels + 72, 32,
                                NULL);
}

static bool reference_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_VP8_PREDICT_V1 4096\n") == 0);
    uint32_t selected[3][10] = {{0}};
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[3], expected_input[3], expected[161];
        uint8_t pixels[640];
        vp8_predict_case(sample, input, pixels);
        CHECK(read_words(file, expected_input, 3) &&
              memcmp(input, expected_input, sizeof(input)) == 0 &&
              read_words(file, expected, 161));
        uint32_t *function = &selected[input[0]][input[1]];
        CHECK(expected[0] && (!*function || *function == expected[0]));
        *function = expected[0];
        CHECK(predict_case(input, pixels));
        for (size_t index = 0; index < 160; ++index) {
            uint32_t actual = ab_u32le(pixels + index * 4);
            if (actual != expected[index + 1]) {
                fprintf(stderr,
                        "VP8 predictor case %zu type %u mode %u word %zu: %08" PRIx32
                        "/%08" PRIx32 "\n",
                        sample, input[0], input[1], index, actual, expected[index + 1]);
                return false;
            }
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB VP8 predictor reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched all 4,096 original predictor calls and all pixel/guard bytes.");
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
