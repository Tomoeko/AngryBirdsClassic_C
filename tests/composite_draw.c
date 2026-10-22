#include "ab/composite_draw.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static float real(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index)
        if (fscanf(file, "%" SCNx32, output + index) != 1)
            return false;
    return true;
}

static bool compare(FILE *file) {
    char line[256];
    size_t count = 0, header_lines = 0;
    while (header_lines++ < 8 && fgets(line, sizeof(line), file))
        if (sscanf(line, "AB_ANDROID_COMPOSITE_DRAW_V1 %zu", &count) == 1)
            break;
    if (count != 2048)
        return false;
    size_t checked = 0;
    for (size_t item = 0; item < count; ++item) {
        uint32_t input[12], raw[4][12], metrics[4];
        if (!words(file, input, 12) || !input[0] || input[0] > 4)
            return false;
        for (size_t index = 0; index < input[0]; ++index)
            if (!words(file, raw[index], 12))
                return false;
        if (!words(file, metrics, 4))
            return false;
        int32_t bounds[4];
        memcpy(bounds, metrics, sizeof(bounds));
        AbSpriteRenderState parent = {.translation_x = real(input[1]),
                                      .translation_y = real(input[2]),
                                      .scale_x = real(input[3]),
                                      .scale_y = real(input[4]),
                                      .angle_radians = real(input[5]),
                                      .rotation_origin_x = real(input[6]),
                                      .rotation_origin_y = real(input[7])};
        AbVec2 position = {real(input[8]), real(input[9])};
        if (input[10] == AB_ANCHOR_TOP)
            position.y += (float)bounds[3];
        else if (input[10] == AB_ANCHOR_VCENTER)
            position.y += (float)(bounds[3] - bounds[1] / 2);
        else if (input[10] == AB_ANCHOR_BOTTOM)
            position.y += (float)(bounds[3] - bounds[1]);
        if (input[11] == AB_ANCHOR_LEFT)
            position.x += (float)bounds[2];
        else if (input[11] == AB_ANCHOR_HCENTER)
            position.x += (float)(bounds[2] - bounds[0] / 2);
        else if (input[11] == AB_ANCHOR_RIGHT)
            position.x += (float)(bounds[2] - bounds[0]);
        for (size_t index = 0; index < input[0]; ++index) {
            uint32_t expected[12];
            if (!words(file, expected, 12))
                return false;
            AbSpriteRegion region = {.width = (int16_t)raw[index][0],
                                     .height = (int16_t)raw[index][1],
                                     .pivot_x = (int16_t)(int32_t)raw[index][2],
                                     .pivot_y = (int16_t)(int32_t)raw[index][3]};
            AbCompositeBoundsPart part = {
                .region = &region,
                .offset = {real(raw[index][4]), real(raw[index][5])},
                .scale = {real(raw[index][6]), real(raw[index][7])},
                .factors = {real(raw[index][8]), real(raw[index][9])},
                .rotation = real(raw[index][10]),
                .enabled = raw[index][11] != 0};
            AbVec2 points[4];
            AbError error = {0};
            if (!ab_composite_draw_points(&part, parent, position, points, &error)) {
                fprintf(stderr, "%s\n", error.message);
                return false;
            }
            for (size_t vertex = 0; vertex < 4; ++vertex)
                for (size_t axis = 0; axis < 2; ++axis) {
                    float value = axis ? points[vertex].y : points[vertex].x;
                    uint32_t actual;
                    memcpy(&actual, &value, sizeof(actual));
                    if (actual != expected[vertex * 3 + axis]) {
                        fprintf(stderr,
                                "Composite draw %zu/%zu vertex%zu axis%zu: %08" PRIx32
                                " != %08" PRIx32 "\n",
                                item, index, vertex, axis, actual,
                                expected[vertex * 3 + axis]);
                        return false;
                    }
                }
            ++checked;
        }
    }
    printf(
        "Original composite draw: %zu cases, %zu ordered part quads match exactly.\n",
        count, checked);
    return true;
}

static bool edges(void) {
    AbSpriteRegion region = {.width = 8, .height = 4, .pivot_x = 2, .pivot_y = 1};
    AbCompositeBoundsPart part = {.region = &region,
                                  .offset = {3, 5},
                                  .scale = {2, 3},
                                  .factors = {1, 1},
                                  .enabled = true};
    AbSpriteRenderState parent = {.scale_x = 2, .scale_y = 4, .translation_x = 1};
    AbVec2 points[4];
    if (!ab_composite_draw_points(&part, parent, (AbVec2){0}, points, NULL) ||
        points[0].x != 0 || points[0].y != 8 || points[3].x != 32 || points[3].y != 56)
        return false;
    AbVec2 previous[4];
    memcpy(previous, points, sizeof(points));
    parent.angle_radians = 1;
    return !ab_composite_draw_points(&part, parent, (AbVec2){0}, points, NULL) &&
           !memcmp(previous, points, sizeof(points));
}

int main(int argc, char **argv) {
    if (argc == 1)
        return edges() ? 0 : 1;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = compare(file);
    return fclose(file) == 0 && okay ? 0 : 1;
}
