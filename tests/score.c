#include "ab/score.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Score failed at line %d: %s\n", __LINE__, #condition);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static float from_bits(uint32_t value) {
    float output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static bool edge_checks(void) {
    float damage = 19;
    CHECK(!ab_damage_score(NAN, &damage) && damage == 19);
    CHECK(!ab_damage_score(INFINITY, &damage) && damage == 19);
    CHECK(!ab_damage_score(FLT_MAX, &damage) && damage == 19);
    CHECK(!ab_damage_score(1, NULL));
    AbScoreNumber output = {.value = 19, .present = true};
    AbBreakScoreInput input = {.block_increment = NAN};
    CHECK(!ab_break_score(&input, &output) && output.value == 19 && output.present);
    input.merged_override_present = true;
    CHECK(ab_break_score(&input, &output) && !output.present &&
          bits(output.value) == 0);
    input.base_override = (AbScoreNumber){.value = INFINITY, .present = true};
    CHECK(!ab_break_score(&input, &output) && !output.present &&
          bits(output.value) == 0);
    CHECK(!ab_break_score(NULL, &output) && !ab_break_score(&input, NULL));
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
    CHECK(strcmp(marker, "AB_ANDROID_SCORE_V1 4096\n") == 0);
    size_t overrides = 0, missing = 0, goals = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[8], expected[3];
        CHECK(read_words(file, input, 8) && read_words(file, expected, 3));
        CHECK(input[5] <= 4 && input[6] <= 1 && input[7] <= 4);
        AbBreakScoreInput parameters = {
            .block_increment = from_bits(input[0]),
            .goal_increment = from_bits(input[1]),
            .base_override = {.value = from_bits(input[2]), .present = input[6] != 0},
            .merged_override_present = input[5] != 0,
            .level_goal_truthy = input[7] >= 2};
        AbScoreNumber breaking;
        float damaging;
        CHECK(ab_break_score(&parameters, &breaking));
        CHECK(ab_damage_score(from_bits(input[4]), &damaging));
        uint32_t actual[] = {breaking.present, bits(breaking.value), bits(damaging)};
        for (size_t field = 0; field < 3; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native score %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        overrides += parameters.merged_override_present;
        missing += !breaking.present;
        goals += !parameters.merged_override_present && parameters.level_goal_truthy;
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB score reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original break/damage score pairs match: %zu overrides, %zu "
           "absent base values, %zu goal selections.\n",
           overrides, missing, goals);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_score_tests [NATIVE_SCORE_TRACE]\n");
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
