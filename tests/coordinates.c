#include "ab/coordinates.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Coordinates failed at line %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t word(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edge_checks(void) {
    AbVec2 output = {19, 20};
    AbCoordinateCamera camera = {.zoom = {1, 1}};
    CHECK(!ab_coordinate_point(NULL, AB_SPACE_SCREEN, AB_SPACE_WORLD, (AbVec2){0},
                               &output));
    CHECK(output.x == 19 && output.y == 20);
    CHECK(!ab_coordinate_point(&camera, (AbCoordinateSpace)-1, AB_SPACE_WORLD,
                               (AbVec2){0}, &output));
    CHECK(!ab_coordinate_point(&camera, AB_SPACE_WORLD, AB_SPACE_PHYSICS,
                               (AbVec2){NAN, 0}, &output));
    CHECK(!ab_coordinate_point(&camera, AB_SPACE_PHYSICS, AB_SPACE_WORLD,
                               (AbVec2){FLT_MAX, 0}, &output));
    CHECK(ab_coordinate_point(NULL, AB_SPACE_WORLD, AB_SPACE_PHYSICS, (AbVec2){1, -1},
                              &output));
    CHECK(word(output.x) == word(0.05f) && word(output.y) == word(-0.05f));
    camera.zoom.x = 0;
    CHECK(!ab_coordinate_point(&camera, AB_SPACE_SCREEN, AB_SPACE_WORLD, (AbVec2){0},
                               &output));
    CHECK(ab_coordinate_point(&camera, AB_SPACE_WORLD, AB_SPACE_SCREEN, (AbVec2){1, 0},
                              &output));
    CHECK(output.x == 0 && output.y == 0);
    camera = (AbCoordinateCamera){.top_left = {33554432, -33554432}, .zoom = {1, 1}};
    CHECK(ab_coordinate_distance(&camera, AB_SPACE_SCREEN, AB_SPACE_WORLD,
                                 (AbVec2){1, 1}, &output));
    CHECK(output.x == 0 && output.y == 0);
    float scale = 19;
    camera.zoom.x = 0;
    CHECK(!ab_coordinate_screen_scale(&camera, 1, &scale) && scale == 19);
    camera = (AbCoordinateCamera){.top_left = {NAN, NAN}, .zoom = {2, NAN}};
    CHECK(ab_coordinate_screen_scale(&camera, 1, &scale) && scale == 0.025f);
    CHECK(!ab_coordinate_distance(NULL, AB_SPACE_WORLD, AB_SPACE_PHYSICS, (AbVec2){0},
                                  NULL));
    CHECK(!ab_coordinate_screen_scale(NULL, 1, &scale));
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
    CHECK(strcmp(marker, "AB_ANDROID_COORDINATES_V1 4096\n") == 0);
    static const AbCoordinateSpace spaces[6][2] = {
        {AB_SPACE_WORLD, AB_SPACE_PHYSICS},  {AB_SPACE_PHYSICS, AB_SPACE_WORLD},
        {AB_SPACE_SCREEN, AB_SPACE_WORLD},   {AB_SPACE_WORLD, AB_SPACE_SCREEN},
        {AB_SPACE_PHYSICS, AB_SPACE_SCREEN}, {AB_SPACE_SCREEN, AB_SPACE_PHYSICS}};
    size_t zeros = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[7], expected[25], actual[25];
        CHECK(read_words(file, input, 7) && read_words(file, expected, 25));
        AbCoordinateCamera camera = {.top_left = {number(input[0]), number(input[1])},
                                     .zoom = {number(input[2]), number(input[3])}};
        AbVec2 point = {number(input[4]), number(input[5])};
        for (size_t operation = 0; operation < 12; ++operation) {
            AbVec2 result;
            AbCoordinateSpace source = spaces[operation % 6][0];
            AbCoordinateSpace target = spaces[operation % 6][1];
            CHECK(
                operation < 6
                    ? ab_coordinate_point(&camera, source, target, point, &result)
                    : ab_coordinate_distance(&camera, source, target, point, &result));
            actual[2 * operation] = word(result.x);
            actual[2 * operation + 1] = word(result.y);
        }
        float scalar;
        CHECK(ab_coordinate_screen_scale(&camera, number(input[6]), &scalar));
        actual[24] = word(scalar);
        for (size_t field = 0; field < 25; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native coordinates %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
            zeros += (actual[field] & UINT32_C(0x7fffffff)) == 0;
        }
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB coordinates reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native camera coordinate sets match: six point conversions, "
           "six transformed-origin deltas and one scalar; %zu zero components.\n",
           zeros);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_coordinate_tests [NATIVE_COORDINATE_TRACE]\n");
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
