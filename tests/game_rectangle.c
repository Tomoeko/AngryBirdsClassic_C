#include "ab/game_rectangle.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Game rectangle check failed at %d\n", __LINE__);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static bool edges(void) {
    AbGameRectangle rectangle = {.color = {1, 0, 0.5f, 1},
                                 .left = -1.5f,
                                 .top = 2.5f,
                                 .right = 8.25f,
                                 .bottom = -5.25f};
    AbGameRectanglePlan plan;
    AbError error = {0};
    CHECK(ab_game_rectangle_plan(&rectangle, &plan, &error) &&
          plan.argb == UINT32_C(0xffff007f) && plan.left == -1 && plan.top == 2 &&
          plan.width == 9 && plan.height == -7 && plan.reset_state);
    uint8_t retained[sizeof(plan)];
    memcpy(retained, &plan, sizeof(plan));
    rectangle.color[0] = NAN;
    CHECK(!ab_game_rectangle_plan(&rectangle, &plan, &error) &&
          memcmp(retained, &plan, sizeof(plan)) == 0);
    rectangle.color[0] = 1;
    rectangle.right = 0x1p32f;
    CHECK(!ab_game_rectangle_plan(&rectangle, &plan, &error) &&
          memcmp(retained, &plan, sizeof(plan)) == 0);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_GAME_RECTANGLE_V1 8192\n") == 0);
    for (size_t item = 0; item < 8192; ++item) {
        uint32_t input[9], expected[8];
        CHECK(words(file, input, 9) && words(file, expected, 8) && input[8] <= 1);
        AbGameRectangle rectangle = {.color = {number(input[0]), number(input[1]),
                                               number(input[2]), number(input[3])},
                                     .left = number(input[4]),
                                     .top = number(input[5]),
                                     .right = number(input[6]),
                                     .bottom = number(input[7]),
                                     .respect_state = input[8] != 0};
        AbGameRectanglePlan plan;
        AbError error = {0};
        CHECK(ab_game_rectangle_plan(&rectangle, &plan, &error));
        uint32_t actual[] = {plan.argb,
                             (uint32_t)plan.left,
                             (uint32_t)plan.top,
                             (uint32_t)plan.width,
                             (uint32_t)plan.height,
                             1,
                             plan.reset_state,
                             plan.reset_state};
        for (size_t index = 0; index < 8; ++index) {
            if (actual[index] != expected[index]) {
                fprintf(stderr,
                        "Game rectangle %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        item, index, actual[index], expected[index]);
                return false;
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB game rectangle reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 8192 original rectangle fill arguments match, including color packing, "
         "signed endpoint-difference truncation and reset-before-fill selection.");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edges() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare(file);
    return fclose(file) == 0 && okay ? 0 : 1;
}
