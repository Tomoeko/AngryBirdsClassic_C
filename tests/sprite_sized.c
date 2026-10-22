#include "ab/draw.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Sized Sprite failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float output;
    memcpy(&output, &bits, sizeof(output));
    return output;
}

static uint32_t bits(float value) {
    uint32_t output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static bool edge_checks(void) {
    AbSpriteRegion region = {
        .x = 1, .y = 2, .width = 9, .height = 7, .pivot_x = -2, .pivot_y = 3};
    AbSpriteSizedPlacement placement = {
        5, 6, 100, 200, AB_ANCHOR_VCENTER, AB_ANCHOR_HCENTER};
    AbDrawQuad quad;
    CHECK(ab_sprite_sized_quad(&region, 32, 32, placement, &quad, NULL) &&
          quad.vertices[0].x == 1 && quad.vertices[0].y == 3 &&
          quad.vertices[3].x == 101 && quad.vertices[3].y == 203 &&
          quad.vertices[0].u == 1.0f / 32 && quad.vertices[3].u == 10.0f / 32);
    placement.horizontal = AB_ANCHOR_HPIVOT;
    placement.vertical = AB_ANCHOR_VPIVOT;
    placement.width = -100;
    placement.height = 0;
    CHECK(ab_sprite_sized_quad(&region, 32, 32, placement, &quad, NULL) &&
          quad.vertices[0].x == 6 && quad.vertices[0].y == 3 &&
          quad.vertices[3].x == -94 && quad.vertices[3].y == 3);
    AbDrawQuad saved = quad;
    placement.width = NAN;
    CHECK(!ab_sprite_sized_quad(&region, 32, 32, placement, &quad, NULL) &&
          memcmp(&quad, &saved, sizeof(quad)) == 0);
    placement.width = FLT_MAX;
    placement.x = FLT_MAX;
    CHECK(!ab_sprite_sized_quad(&region, 32, 32, placement, &quad, NULL) &&
          memcmp(&quad, &saved, sizeof(quad)) == 0);
    puts("Sized Sprite unscaled anchors, rounded negative pivot, full UVs, "
         "signed/zero sizes and failed-output preservation pass.");
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
          strcmp(header, "AB_ANDROID_SPRITE_SIZED_V1 4096\n") == 0);
    size_t negative_widths = 0, negative_heights = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[14], expected[12];
        CHECK(words(file, input, 14) && words(file, expected, 12) && input[12] < 5 &&
              input[13] < 4);
        AbSpriteRegion region = {.x = (int16_t)input[0],
                                 .y = (int16_t)input[1],
                                 .width = (int16_t)input[2],
                                 .height = (int16_t)input[3],
                                 .pivot_x = (int16_t)input[4],
                                 .pivot_y = (int16_t)input[5]};
        AbSpriteSizedPlacement placement = {number(input[8]),
                                            number(input[9]),
                                            number(input[10]),
                                            number(input[11]),
                                            (AbVerticalAnchor)input[12],
                                            (AbHorizontalAnchor)input[13]};
        AbDrawQuad quad;
        CHECK(
            ab_sprite_sized_quad(&region, input[6], input[7], placement, &quad, NULL));
        float left = number(expected[0]), top = number(expected[1]);
        float right = left + number(expected[2]);
        float bottom = top + number(expected[3]);
        const float x[] = {left, right, left, right};
        const float y[] = {top, top, bottom, bottom};
        for (size_t vertex = 0; vertex < 4; ++vertex) {
            const AbDrawVertex *point = quad.vertices + vertex;
            uint32_t actual[] = {bits(point->x), bits(point->y), bits(point->u),
                                 bits(point->v)};
            uint32_t reference[] = {bits(x[vertex]), bits(y[vertex]),
                                    expected[4 + vertex * 2], expected[5 + vertex * 2]};
            for (size_t field = 0; field < 4; ++field) {
                if (actual[field] != reference[field]) {
                    fprintf(stderr,
                            "Sized Sprite %zu vertex %zu field %zu: "
                            "%08" PRIx32 " != %08" PRIx32 "\n",
                            item, vertex, field, actual[field], reference[field]);
                    return false;
                }
            }
        }
        negative_widths += placement.width < 0;
        negative_heights += placement.height < 0;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB sprite sized reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original sized Sprite rectangles and UVs match, including "
           "%zu negative widths and %zu negative heights.\n",
           negative_widths, negative_heights);
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
