#include "ab/physics_matrix.h"

#include "matrix_inverse_cases.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Matrix inverse failed at %d: %s\n", __LINE__,             \
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
    AbSymmetric3 input = {0}, output;
    CHECK(ab_symmetric3_inverse22(&input, &output, NULL) && bits(output.xx) == 0 &&
          bits(output.xy) == UINT32_C(0x80000000) && bits(output.yy) == 0 &&
          bits(output.xz) == 0 && bits(output.yz) == 0 && bits(output.zz) == 0);
    CHECK(ab_symmetric3_inverse(&input, &output, NULL));
    const float zeros[] = {output.xx, output.xy, output.xz,
                           output.yy, output.yz, output.zz};
    for (size_t index = 0; index < 6; ++index) {
        CHECK(bits(zeros[index]) == 0);
    }
    input = (AbSymmetric3){.xx = 1, .yy = 2, .zz = 4};
    CHECK(ab_symmetric3_inverse(&input, &input, NULL) && input.xx == 1 &&
          input.yy == 0.5f && input.zz == 0.25f);
    input = (AbSymmetric3){.xx = 1, .yy = 2, .xz = NAN, .yz = INFINITY, .zz = NAN};
    CHECK(ab_symmetric3_inverse22(&input, &output, NULL) && output.xx == 1 &&
          output.yy == 0.5f && bits(output.xz) == 0 && bits(output.yz) == 0 &&
          bits(output.zz) == 0);
    AbSymmetric3 before = output;
    CHECK(!ab_symmetric3_inverse(&input, &output, NULL) &&
          !ab_symmetric3_inverse(NULL, &output, NULL) &&
          !ab_symmetric3_inverse(&input, NULL, NULL) &&
          !ab_symmetric3_inverse22(NULL, &output, NULL) &&
          !ab_symmetric3_inverse22(&input, NULL, NULL));
    input.xy = NAN;
    CHECK(!ab_symmetric3_inverse22(&input, &output, NULL));
    input = (AbSymmetric3){.xx = FLT_MAX, .xy = FLT_MAX, .yy = FLT_MAX, .zz = 1};
    CHECK(!ab_symmetric3_inverse22(&input, &output, NULL) &&
          !ab_symmetric3_inverse(&input, &output, NULL) &&
          memcmp(&output, &before, sizeof(output)) == 0);
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
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_MATRIX_INVERSE_V1 4096 2\n") == 0);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t ordinal, encoded[6], generated[6];
        CHECK(words(file, &ordinal, 1) && ordinal == sample && words(file, encoded, 6));
        matrix_inverse_case(sample, generated);
        CHECK(memcmp(encoded, generated, sizeof(encoded)) == 0);
        float values[6];
        for (size_t index = 0; index < 6; ++index) {
            memcpy(values + index, encoded + index, sizeof(float));
        }
        AbSymmetric3 input = {values[0], values[1], values[2],
                              values[3], values[4], values[5]};
        for (size_t kind = 0; kind < 2; ++kind) {
            uint32_t expected[9];
            CHECK(words(file, expected, 9));
            AbSymmetric3 output;
            CHECK(kind == 0 ? ab_symmetric3_inverse22(&input, &output, NULL)
                            : ab_symmetric3_inverse(&input, &output, NULL));
            const float actual[] = {output.xx, output.xy, output.xz,
                                    output.xy, output.yy, output.yz,
                                    output.xz, output.yz, output.zz};
            for (size_t index = 0; index < 9; ++index) {
                if (bits(actual[index]) != expected[index]) {
                    fprintf(stderr,
                            "Matrix sample %" PRIu32 " kind %zu field %zu: %08" PRIx32
                            " != %08" PRIx32 "\n",
                            sample, kind, index, bits(actual[index]), expected[index]);
                    return false;
                }
            }
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB matrix inverse reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Compared 8,192 native matrix inversions and 73,728 output float words");
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
        fputs("Usage: ab_matrix_inverse_tests [NATIVE_CORPUS]\n", stderr);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
