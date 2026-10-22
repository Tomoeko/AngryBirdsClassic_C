#include "ab/mp3.h"

#include "mp3_transform_cases.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(AB_TEST_MP3_SHORT)
#define checked_transform ab_mp3_short_transform
#define TRANSFORM_HEADER "AB_ANDROID_MP3_DCT12_V1 1024\n"
#define TRANSFORM_FOOTER "AB MP3 dct12 reference complete\n"
#define TRANSFORM_NAME "short"
#else
#define checked_transform ab_mp3_long_transform
#define TRANSFORM_HEADER "AB_ANDROID_MP3_DCT36_V2 1024\n"
#define TRANSFORM_FOOTER "AB MP3 dct36 reference complete\n"
#define TRANSFORM_NAME "long"
#endif

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "MP3 transform failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float number) {
    uint32_t value;
    memcpy(&value, &number, sizeof(value));
    return value;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare(FILE *file, const float *actual, size_t count, size_t sample,
                    const char *label) {
    uint32_t expected[36];
    CHECK(count <= 36 && words(file, expected, count));
    for (size_t index = 0; index < count; ++index) {
        if (bits(actual[index]) != expected[index]) {
            fprintf(stderr, "%s sample %zu index %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    label, sample, index, bits(actual[index]), expected[index]);
            return false;
        }
    }
    return true;
}

static bool edges(void) {
    AbMp3TransformCoefficients transform;
    CHECK(ab_mp3_transform_init(&transform, NULL) &&
          !ab_mp3_transform_init(NULL, NULL));
    float frequency[18] = {0}, previous[18] = {0}, window[36];
    float output[18], next[18], saved[18];
    for (size_t index = 0; index < 36; ++index) {
        window[index] = 1.0f;
    }
    for (size_t index = 0; index < 18; ++index) {
        previous[index] = (float)index;
        output[index] = -55.0f;
        next[index] = 77.0f;
    }
    CHECK(
        checked_transform(&transform, frequency, previous, window, output, next, NULL));
    for (size_t index = 0; index < 18; ++index) {
        CHECK(output[index] == previous[index] && next[index] == 0.0f);
    }
    memcpy(saved, output, sizeof(saved));
    CHECK(
        !checked_transform(NULL, frequency, previous, window, output, next, NULL) &&
        !checked_transform(&transform, NULL, previous, window, output, next, NULL) &&
        !checked_transform(&transform, frequency, NULL, window, output, next, NULL) &&
        !checked_transform(&transform, frequency, previous, NULL, output, next, NULL) &&
        !checked_transform(&transform, frequency, previous, window, NULL, next, NULL) &&
        !checked_transform(&transform, frequency, previous, window, output, NULL,
                           NULL) &&
        !checked_transform(&transform, frequency, previous, window, output, output,
                           NULL) &&
        memcmp(saved, output, sizeof(saved)) == 0);
    frequency[0] = INFINITY;
    CHECK(!checked_transform(&transform, frequency, previous, window, output, next,
                             NULL) &&
          memcmp(saved, output, sizeof(saved)) == 0);
    frequency[0] = FLT_MAX;
    frequency[1] = FLT_MAX;
    CHECK(!checked_transform(&transform, frequency, previous, window, output, next,
                             NULL) &&
          memcmp(saved, output, sizeof(saved)) == 0);
    frequency[0] = 0.0f;
    frequency[1] = 0.0f;
    transform.cosine[2] = NAN;
    CHECK(!checked_transform(&transform, frequency, previous, window, output, next,
                             NULL) &&
          memcmp(saved, output, sizeof(saved)) == 0);
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) && strcmp(line, TRANSFORM_HEADER) == 0);
    AbMp3TransformCoefficients transform;
    CHECK(ab_mp3_transform_init(&transform, NULL));
#if !defined(AB_TEST_MP3_SHORT)
    CHECK(compare(file, transform.cosine, 9, 0, "Cosine coefficients") &&
          compare(file, transform.half_cosine, 9, 0, "Half-cosine coefficients"));
    float first[] = {transform.cosine[3], transform.cosine[6]};
    float second[] = {transform.cosine[2], -transform.cosine[8], -transform.cosine[4],
                      transform.cosine[1], -transform.cosine[7], -transform.cosine[5]};
    CHECK(compare(file, first, 2, 0, "Static factors") &&
          compare(file, second, 6, 0, "Cosine factors"));
#endif
    for (size_t sample = 0; sample < AB_TEST_MP3_TRANSFORM_CASES; ++sample) {
        uint32_t expected_sample;
        float frequency[18], previous[18], window[36], output[18], next[18];
        mp3_transform_case(sample, frequency, previous, window);
        CHECK(words(file, &expected_sample, 1) && expected_sample == sample &&
              compare(file, frequency, 18, sample, "Frequency input") &&
              compare(file, previous, 18, sample, "Overlap input") &&
              compare(file, window, 36, sample, "Window input") &&
              checked_transform(&transform, frequency, previous, window, output, next,
                                NULL) &&
              compare(file, output, 18, sample, "Hybrid samples") &&
              compare(file, next, 18, sample, "Next overlap"));
    }
    CHECK(fgets(line, sizeof(line), file) && strcmp(line, TRANSFORM_FOOTER) == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared 1024 native %s transforms and 36864 output/overlap floats\n",
           TRANSFORM_NAME);
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
        fprintf(stderr, "Usage: %s [NATIVE_CORPUS]\n", argv[0]);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
