#include "ab/camera_viewport.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Viewport failed at line %d: %s\n", __LINE__, #condition); \
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
    AbCameraViewportInput input = {.pixel_size = {480, 320}, .zoom = 1};
    AbCameraViewport output;
    CHECK(ab_camera_viewport(&input, &output));
    CHECK(output.top_left.x == -240 && output.top_left.y == -160 &&
          output.bottom_right.x == 240 && output.bottom_right.y == 160);
    input.zoom = 0;
    output.top_left.x = 19;
    CHECK(!ab_camera_viewport(&input, &output) && output.top_left.x == 19);
    input.zoom = NAN;
    CHECK(!ab_camera_viewport(&input, &output) && output.top_left.x == 19);
    input.zoom = 0.5f;
    input.pixel_size = (AbVec2){number(1), 0};
    CHECK(ab_camera_viewport(&input, &output));
    CHECK(word(output.world_size.x) == 2 && output.top_left.x == 0 &&
          output.bottom_right.x == 0);
    input = (AbCameraViewportInput){.pixel_size = {FLT_MAX, FLT_MAX}, .zoom = 0.25f};
    CHECK(!ab_camera_viewport(&input, &output));
    CHECK(!ab_camera_viewport(NULL, &output) && !ab_camera_viewport(&input, NULL));
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static AbCameraViewportInput parameters(const uint32_t input[7]) {
    return (AbCameraViewportInput){.center = {number(input[0]), number(input[1])},
                                   .pixel_size = {number(input[2]), number(input[3])},
                                   .shake = {number(input[4]), number(input[5])},
                                   .zoom = number(input[6])};
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB_ANDROID_CAMERA_VIEWPORT_V1 4096\n") == 0);
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[7], expected[9];
        CHECK(read_words(file, input, 7) && read_words(file, expected, 9));
        AbCameraViewportInput setup = parameters(input);
        AbCameraViewport viewport;
        CHECK(ab_camera_viewport(&setup, &viewport));
        const uint32_t actual[] = {
            word(viewport.top_left.x),           word(viewport.top_left.y),
            word(viewport.bottom_right.x),       word(viewport.bottom_right.y),
            word(viewport.world_size.x),         word(viewport.world_size.y),
            word(viewport.rendering.top_left.x), word(viewport.rendering.top_left.y)};
        for (size_t field = 0; field < 8; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native viewport %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        CHECK(expected[8] == 8 && viewport.rendering.zoom.x == setup.zoom &&
              viewport.rendering.zoom.y == setup.zoom);
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB camera viewport reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("All 4096 original camera viewport sets match: independent extents, "
         "world dimensions and shake-adjusted rendering origin.");
    return true;
}

static bool live_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB_CAMERA_NATIVE_VIEWPORT_V1 1\n") == 0);
    uint32_t input[7], expected[4];
    CHECK(read_words(file, input, 7) && read_words(file, expected, 4));
    AbCameraViewportInput setup = parameters(input);
    AbCameraViewport viewport;
    CHECK(ab_camera_viewport(&setup, &viewport));
    uint32_t actual[] = {
        word(viewport.rendering.top_left.x), word(viewport.rendering.top_left.y),
        word(viewport.rendering.zoom.x), word(viewport.rendering.zoom.y)};
    CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB camera native viewport complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("All four running native camera fields match the captured Lua viewport "
         "inputs.");
    return true;
}

int main(int argc, char **argv) {
    bool live = argc == 3 && strcmp(argv[1], "--live") == 0;
    if (argc > 2 && !live) {
        fprintf(stderr,
                "Usage: ab_camera_viewport_tests [--live] [NATIVE_VIEWPORT_TRACE]\n");
        return 2;
    }
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    const char *path = argv[live ? 2 : 1];
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror(path);
        return 1;
    }
    bool okay = live ? live_checks(file) : native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
