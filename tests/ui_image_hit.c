#include "ab/ui_image_hit.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI image hit failed at %d: %s\n", __LINE__, #condition);  \
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
    AbUiHitRectangle rectangle = {{10, 20}, {8, 6}, 0};
    bool hit = false;
    CHECK(ab_ui_rectangle_hit(&rectangle, (AbVec2){10, 20}, &hit) && hit);
    CHECK(ab_ui_rectangle_hit(&rectangle, (AbVec2){6, 20}, &hit) && !hit);
    CHECK(ab_ui_rectangle_hit(&rectangle, (AbVec2){14, 20}, &hit) && !hit);
    CHECK(ab_ui_rectangle_hit(&rectangle, (AbVec2){10, 17}, &hit) && !hit);
    CHECK(ab_ui_rectangle_hit(&rectangle, (AbVec2){10, 23}, &hit) && !hit);
    rectangle.angle_radians = -0.0f;
    CHECK(ab_ui_rectangle_hit(&rectangle, (AbVec2){10, 20}, &hit) && hit);
    rectangle.size.x = -8;
    CHECK(ab_ui_rectangle_hit(&rectangle, (AbVec2){10, 20}, &hit) && !hit);
    rectangle.size.x = 0;
    CHECK(ab_ui_rectangle_hit(&rectangle, (AbVec2){10, 20}, &hit) && !hit);
    AbUiFrameGeometry parent = {.position = {1000, 2000}, .scale = {-2, -3}};
    AbUiFrameGeometry frame = {
        .parent = &parent, .position = {10, 20}, .scale = {-1, 1}};
    AbUiImageHit button = {.frame = &frame,
                           .sprite_size = {8, 6},
                           .sprite_pivot = {4, 3},
                           .padding_left = 2,
                           .padding_bottom = 1,
                           .image_present = true};
    bool present = false;
    CHECK(ab_ui_image_button_rectangle(&button, 2, &rectangle, &present) && present &&
          rectangle.center.x == 8 && rectangle.center.y == 21.5f &&
          rectangle.size.x == 20 && rectangle.size.y == 21);
    CHECK(ab_ui_image_button_hit(&button, 2, (AbVec2){8, 21.5f}, &hit) && hit);
    AbUiHitRectangle saved = rectangle;
    CHECK(!ab_ui_image_button_rectangle(&button, 1, &rectangle, &present) && present &&
          memcmp(&rectangle, &saved, sizeof(saved)) == 0);
    parent.scale.x = -0.0f;
    CHECK(ab_ui_image_button_rectangle(&button, 2, &rectangle, &present) && !present &&
          memcmp(&rectangle, &saved, sizeof(saved)) == 0);
    CHECK(ab_ui_image_button_hit(&button, 2, (AbVec2){0}, &hit) && !hit);
    parent.scale.x = 2;
    button.angle_radians = 1025;
    present = true;
    CHECK(!ab_ui_image_button_rectangle(&button, 2, &rectangle, &present) && present &&
          memcmp(&rectangle, &saved, sizeof(saved)) == 0);
    button.image_present = false;
    CHECK(ab_ui_image_button_rectangle(&button, 2, &rectangle, &present) && !present);
    saved.angle_radians = NAN;
    hit = true;
    CHECK(!ab_ui_rectangle_hit(&saved, (AbVec2){0}, &hit) && hit &&
          !ab_ui_rectangle_hit(NULL, (AbVec2){0}, &hit) &&
          !ab_ui_image_button_rectangle(NULL, 1, &rectangle, &present));
    puts("UI image strict edges, signed scales, padding, local coordinates, absent "
         "images and failed-output preservation pass.");
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
          strcmp(header, "AB_ANDROID_UI_IMAGE_HIT_V1 4096 10\n") == 0);
    size_t present_count = 0, image_hits = 0, rectangle_hits = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t setup[14], expected[8];
        CHECK(words(file, setup, 14) && setup[0] >= 1 && setup[0] <= 4 &&
              setup[1] <= 1 && setup[2] < 16);
        AbUiFrameGeometry frames[4] = {0};
        for (size_t index = 0; index < setup[0]; ++index) {
            uint32_t scale[2];
            CHECK(words(file, scale, 2));
            frames[index].scale = (AbVec2){number(scale[0]), number(scale[1])};
            frames[index].parent = index + 1 < setup[0] ? frames + index + 1 : NULL;
        }
        frames[0].position = (AbVec2){number(setup[3]), number(setup[4])};
        AbUiImageHit button = {
            .frame = frames,
            .sprite_size = {number(setup[6]), number(setup[7])},
            .sprite_pivot = {number(setup[8]), number(setup[9])},
            .padding_left = setup[2] & 1 ? number(setup[10]) : 0,
            .padding_right = setup[2] & 2 ? number(setup[11]) : 0,
            .padding_top = setup[2] & 4 ? number(setup[12]) : 0,
            .padding_bottom = setup[2] & 8 ? number(setup[13]) : 0,
            .angle_radians = number(setup[5]),
            .image_present = setup[1] != 0,
        };
        AbUiHitRectangle rectangle = {0};
        bool present = false;
        CHECK(words(file, expected, 8) &&
              ab_ui_image_button_rectangle(&button, 4, &rectangle, &present));
        uint32_t actual[] = {present,
                             bits(rectangle.center.x),
                             bits(rectangle.center.y),
                             bits(rectangle.size.x),
                             bits(rectangle.size.y),
                             bits(rectangle.angle_radians),
                             present,
                             present};
        for (size_t field = 0; field < 8; ++field) {
            if (expected[field] != actual[field]) {
                fprintf(stderr,
                        "UI image case %zu rectangle field %zu: %08" PRIx32
                        " != %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        present_count += present;
        for (size_t query = 0; query < 10; ++query) {
            uint32_t point[3];
            bool hit = false;
            CHECK(words(file, point, 3) && point[2] <= 1 &&
                  ab_ui_image_button_hit(
                      &button, 4, (AbVec2){number(point[0]), number(point[1])}, &hit));
            if (hit != (point[2] != 0)) {
                fprintf(stderr, "UI image case %zu query %zu: %u != %u\n", item, query,
                        (unsigned)hit, point[2]);
                return false;
            }
            image_hits += hit;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_RECTANGLE_V1 65536\n") == 0);
    for (size_t item = 0; item < 65536; ++item) {
        uint32_t input[8];
        bool hit;
        CHECK(words(file, input, 8) && input[7] <= 1);
        AbUiHitRectangle rectangle = {{number(input[0]), number(input[1])},
                                      {number(input[2]), number(input[3])},
                                      number(input[4])};
        CHECK(ab_ui_rectangle_hit(&rectangle,
                                  (AbVec2){number(input[5]), number(input[6])}, &hit));
        if (hit != (input[7] != 0)) {
            fprintf(stderr, "UI native rectangle case %zu: %u != %u\n", item,
                    (unsigned)hit, input[7]);
            return false;
        }
        rectangle_hits += hit;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI image hit reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original image geometries (%zu present), 40960 image queries "
           "(%zu hits) and 65536 native rectangles (%zu hits) match.\n",
           present_count, image_hits, rectangle_hits);
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
