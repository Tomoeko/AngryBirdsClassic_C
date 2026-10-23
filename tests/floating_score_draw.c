#include "ab/floating_score_draw.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Floating draw check at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index)
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    return true;
}

static bool edges(void) {
    AbFloatingScoreDrawPlan plan;
    AbCoordinateCamera camera = {.top_left = {20, 40}, .zoom = {2, NAN}};
    CHECK(ab_floating_score_draw_plan((AbVec2){2, 3}, 0, camera, 1, &plan, NULL) &&
          !plan.visible && plan.scale == 0 && plan.screen.x == 40 &&
          plan.screen.y == 40);
    CHECK(ab_floating_score_draw_plan((AbVec2){2, 3}, -1, camera, 2, &plan, NULL) &&
          plan.visible && plan.scale == -2 && plan.text_position.x == -20 &&
          plan.sprite_position.y == -20);
    uint8_t retained[sizeof(plan)];
    memcpy(retained, &plan, sizeof(plan));
    CHECK(!ab_floating_score_draw_plan((AbVec2){NAN, 3}, 1, camera, 1, &plan, NULL) &&
          !ab_floating_score_draw_plan((AbVec2){2, 3}, INFINITY, camera, 1, &plan,
                                       NULL) &&
          !ab_floating_score_draw_plan((AbVec2){2, 3}, 1, camera, NAN, &plan, NULL) &&
          !ab_floating_score_draw_plan((AbVec2){2, 3}, 1, camera, 1, NULL, NULL) &&
          !memcmp(retained, &plan, sizeof(plan)));
    return true;
}

static bool reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_FLOATING_SCORE_DRAW_V1 1024\n"));
    size_t texts = 0, sprites = 0;
    for (size_t sample = 0; sample < 1024; ++sample) {
        uint32_t input[8], count;
        CHECK(words(file, input, 8) && words(file, &count, 1) && count <= 32 &&
              input[7] <= 3);
        AbFloatingScoreDrawPlan plan;
        CHECK(ab_floating_score_draw_plan(
            (AbVec2){number(input[0]), number(input[1])}, number(input[6]),
            (AbCoordinateCamera){.top_left = {number(input[2]), number(input[3])},
                                 .zoom = {number(input[4]), NAN}},
            number(input[5]), &plan, NULL));
        uint32_t scale_x = 0, scale_y = 0, seen = 0;
        for (size_t event = 0; event < count; ++event) {
            uint32_t call[12];
            CHECK(words(file, call, 12) && call[0] >= 1 && call[0] <= 7);
            if (call[0] == 2) {
                CHECK(call[1] == 4 && call[2] == 0 && call[3] == 0);
                scale_x = call[4];
                scale_y = call[5];
            } else if (call[0] == 6 || call[0] == 7) {
                bool sprite = call[0] == 7;
                uint32_t flag = sprite ? 2u : 1u;
                AbVec2 position = sprite ? plan.sprite_position : plan.text_position;
                if (!(input[7] & flag) || (seen & flag) || (!sprite && (seen & 2)) ||
                    scale_x != bits(plan.scale) || scale_y != bits(plan.scale) ||
                    call[2] != bits(position.x) || call[3] != bits(position.y)) {
                    fprintf(stderr, "Native floating draw differs at %zu/%zu\n", sample,
                            event);
                    return false;
                }
                seen |= flag;
                texts += !sprite;
                sprites += sprite;
            } else {
                CHECK(call[0] == 1 || call[0] == 5);
            }
        }
        CHECK(seen == input[7]);
    }
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB floating score draw reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    printf("1024 native floating draw cases match: %zu texts, %zu sprites.\n", texts,
           sprites);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2)
        return 2;
    bool okay = edges();
    if (okay && argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        okay = file && reference(file);
        if (file && fclose(file) != 0)
            okay = false;
    }
    return okay ? 0 : 1;
}
