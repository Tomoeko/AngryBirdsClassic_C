#include "ab/mp3.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "MP3 synthesis transform failed at %d: %s\n", __LINE__,    \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edges(void) {
    AbMp3SynthesisTransform transform;
    CHECK(ab_mp3_synthesis_transform_init(&transform, NULL) &&
          !ab_mp3_synthesis_transform_init(NULL, NULL));
    float frequency[32] = {0}, output[32], previous[32];
    CHECK(ab_mp3_synthesis_transform(&transform, frequency, output, NULL));
    for (size_t index = 0; index < 32; ++index) {
        CHECK(bits(output[index]) == 0);
        output[index] = 123.0f;
    }
    memcpy(previous, output, sizeof(previous));
    CHECK(!ab_mp3_synthesis_transform(NULL, frequency, output, NULL) &&
          !ab_mp3_synthesis_transform(&transform, NULL, output, NULL) &&
          !ab_mp3_synthesis_transform(&transform, frequency, NULL, NULL));
    frequency[17] = NAN;
    CHECK(!ab_mp3_synthesis_transform(&transform, frequency, output, NULL));
    frequency[17] = 0.0f;
    frequency[0] = FLT_MAX;
    frequency[31] = FLT_MAX;
    CHECK(!ab_mp3_synthesis_transform(&transform, frequency, output, NULL));
    frequency[0] = frequency[31] = 0.0f;
    transform.factors[30] = INFINITY;
    CHECK(!ab_mp3_synthesis_transform(&transform, frequency, output, NULL) &&
          memcmp(previous, output, sizeof(previous)) == 0);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) && strcmp(line, "AB_MP3_DCT64_V1\n") == 0);
    AbMp3SynthesisTransform transform;
    CHECK(ab_mp3_synthesis_transform_init(&transform, NULL));
    size_t offset = 0;
    for (size_t count = 16; count; count /= 2) {
        uint32_t expected[16];
        CHECK(words(file, expected, count));
        for (size_t index = 0; index < count; ++index) {
            CHECK(bits(transform.factors[offset++]) == expected[index]);
        }
    }
    CHECK(offset == 31);
    size_t samples = 0;
    for (;;) {
        uint32_t metadata[3];
        CHECK(fgets(line, sizeof(line), file));
        int count = sscanf(line, "%" SCNx32 " %" SCNx32 " %" SCNx32, metadata,
                           metadata + 1, metadata + 2);
        if (count == 1) {
            CHECK(metadata[0] == samples && samples);
            break;
        }
        CHECK(count == 3 && metadata[0] == samples && metadata[1] == samples / 18 &&
              (metadata[2] == UINT32_C(0x8f1fa4) || metadata[2] == UINT32_C(0x8f2314)));
        uint32_t encoded[32], first[17], second[16];
        CHECK(words(file, encoded, 32) && words(file, first, 17) &&
              words(file, second, 16) && first[0] == second[0]);
        float frequency[32], actual[32];
        for (size_t index = 0; index < 32; ++index) {
            memcpy(frequency + index, encoded + index, sizeof(float));
        }
        CHECK(ab_mp3_synthesis_transform(&transform, frequency, actual, NULL));
        for (size_t index = 0; index < 32; ++index) {
            uint32_t expected = index <= 16 ? first[16 - index] : second[index - 16];
            if (bits(actual[index]) != expected) {
                fprintf(stderr,
                        "Synthesis transform %zu index %zu: "
                        "%08" PRIx32 " != %08" PRIx32 "\n",
                        samples, index, bits(actual[index]), expected);
                return false;
            }
        }
        ++samples;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB MP3 dct64 reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared %zu native synthesis transforms and %zu float coefficients\n",
           samples, samples * 32);
    return true;
}

int main(int argc, char **argv) {
    if (!edges()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: ab_mp3_synthesis_transform_tests [NATIVE_CORPUS]\n");
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
