#include "ab/fill_rectangle.h"
#include "shader_resources_fixture.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Fill rectangle check failed at %d\n", __LINE__);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float number) {
    uint32_t value;
    memcpy(&value, &number, sizeof(value));
    return value;
}

static int32_t signed_word(uint32_t word) {
    int32_t value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static bool edges(void) {
    AbFillRectangle rectangle = {UINT32_C(0xff0a1339), 240, 160, 240, 160,
                                 AB_FILL_BLEND_AUTO};
    AbDrawCommand command = {0};
    AbError error = {0};
    bool visible;
    CHECK(ab_fill_rectangle_reset_command(rectangle, 480, 320, 7, &command, &visible,
                                          &error) &&
          visible && bits(command.quad.vertices[0].x) == 0 &&
          command.quad.vertices[0].y == 0 && command.blend == AB_BLEND_OPAQUE &&
          command.opacity == 1 && command.coordinates == AB_DRAW_CLIP);
    rectangle.argb &= UINT32_C(0x7fffffff);
    CHECK(ab_fill_rectangle_reset_command(rectangle, 480, 320, 7, &command, &visible,
                                          &error) &&
          command.blend == AB_BLEND_STRAIGHT);
    uint8_t retained[sizeof(command)];
    memcpy(retained, &command, sizeof(command));
    CHECK(!ab_fill_rectangle_reset_command(rectangle, 0, 320, 7, &command, &visible,
                                           &error) &&
          visible && memcmp(retained, &command, sizeof(command)) == 0);
    CHECK(!ab_fill_rectangle_reset_command(rectangle, 480, 320, 0, &command, &visible,
                                           &error) &&
          visible && memcmp(retained, &command, sizeof(command)) == 0);
    rectangle.left = 960;
    CHECK(ab_fill_rectangle_reset_command(rectangle, 480, 320, 0, &command, &visible,
                                          &error) &&
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

static bool compare_words(const uint32_t *actual, const uint32_t *expected,
                          size_t count, size_t item) {
    for (size_t index = 0; index < count; ++index) {
        if (actual[index] != expected[index]) {
            fprintf(stderr, "Fill %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    item, index, actual[index], expected[index]);
            return false;
        }
    }
    return true;
}

static bool compare(FILE *file) {
    char line[80];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_FILL_RECTANGLE_V1 4096\n") == 0);
    size_t counts[3] = {0};
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[8], expected[3];
        CHECK(words(file, input, 8) && words(file, expected, 3) && expected[0] <= 1 &&
              expected[1] <= 2 && expected[2] == bits(1));
        AbFillRectangle rectangle = {input[2],
                                     signed_word(input[3]),
                                     signed_word(input[4]),
                                     signed_word(input[5]),
                                     signed_word(input[6]),
                                     (AbFillBlend)input[7]};
        AbDrawCommand command = {0};
        memset(&command, 0xa5, sizeof(command));
        uint8_t retained[sizeof(command)];
        memcpy(retained, &command, sizeof(command));
        AbError error = {0};
        bool visible;
        CHECK(ab_fill_rectangle_reset_command(rectangle, input[0], input[1], 7,
                                              &command, &visible, &error));
        if ((uint32_t)visible != expected[0]) {
            fprintf(stderr, "Fill %zu visibility differs\n", item);
            return false;
        }
        uint32_t selected = 0;
        if (visible) {
            selected = command.blend == AB_BLEND_STRAIGHT ? 2u : 1u;
            uint32_t actual[42], native[42];
            CHECK(words(file, native, 18) && words(file, native + 18, 24));
            const size_t order[] = {0, 1, 2, 2, 1, 3};
            for (size_t index = 0; index < 6; ++index) {
                const AbDrawVertex *vertex = command.quad.vertices + order[index];
                actual[index * 3] = bits(vertex->x);
                actual[index * 3 + 1] = bits(vertex->y);
                actual[index * 3 + 2] = UINT32_C(0x3a83126f);
                for (size_t channel = 0; channel < 4; ++channel) {
                    actual[18 + index * 4 + channel] =
                        bits(command.color[channel] * 255);
                }
            }
            CHECK(compare_words(actual, native, 42, item) && command.texture == 7 &&
                  command.coordinates == AB_DRAW_CLIP && command.opacity == 1 &&
                  ab_draw_command_check(&command, &error));
        } else {
            CHECK(memcmp(retained, &command, sizeof(command)) == 0);
        }
        CHECK(selected == expected[1]);
        ++counts[selected];
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB fill rectangle reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file) && counts[0] == 3008 &&
          counts[1] == 589 && counts[2] == 499);
    printf("4096 original reset-state fills match: %zu culls, %zu opaque and %zu "
           "alpha CPU batches, exact vertices/raw colors; GPU pixels unverified.\n",
           counts[0], counts[1], counts[2]);
    return true;
}

int main(int argc, char **argv) {
    if (!edges()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if ((argc != 2 && argc != 3) ||
        (argc == 3 && !native_shader_resources_apk(argv[1]))) {
        return 2;
    }
    FILE *file = fopen(argv[argc - 1], "rb");
    bool okay = file && compare(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
