#include "ab/mask_draw.h"
#include "mask_draw_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static float real(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool words(FILE *file, uint32_t *values, size_t count) {
    for (size_t word = 0; word < count; ++word)
        if (fscanf(file, "%" SCNx32, values + word) != 1 ||
            fgetc(file) != (word + 1 == count ? '\n' : ' '))
            return false;
    return true;
}

static bool compare(FILE *file) {
    char line[80];
    if (!fgets(line, sizeof(line), file) ||
        strcmp(line, "AB_ANDROID_MASK_DRAW_V1 2048\n"))
        return false;
    size_t drawn = 0;
    for (uint32_t sample = 0; sample < AB_MASK_DRAW_CASES; ++sample) {
        uint32_t input[AB_MASK_DRAW_INPUT_WORDS], generated[AB_MASK_DRAW_INPUT_WORDS];
        uint32_t count, positions[24], texture[24], mask_uv[24];
        mask_draw_case(sample, generated);
        if (!words(file, input, AB_MASK_DRAW_INPUT_WORDS) ||
            memcmp(input, generated, sizeof(input)) || !words(file, &count, 1) ||
            (count && count != 6))
            return false;
        AbSpriteRegion mask = {.x = (int16_t)input[6],
                               .y = (int16_t)input[7],
                               .width = (int16_t)input[8],
                               .height = (int16_t)input[9],
                               .pivot_x = (int16_t)real(input[10]),
                               .pivot_y = (int16_t)real(input[11])};
        AbMaskDrawPlan plan;
        AbMaskDrawInput request = {
            .mask = &mask,
            .position = {real(input[12]), real(input[13])},
            .scale = {real(input[16]), real(input[17])},
            .rotation_origin = {real(input[21]), real(input[22])},
            .transform = {real(input[18]), -real(input[19]), real(input[14]),
                          real(input[19]), real(input[18]), real(input[15])},
            .width = input[0],
            .height = input[1],
            .texture_width = input[2],
            .texture_height = input[3],
            .mask_width = input[4],
            .mask_height = input[5]};
        if (!ab_mask_draw_plan(request, &plan, NULL) || plan.visible != (count != 0)) {
            fprintf(stderr, "Mask visibility differs at %u\n", sample);
            return false;
        }
        if (!count)
            continue;
        if (!words(file, positions, 24) || !words(file, texture, 24) ||
            !words(file, mask_uv, 24))
            return false;
        static const size_t order[] = {0, 1, 2, 2, 1, 3};
        for (size_t vertex = 0; vertex < 6; ++vertex) {
            size_t index = order[vertex];
            const AbDrawVertex *value = plan.quad.vertices + index;
            uint32_t actual[] = {mask_draw_float(value->x),
                                 mask_draw_float(value->y),
                                 UINT32_C(0x3a83126f),
                                 UINT32_C(0x3f800000),
                                 mask_draw_float(plan.texture_uv[index * 2]),
                                 mask_draw_float(plan.texture_uv[index * 2 + 1]),
                                 mask_draw_float(value->u),
                                 mask_draw_float(value->v)};
            uint32_t expected[] = {positions[vertex * 4],     positions[vertex * 4 + 1],
                                   positions[vertex * 4 + 2], positions[vertex * 4 + 3],
                                   texture[vertex * 4],       texture[vertex * 4 + 1],
                                   mask_uv[vertex * 4],       mask_uv[vertex * 4 + 1]};
            for (size_t word = 0; word < 8; ++word)
                if (actual[word] != expected[word]) {
                    fprintf(stderr, "Mask case %u vertex %zu word %zu: %08x != %08x\n",
                            sample, vertex, word, actual[word], expected[word]);
                    return false;
                }
        }
        ++drawn;
    }
    if (!fgets(line, sizeof(line), file) ||
        strcmp(line, "AB mask draw reference complete\n") || fgetc(file) != EOF)
        return false;
    printf("Native mask draw: 2048 cases, %zu visible quads and two UV sets.\n", drawn);
    return true;
}

int main(int argc, char **argv) {
    AbMaskDrawPlan retained = {.visible = true};
    if (ab_mask_draw_plan((AbMaskDrawInput){0}, &retained, NULL) || !retained.visible)
        return 1;
    if (argc == 1)
        return 0;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = compare(file);
    if (fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
