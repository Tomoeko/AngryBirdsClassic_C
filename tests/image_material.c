#include "ab/image_points.h"
#include "ab/image_rectangle.h"
#include "shader_resources_fixture.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Image material check failed at %d\n", __LINE__);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static bool edges(void) {
    AbVec2 points[] = {{0, 0}, {480, 0}, {0, 320}, {480, 320}};
    const float uv[] = {0, 0, 1, 0, 0, 1, 1, 1};
    AbDrawCommand command = {0};
    bool visible = false;
    AbError error = {0};
    CHECK(ab_image_points_command(points, 480, 320, uv,
                                  (AbImagePointMaterial){7, false, NAN}, &command,
                                  &visible, &error) &&
          visible && command.blend == AB_BLEND_OPAQUE && command.opacity == 1 &&
          command.texture == 7 && command.coordinates == AB_DRAW_CLIP);
    uint8_t retained[sizeof(command)];
    memcpy(retained, &command, sizeof(command));
    CHECK(!ab_image_points_command(points, 480, 320, uv,
                                   (AbImagePointMaterial){7, true, NAN}, &command,
                                   &visible, &error) &&
          visible && memcmp(retained, &command, sizeof(command)) == 0);
    CHECK(ab_image_points_command(points, 480, 320, uv,
                                  (AbImagePointMaterial){7, true, -0.0f}, &command,
                                  &visible, &error) &&
          command.blend == AB_BLEND_PREMULTIPLIED &&
          bits(command.opacity) == UINT32_C(0x80000000));
    memcpy(retained, &command, sizeof(command));
    for (size_t index = 0; index < 4; ++index) {
        points[index].x += 960;
    }
    CHECK(ab_image_points_command(points, 480, 320, uv,
                                  (AbImagePointMaterial){0, true, NAN}, &command,
                                  &visible, &error) &&
          !visible && memcmp(retained, &command, sizeof(command)) == 0);
    points[0].x = NAN;
    CHECK(!ab_image_points_command(points, 480, 320, uv,
                                   (AbImagePointMaterial){7, true, 1}, &command,
                                   &visible, &error) &&
          !visible && memcmp(retained, &command, sizeof(command)) == 0);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool rectangle_edges(void) {
    AbImageRectangle rectangle = {0, 0, 16, 16};
    AbSpriteRenderState state = {.scale_x = 1, .scale_y = 1};
    const float uv[] = {0, 0, 1, 0, 0, 1, 1, 1};
    AbDrawCommand command = {0};
    bool visible = false;
    AbError error = {0};
    CHECK(ab_image_rectangle_command(rectangle, state, 480, 320, uv,
                                     (AbImageRectangleMaterial){7, false, 0.5f},
                                     &command, &visible, &error) &&
          visible && command.blend == AB_BLEND_PREMULTIPLIED &&
          command.opacity == 0.5f);
    uint8_t retained[sizeof(command)];
    memcpy(retained, &command, sizeof(command));
    CHECK(!ab_image_rectangle_command(rectangle, state, 480, 320, uv,
                                      (AbImageRectangleMaterial){7, true, NAN},
                                      &command, &visible, &error) &&
          visible && memcmp(retained, &command, sizeof(command)) == 0);
    rectangle.x = 960;
    CHECK(ab_image_rectangle_command(rectangle, state, 480, 320, uv,
                                     (AbImageRectangleMaterial){0, true, NAN}, &command,
                                     &visible, &error) &&
          !visible && memcmp(retained, &command, sizeof(command)) == 0);
    return true;
}

static bool compare(FILE *file, bool rectangles) {
    char line[80];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, rectangles ? "AB_ANDROID_IMAGE_MATERIAL_RECTANGLE_V1 4096\n"
                                  : "AB_ANDROID_IMAGE_MATERIAL_V1 4096\n") == 0);
    const float uv[] = {0, 0, 1, 0, 0, 1, 1, 1};
    size_t counts[3] = {0};
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[8], expected[6];
        CHECK(words(file, input, 8) && words(file, expected, 6) && expected[0] <= 1 &&
              expected[1] <= 2 && input[0] < 34);
        const AbVec2 points[] = {{number(input[4]), number(input[5])},
                                 {number(input[6]), number(input[5])},
                                 {number(input[4]), number(input[7])},
                                 {number(input[6]), number(input[7])}};
        AbDrawCommand command = {0};
        memset(&command, 0xa5, sizeof(command));
        uint8_t retained[sizeof(command)];
        memcpy(retained, &command, sizeof(command));
        bool visible;
        AbError error = {0};
        bool okay;
        if (rectangles) {
            AbImageRectangle rectangle = {number(input[4]), number(input[5]), 257, 193};
            AbSpriteRenderState state = {.scale_x = 1, .scale_y = 1};
            okay = ab_image_rectangle_command(
                rectangle, state, input[1], input[2], uv,
                (AbImageRectangleMaterial){7, expected[0] != 0, number(input[3])},
                &command, &visible, &error);
        } else {
            okay = ab_image_points_command(
                points, input[1], input[2], uv,
                (AbImagePointMaterial){7, expected[0] != 0, number(input[3])}, &command,
                &visible, &error);
        }
        CHECK(okay && (uint32_t)visible == expected[5]);
        for (size_t index = 2; index < 5; ++index) {
            CHECK(expected[index] == (uint32_t)visible);
        }
        uint32_t selected = 0;
        if (visible) {
            selected = command.blend == AB_BLEND_PREMULTIPLIED ? 2u : 1u;
            CHECK(command.texture == 7 && command.coordinates == AB_DRAW_CLIP &&
                  bits(command.opacity) == (expected[1] == 2 ? input[3] : bits(1)) &&
                  ab_draw_command_check(&command, &error));
            for (size_t channel = 0; channel < 4; ++channel) {
                CHECK(command.color[channel] == 1);
            }
        } else {
            CHECK(memcmp(retained, &command, sizeof(command)) == 0);
        }
        CHECK(selected == expected[1]);
        ++counts[selected];
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, rectangles ? "AB rectangle image material reference complete\n"
                                  : "AB image material reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    const size_t expected_counts[2][3] = {{3611, 224, 261}, {3369, 152, 575}};
    CHECK(memcmp(counts, expected_counts[rectangles], sizeof(counts)) == 0);
    printf("4096 native %s-image routes match: %zu culls, %zu opaque shaders, "
           "%zu alpha shaders; all twelve original shader-resource hashes match.\n",
           rectangles ? "rectangle" : "point", counts[0], counts[1], counts[2]);
    return true;
}

int main(int argc, char **argv) {
    if (!edges() || !rectangle_edges()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    bool rectangles = argc == 4 && strcmp(argv[1], "--rectangles") == 0;
    if ((argc != 3 && !rectangles) ||
        !native_shader_resources_apk(argv[rectangles ? 2 : 1])) {
        return 2;
    }
    FILE *file = fopen(argv[argc - 1], "rb");
    bool okay = file && compare(file, rectangles);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
