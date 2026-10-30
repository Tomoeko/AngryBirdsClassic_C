#include "ab/physics_matrix.h"

#include "matrix_solve_cases.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Matrix solve failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edges(void) {
    AbSymmetric3 matrix = {.xx = 1, .yy = 2, .zz = 4};
    AbVec2 output2;
    AbVec3 output3 = {0};
    CHECK(ab_symmetric3_solve22(&matrix, (AbVec2){1, 2}, &output2, NULL) &&
          output2.x == 1 && output2.y == 1 &&
          ab_symmetric3_solve(&matrix, (AbVec3){1, 2, 3}, &output3, NULL) &&
          output3.x == 1 && output3.y == 1 && output3.z == 0.75f);
    matrix = (AbSymmetric3){0};
    CHECK(ab_symmetric3_solve22(&matrix, (AbVec2){1, -2}, &output2, NULL) &&
          output2.x == 0 && output2.y == 0 &&
          ab_symmetric3_solve(&matrix, (AbVec3){1, -2, 3}, &output3, NULL) &&
          output3.x == 0 && output3.y == 0 && output3.z == 0);
    matrix = (AbSymmetric3){.xx = 1, .yy = 2, .xz = NAN, .yz = INFINITY, .zz = NAN};
    CHECK(ab_symmetric3_solve22(&matrix, (AbVec2){1, 2}, &output2, NULL) &&
          output2.x == 1 && output2.y == 1);
    AbVec2 before2 = output2;
    AbVec3 before3 = output3;
    CHECK(!ab_symmetric3_solve(&matrix, (AbVec3){1, 2, 3}, &output3, NULL) &&
          !ab_symmetric3_solve22(NULL, (AbVec2){0}, &output2, NULL) &&
          !ab_symmetric3_solve22(&matrix, (AbVec2){0}, NULL, NULL) &&
          !ab_symmetric3_solve(NULL, (AbVec3){0}, &output3, NULL) &&
          !ab_symmetric3_solve(&matrix, (AbVec3){0}, NULL, NULL) &&
          !ab_symmetric3_solve22(&matrix, (AbVec2){NAN, 0}, &output2, NULL));
    matrix = (AbSymmetric3){.xx = FLT_MAX, .xy = FLT_MAX, .yy = FLT_MAX, .zz = 1};
    CHECK(!ab_symmetric3_solve22(&matrix, (AbVec2){1, 2}, &output2, NULL) &&
          !ab_symmetric3_solve(&matrix, (AbVec3){1, 2, 3}, &output3, NULL) &&
          memcmp(&output2, &before2, sizeof(output2)) == 0 &&
          memcmp(&output3, &before3, sizeof(output3)) == 0);
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
          strcmp(line, "AB_ANDROID_MATRIX_SOLVE_V1 4096 2\n") == 0);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t ordinal, encoded[9], generated[9], expected[5];
        CHECK(words(file, &ordinal, 1) && ordinal == sample &&
              words(file, encoded, 9) && words(file, expected, 2) &&
              words(file, expected + 2, 3));
        matrix_solve_case(sample, generated);
        CHECK(memcmp(encoded, generated, sizeof(encoded)) == 0);
        float values[9];
        for (size_t index = 0; index < 9; ++index) {
            memcpy(values + index, encoded + index, sizeof(float));
        }
        AbSymmetric3 matrix = {values[0], values[1], values[2],
                               values[3], values[4], values[5]};
        AbVec2 output2;
        AbVec3 output3;
        CHECK(ab_symmetric3_solve22(&matrix, (AbVec2){values[6], values[7]}, &output2,
                                    NULL) &&
              ab_symmetric3_solve(&matrix, (AbVec3){values[6], values[7], values[8]},
                                  &output3, NULL));
        const float actual[] = {output2.x, output2.y, output3.x, output3.y, output3.z};
        for (size_t index = 0; index < 5; ++index) {
            if (bits(actual[index]) != expected[index]) {
                fprintf(stderr,
                        "Matrix solve sample %" PRIu32 " field %zu: %08" PRIx32
                        " != %08" PRIx32 "\n",
                        sample, index, bits(actual[index]), expected[index]);
                return false;
            }
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB matrix solve reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Compared 8,192 native direct matrix solves and 20,480 float words");
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
        fputs("Usage: ab_matrix_solve_tests [NATIVE_CORPUS]\n", stderr);
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
