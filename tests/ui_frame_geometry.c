#include "ab/ui_frame_geometry.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI geometry failed at %d: %s\n", __LINE__, #condition);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static float number(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static bool edge_checks(void) {
    AbUiFrameGeometry parent = {
        .position = {10, 20}, .scale = {3, 4}, .scroll = {2, 7}, .scroll_present = 3};
    AbUiFrameGeometry child = {.parent = &parent,
                               .position = {3, 4},
                               .scale = {2, -1},
                               .scroll = {900, 800},
                               .scroll_present = 3};
    AbVec2 result;
    CHECK(ab_ui_frame_screen_position(&child, 2, &result) && result.x == 11 &&
          result.y == 17 && ab_ui_frame_cumulative_scale(&child, 2, &result) &&
          result.x == 6 && result.y == -4);
    parent.scroll_present = 1;
    parent.scroll.y = NAN;
    CHECK(ab_ui_frame_screen_position(&child, 2, &result) && result.x == 13 &&
          result.y == 24);
    parent.scroll_present = 3;
    CHECK(!ab_ui_frame_screen_position(&child, 2, &result) && result.x == 13 &&
          result.y == 24 && !ab_ui_frame_screen_position(&child, 1, &result));
    CHECK(ab_ui_frame_cumulative_scale(NULL, 0, &result) && result.x == 1 &&
          result.y == 1 && !ab_ui_frame_screen_position(NULL, 1, &result));
    parent.parent = &child;
    CHECK(!ab_ui_frame_cumulative_scale(&child, 4, &result));
    AbUiInvisibleGeometry rectangle = {.position = {10, 20}, .size = {100, 80}};
    bool hit;
    CHECK(ab_ui_invisible_hit(&rectangle, (AbVec2){-40, -20}, &hit) && hit &&
          ab_ui_invisible_hit(&rectangle, (AbVec2){60, 60}, &hit) && hit &&
          ab_ui_invisible_hit(&rectangle, (AbVec2){60.5f, 20}, &hit) && !hit);
    rectangle.scale_present = 3;
    rectangle.scale = (AbVec2){-1, 1};
    CHECK(ab_ui_invisible_hit(&rectangle, rectangle.position, &hit) && !hit);
    rectangle.scale = (AbVec2){0, 0};
    CHECK(ab_ui_invisible_hit(&rectangle, rectangle.position, &hit) && hit &&
          ab_ui_invisible_hit(&rectangle, (AbVec2){11, 20}, &hit) && !hit);
    rectangle.scale.x = NAN;
    CHECK(!ab_ui_invisible_hit(&rectangle, rectangle.position, &hit) && !hit);
    rectangle.scale_present = 0;
    CHECK(ab_ui_invisible_hit(&rectangle, rectangle.position, &hit) && hit &&
          !ab_ui_invisible_hit(NULL, (AbVec2){0}, &hit) &&
          !ab_ui_invisible_hit(&rectangle, (AbVec2){0}, NULL));
    puts("UI ancestor position/scroll, separate cumulative scale, inclusive hit "
         "edges, signed/absent scale and bounded-chain checks pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_FRAME_GEOMETRY_V1 4096 8\n") == 0);
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t setup[8];
        CHECK(words(file, setup, 8) && setup[0] >= 1 && setup[0] <= 8 && setup[1] <= 3);
        AbUiFrameGeometry frames[8];
        for (size_t index = 0; index < setup[0]; ++index) {
            uint32_t input[7];
            CHECK(words(file, input, 7) && input[4] <= 7);
            frames[index] = (AbUiFrameGeometry){
                .parent = index + 1 < setup[0] ? frames + index + 1 : NULL,
                .position = {number(input[0]), number(input[1])},
                .scale = {number(input[2]), number(input[3])},
                .scroll = {number(input[5]), number(input[6])},
                .scroll_present = input[4] & 4 ? input[4] & 3 : 0};
        }
        uint32_t expected[6];
        AbVec2 position, scale, nil_scale;
        CHECK(words(file, expected, 6) &&
              ab_ui_frame_screen_position(frames, 8, &position) &&
              ab_ui_frame_cumulative_scale(frames, 8, &scale) &&
              ab_ui_frame_cumulative_scale(NULL, 0, &nil_scale));
        uint32_t actual[] = {bits(position.x), bits(position.y),  bits(scale.x),
                             bits(scale.y),    bits(nil_scale.x), bits(nil_scale.y)};
        for (size_t field = 0; field < 6; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "UI geometry case %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        AbUiInvisibleGeometry geometry = {
            .position = {number(setup[2]), number(setup[3])},
            .size = {number(setup[4]), number(setup[5])},
            .scale = {number(setup[6]), number(setup[7])},
            .scale_present = setup[1]};
        for (size_t query = 0; query < 8; ++query) {
            uint32_t point[3];
            bool hit;
            CHECK(words(file, point, 3) && point[2] <= 1 &&
                  ab_ui_invisible_hit(
                      &geometry, (AbVec2){number(point[0]), number(point[1])}, &hit) &&
                  (uint32_t)hit == point[2]);
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI frame geometry reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 4096 original frame position/scale chains and 32768 invisible-button "
         "rectangle queries match.");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare_native(file);
    return fclose(file) == 0 && okay ? 0 : 1;
}
