#include "ab/game_collision.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    AbGameCollisionMatrix matrix = {0};
    matrix.allowed[1][2] = true;
    AbGameCollisionObject first = {1, true}, second = {2, true};
    AbCollisionFilter filter = {1, 0xffff, 0};
    CHECK(ab_game_collision_allowed(filter, filter, &first, &second, &matrix));
    CHECK(!ab_game_collision_allowed(filter, filter, &second, &first, &matrix));
    second.enabled = false;
    CHECK(!ab_game_collision_allowed(filter, filter, &first, &second, &matrix));
    CHECK(ab_game_collision_allowed(filter, filter, NULL, &second, NULL));
    CHECK(ab_game_collision_allowed(filter, filter, &first, NULL, NULL));
    CHECK(!ab_game_collision_allowed(filter, filter, &first, &second, NULL));
    filter.group = 1;
    filter.mask = 0;
    first.collider = UINT32_MAX;
    CHECK(ab_game_collision_allowed(filter, filter, &first, &second, NULL));
    filter.group = -1;
    CHECK(!ab_game_collision_allowed(filter, filter, NULL, NULL, NULL));
    filter.group = 0;
    CHECK(!ab_game_collision_allowed(filter, filter, NULL, NULL, NULL));
    filter.mask = 0xffff;
    CHECK(!ab_game_collision_allowed(filter, filter, &first, &second, &matrix));
    AbGameCollisionMatrix unchanged = matrix;
    CHECK(!ab_game_collision_set(&matrix, 12, 0, true));
    CHECK(memcmp(&matrix, &unchanged, sizeof(matrix)) == 0);
    CHECK(!ab_game_collision_set(NULL, 0, 0, true));
    CHECK(!ab_game_collision_setup(NULL));
    CHECK(!ab_game_collision_initialize(NULL));
    return true;
}

static bool compare_rule_rows(FILE *file, const AbGameCollisionMatrix *matrix,
                              size_t item, const char *stage) {
    for (size_t first = 0; first < AB_GAME_COLLIDER_TYPES; ++first) {
        uint32_t expected, actual = 0;
        CHECK(fscanf(file, "%8" SCNx32, &expected) == 1);
        for (size_t second = 0; second < AB_GAME_COLLIDER_TYPES; ++second) {
            actual |= (uint32_t)matrix->allowed[first][second] << second;
        }
        if (actual != expected) {
            fprintf(stderr,
                    "Game rules case %zu %s row %zu: %03" PRIx32 " != %03" PRIx32 "\n",
                    item, stage, first, actual, expected);
            return false;
        }
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_rule_checks(FILE *file) {
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[15];
        for (size_t field = 0; field < 15; ++field) {
            CHECK(fscanf(file, "%8" SCNx32, input + field) == 1);
        }
        CHECK(fgetc(file) == '\n' && input[12] < 12 && input[13] < 12 &&
              input[14] <= 1);
        AbGameCollisionMatrix matrix = {0};
        for (size_t first = 0; first < AB_GAME_COLLIDER_TYPES; ++first) {
            CHECK(input[first] <= 0xfff);
            for (size_t second = 0; second < AB_GAME_COLLIDER_TYPES; ++second) {
                matrix.allowed[first][second] =
                    (input[first] & (UINT32_C(1) << second)) != 0;
            }
        }
        CHECK(ab_game_collision_setup(&matrix));
        CHECK(compare_rule_rows(file, &matrix, item, "setup"));
        if (!item) {
            AbGameCollisionMatrix initial;
            CHECK(ab_game_collision_initialize(&initial));
            CHECK(memcmp(&matrix, &initial, sizeof(matrix)) == 0);
        }
        CHECK(ab_game_collision_set(&matrix, input[12], input[13], input[14] != 0));
        CHECK(compare_rule_rows(file, &matrix, item, "pair update"));
    }
    char marker[64];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB game rules reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("All 4096 native collision-rule setups and pair updates match.");
    return true;
}

static AbCollisionFilter read_filter(const uint32_t input[3]) {
    uint16_t bits = (uint16_t)input[2];
    int16_t group;
    memcpy(&group, &bits, sizeof(group));
    return (AbCollisionFilter){(uint16_t)input[0], (uint16_t)input[1], group};
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file));
    if (strcmp(header, "AB_ANDROID_GAME_RULES_V1 4096\n") == 0) {
        return native_rule_checks(file);
    }
    CHECK(strcmp(header, "AB_ANDROID_GAME_COLLISION_V1 4096\n") == 0);
    size_t permitted = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[24], expected;
        for (size_t field = 0; field < 24; ++field) {
            CHECK(fscanf(file, "%8" SCNx32, input + field) == 1);
        }
        CHECK(fgetc(file) == '\n');
        CHECK(fscanf(file, "%8" SCNx32, &expected) == 1 && expected <= 1);
        CHECK(fgetc(file) == '\n');
        AbGameCollisionObject first = {input[6], input[8] != 0};
        AbGameCollisionObject second = {input[7], input[9] != 0};
        AbGameCollisionMatrix matrix = {0};
        for (size_t row = 0; row < AB_GAME_COLLIDER_TYPES; ++row) {
            CHECK(input[12 + row] <= 0xfff);
            for (size_t column = 0; column < AB_GAME_COLLIDER_TYPES; ++column) {
                matrix.allowed[row][column] =
                    (input[12 + row] & (UINT32_C(1) << column)) != 0;
            }
        }
        bool actual = ab_game_collision_allowed(
            read_filter(input), read_filter(input + 3), input[10] ? &first : NULL,
            input[11] ? &second : NULL, &matrix);
        if (actual != (expected != 0)) {
            fprintf(stderr, "Game collision case %zu: %u != %u\n", item, actual,
                    expected);
            return false;
        }
        permitted += actual;
    }
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB game collision reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native game collision decisions match (%zu permitted).\n",
           permitted);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_game_collision_tests [NATIVE_TRACE]\n");
        return 2;
    }
    bool okay = edge_checks();
    if (okay && argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        if (!file) {
            perror(argv[1]);
            return 1;
        }
        okay = native_checks(file);
        if (fclose(file) != 0) {
            okay = false;
        }
    }
    return okay ? 0 : 1;
}
