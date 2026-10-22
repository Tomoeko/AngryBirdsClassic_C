#include "ab/image_rectangle.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Image rectangle check failed at line %d: %s\n", __LINE__, \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t raw(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static const float uv[] = {0, 0, 1, 0, 0, 1, 1, 1};

static bool edge_checks(void) {
    AbSpriteRenderState state = {.scale_x = 1, .scale_y = 1};
    AbImageRectangle rectangle = {0, 0, 480, 320};
    AbDrawQuad quad;
    bool visible = false;
    CHECK(
        ab_image_rectangle_quad(rectangle, state, 480, 320, uv, &quad, &visible, NULL));
    CHECK(visible && quad.vertices[0].x == -1 && quad.vertices[0].y == 1 &&
          quad.vertices[3].x == 1 && quad.vertices[3].y == -1);
    rectangle = (AbImageRectangle){0, 320, 10, 0};
    CHECK(
        ab_image_rectangle_quad(rectangle, state, 480, 320, uv, &quad, &visible, NULL));
    CHECK(!visible);
    state.angle_radians = number(1);
    CHECK(
        ab_image_rectangle_quad(rectangle, state, 480, 320, uv, &quad, &visible, NULL));
    CHECK(visible);
    state.direct_vertices = true;
    CHECK(
        ab_image_rectangle_quad(rectangle, state, 480, 320, uv, &quad, &visible, NULL));
    CHECK(visible && quad.vertices[3].x == 10 && quad.vertices[3].y == 320);
    AbDrawQuad preserved = quad;
    rectangle.width = INFINITY;
    CHECK(!ab_image_rectangle_quad(rectangle, state, 480, 320, uv, &quad, &visible,
                                   NULL));
    CHECK(visible && memcmp(&quad, &preserved, sizeof(quad)) == 0);
    rectangle.width = 10;
    CHECK(
        !ab_image_rectangle_quad(rectangle, state, 0, 320, uv, &quad, &visible, NULL));
    CHECK(!ab_image_rectangle_quad(rectangle, state, 480, 320, NULL, &quad, &visible,
                                   NULL));
    CHECK(!ab_image_rectangle_quad(rectangle, state, 480, 320, uv, &quad, NULL, NULL));
    CHECK(visible && memcmp(&quad, &preserved, sizeof(quad)) == 0);
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, words + index) == 1);
    }
    return true;
}

static bool compare_reference(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB_ANDROID_IMAGE_RECTANGLE_V1 4096\n") == 0);
    size_t rendered = 0, rotated = 0, direct = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[16], expected_visible, positions[18], coordinates[12];
        CHECK(read_words(file, input, 16));
        CHECK(read_words(file, &expected_visible, 1));
        CHECK(expected_visible <= 1);
        if (expected_visible) {
            CHECK(read_words(file, positions, 18));
            CHECK(read_words(file, coordinates, 12));
        }
        AbImageRectangle rectangle = {number(input[0]), number(input[1]),
                                      number(input[2]), number(input[3])};
        AbSpriteRenderState state = {
            number(input[4]), number(input[5]), number(input[6]),  number(input[7]),
            number(input[8]), number(input[9]), number(input[10]), input[11] != 0};
        AbDrawQuad quad;
        bool visible = false;
        CHECK(ab_image_rectangle_quad(rectangle, state, input[12], input[13], uv, &quad,
                                      &visible, NULL));
        if (visible != (expected_visible != 0)) {
            fprintf(stderr, "Case %zu visibility %d expected %u\n", item, visible,
                    expected_visible);
            return false;
        }
        if (!visible) {
            continue;
        }
        ++rendered;
        rotated += !state.direct_vertices && state.angle_radians != 0;
        direct += state.direct_vertices;
        const size_t indices[] = {0, 1, 2, 2, 1, 3};
        for (size_t index = 0; index < 6; ++index) {
            AbDrawVertex vertex = quad.vertices[indices[index]];
            uint32_t actual[] = {raw(vertex.x), raw(vertex.y),
                                 state.direct_vertices ? 0 : UINT32_C(0x3a83126f),
                                 raw(vertex.u), raw(vertex.v)};
            uint32_t expected[] = {positions[index * 3], positions[index * 3 + 1],
                                   positions[index * 3 + 2], coordinates[index * 2],
                                   coordinates[index * 2 + 1]};
            for (size_t field = 0; field < 5; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "Case %zu vertex %zu field %zu: %08" PRIx32
                            " expected %08" PRIx32 "\n",
                            item, index, field, actual[field], expected[field]);
                    return false;
                }
            }
        }
    }
    CHECK(fgetc(file) == '\n');
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB image rectangle reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("Matched 4096 native rectangle cases: %zu drawn, %zu rotated, %zu direct\n",
           rendered, rotated, direct);
    return true;
}

int main(int argc, char **argv) {
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare_reference(file);
    if (fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
