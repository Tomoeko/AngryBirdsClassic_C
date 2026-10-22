#include "ab/rubberband.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Rubberband check failed at %d\n", __LINE__);              \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t word) {
    float result;
    memcpy(&result, &word, sizeof(result));
    return result;
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edges(void) {
    AbVec2 output[4];
    memset(output, 0x5a, sizeof(output));
    uint8_t retained[sizeof(output)];
    memcpy(retained, output, sizeof(output));
    bool drawn = true;
    AbError error = {0};
    AbRubberband band = {.width = NAN};
    CHECK(ab_rubberband_vertices(&band, output, &drawn, &error) && !drawn &&
          memcmp(output, retained, sizeof(output)) == 0);
    drawn = true;
    band.start.x = NAN;
    CHECK(!ab_rubberband_vertices(&band, output, &drawn, &error) && drawn &&
          memcmp(output, retained, sizeof(output)) == 0);
    band = (AbRubberband){{FLT_MAX, 0}, {-FLT_MAX, 0}, 1};
    CHECK(!ab_rubberband_vertices(&band, output, &drawn, &error) && drawn &&
          memcmp(output, retained, sizeof(output)) == 0);
    band = (AbRubberband){{0, 0}, {0x1p-60f, 0}, FLT_MAX};
    CHECK(!ab_rubberband_vertices(&band, output, &drawn, &error) && drawn &&
          memcmp(output, retained, sizeof(output)) == 0);
    band = (AbRubberband){{0x1p100f, 0x1p100f}, {0x1p101f, 0x1p101f}, 1};
    CHECK(ab_rubberband_vertices(&band, output, &drawn, &error) && drawn);
    band = (AbRubberband){{0, 0}, {1, 1}, -0.0f};
    CHECK(ab_rubberband_vertices(&band, output, &drawn, &error) && drawn);
    CHECK(!ab_rubberband_vertices(NULL, output, &drawn, &error));
    CHECK(!ab_rubberband_vertices(&band, NULL, &drawn, &error));
    CHECK(!ab_rubberband_vertices(&band, output, NULL, &error));
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool world_edges(void) {
    AbRubberband world = {{1, -2}, {3, 4}, INFINITY};
    AbRubberband screen;
    memset(&screen, 0x5a, sizeof(screen));
    uint8_t retained[sizeof(screen)];
    memcpy(retained, &screen, sizeof(screen));
    AbError error = {0};
    CHECK(!ab_rubberband_project(NULL, (AbVec2){0}, 1, &screen, &error));
    CHECK(!ab_rubberband_project(&world, (AbVec2){NAN, 0}, 1, &screen, &error));
    CHECK(!ab_rubberband_project(&world, (AbVec2){0}, INFINITY, &screen, &error));
    CHECK(!ab_rubberband_project(&world, (AbVec2){0}, 1, NULL, &error));
    world.start.x = FLT_MAX;
    CHECK(!ab_rubberband_project(&world, (AbVec2){-FLT_MAX, 0}, 0, &screen, &error));
    CHECK(memcmp(retained, &screen, sizeof(screen)) == 0);
    world.start.x = 1;
    CHECK(ab_rubberband_project(&world, (AbVec2){1, -2}, -2, &screen, &error));
    CHECK(screen.start.x == 0 && screen.start.y == 0 && screen.end.x == -4 &&
          screen.end.y == -12 && screen.width == -INFINITY);
    CHECK(ab_rubberband_project(&world, (AbVec2){0}, 0, &world, &error));
    CHECK(isnan(world.width));
    AbVec2 vertices[4];
    memset(vertices, 0x5a, sizeof(vertices));
    uint8_t retained_vertices[sizeof(vertices)];
    memcpy(retained_vertices, vertices, sizeof(vertices));
    bool drawn = true;
    CHECK(ab_rubberband_world_vertices(&world, (AbVec2){0}, -1, vertices, &drawn,
                                       &error) &&
          !drawn);
    CHECK(memcmp(vertices, retained_vertices, sizeof(vertices)) == 0);
    drawn = true;
    CHECK(
        !ab_rubberband_world_vertices(NULL, (AbVec2){0}, 1, vertices, &drawn, &error) &&
        drawn);
    CHECK(memcmp(vertices, retained_vertices, sizeof(vertices)) == 0);
    return true;
}

static bool reference(FILE *file, bool world_space) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, world_space ? "AB_ANDROID_RUBBERBAND_WORLD_V1 4096\n"
                                   : "AB_ANDROID_RUBBERBAND_V1 4096\n") == 0);
    size_t calls = 0, empty = 0, unused_nonfinite = 0;
    const uint32_t expected_uv[] = {0, 0,          0x3f800000, 0,
                                    0, 0x3f800000, 0x3f800000, 0x3f800000};
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[8], count, expected[12], uv[8];
        CHECK(words(file, input, world_space ? 8 : 5));
        AbRubberband band = {{number(input[0]), number(input[1])},
                             {number(input[2]), number(input[3])},
                             number(input[4])};
        AbError error = {0};
        AbVec2 camera = {0};
        float zoom = 1;
        if (world_space) {
            uint32_t projected[5], events[4];
            const uint32_t expected_events[] = {1, 2, 3, 4};
            CHECK(words(file, projected, 5) && words(file, events, 4) &&
                  memcmp(events, expected_events, sizeof(events)) == 0);
            camera = (AbVec2){number(input[5]), number(input[6])};
            zoom = number(input[7]);
            AbRubberband screen;
            CHECK(ab_rubberband_project(&band, camera, zoom, &screen, &error));
            uint32_t actual[] = {bits(screen.start.x), bits(screen.start.y),
                                 bits(screen.end.x), bits(screen.end.y),
                                 bits(screen.width)};
            if (memcmp(actual, projected, sizeof(actual)) != 0) {
                fprintf(stderr, "Rubberband projection differs at case %zu\n", item);
                return false;
            }
        }
        CHECK(words(file, &count, 1) && count <= 1);
        AbVec2 output[4];
        memset(output, 0x5a, sizeof(output));
        uint8_t retained[sizeof(output)];
        memcpy(retained, output, sizeof(output));
        bool drawn = !count;
        bool okay = world_space ? ab_rubberband_world_vertices(&band, camera, zoom,
                                                               output, &drawn, &error)
                                : ab_rubberband_vertices(&band, output, &drawn, &error);
        if (!okay || drawn != (count != 0)) {
            fprintf(stderr, "Native rubberband differs at case %zu: %s\n", item,
                    error.message);
            return false;
        }
        if (!count) {
            CHECK(memcmp(retained, output, sizeof(output)) == 0);
            ++empty;
            unused_nonfinite += !isfinite(band.width);
            continue;
        }
        CHECK(words(file, expected, 12) && words(file, uv, 8) &&
              memcmp(uv, expected_uv, sizeof(uv)) == 0);
        for (size_t vertex = 0; vertex < 4; ++vertex) {
            if (bits(output[vertex].x) != expected[vertex * 3] ||
                bits(output[vertex].y) != expected[vertex * 3 + 1] ||
                expected[vertex * 3 + 2] != 0) {
                fprintf(stderr, "Rubberband vertex differs at case %zu vertex %zu\n",
                        item, vertex);
                return false;
            }
        }
        ++calls;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, world_space ? "AB rubberband world reference complete\n"
                                   : "AB rubberband reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("4096 native rubberbands match: %zu image calls, %zu zero-length cases, "
           "%zu unused nonfinite widths.\n",
           calls, empty, unused_nonfinite);
    return true;
}

int main(int argc, char **argv) {
    bool world_space = argc == 3 && strcmp(argv[1], "--world") == 0;
    if (argc > 3 || (argc == 3 && !world_space)) {
        return 2;
    }
    bool okay = edges() && world_edges();
    if (okay && argc >= 2) {
        FILE *file = fopen(argv[argc - 1], "rb");
        okay = file && reference(file, world_space);
        if (file && fclose(file) != 0) {
            okay = false;
        }
    }
    return okay ? 0 : 1;
}
