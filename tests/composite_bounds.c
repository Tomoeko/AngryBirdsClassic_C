#include "ab/composite_bounds.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Composite bounds check failed at %d\n", __LINE__);        \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static bool edges(void) {
    AbCompositeBounds output;
    AbError error = {0};
    CHECK(ab_composite_bounds(NULL, 0, &output, &error) && output.width == 1 &&
          output.height == 1 && output.pivot_x == INT32_MIN + 1 &&
          output.pivot_y == INT32_MIN + 1);
    AbSpriteRegion region = {.width = 10, .height = 20};
    AbCompositeBoundsPart part = {.region = &region,
                                  .offset = {-0.5f, 0.5f},
                                  .scale = {1, 1},
                                  .factors = {1, 1},
                                  .enabled = true};
    CHECK(ab_composite_bounds(&part, 1, &output, &error) && output.width == 9 &&
          output.height == 20 && output.pivot_x == 0 && output.pivot_y == 0);
    AbCompositeBounds retained;
    memcpy(&retained, &output, sizeof(output));
    part.offset.x = 0x1p32f;
    CHECK(!ab_composite_bounds(&part, 1, &output, &error) &&
          memcmp(&retained, &output, sizeof(output)) == 0);
    part.enabled = false;
    part.region = NULL;
    part.offset.x = NAN;
    CHECK(ab_composite_bounds(&part, 1, &output, &error) && output.width == 1);
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
          strcmp(header, "AB_ANDROID_COMPOSITE_BOUNDS_V1 4096\n") == 0);
    size_t entries = 0, enabled = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t count, expected[4];
        CHECK(words(file, &count, 1) && count <= 8);
        AbSpriteRegion regions[8];
        AbCompositeBoundsPart parts[8];
        for (size_t index = 0; index < count; ++index) {
            uint32_t input[12];
            CHECK(words(file, input, 12) && input[11] <= 1);
            regions[index] = (AbSpriteRegion){.width = (int16_t)input[0],
                                              .height = (int16_t)input[1],
                                              .pivot_x = (int16_t)(int32_t)input[2],
                                              .pivot_y = (int16_t)(int32_t)input[3]};
            parts[index] =
                (AbCompositeBoundsPart){.region = regions + index,
                                        .offset = {number(input[4]), number(input[5])},
                                        .scale = {number(input[6]), number(input[7])},
                                        .factors = {number(input[8]), number(input[9])},
                                        .rotation = number(input[10]),
                                        .enabled = input[11] != 0};
            enabled += parts[index].enabled;
        }
        CHECK(words(file, expected, 4));
        AbCompositeBounds output;
        AbError error = {0};
        CHECK(ab_composite_bounds(parts, count, &output, &error));
        uint32_t actual[] = {(uint32_t)output.width, (uint32_t)output.height,
                             (uint32_t)output.pivot_x, (uint32_t)output.pivot_y};
        for (size_t index = 0; index < 4; ++index) {
            if (actual[index] != expected[index]) {
                fprintf(stderr,
                        "Composite bounds %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        item, index, actual[index], expected[index]);
                return false;
            }
        }
        entries += count;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB composite bounds reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native composite bounds match: %zu parts, %zu enabled.\n", entries,
           enabled);
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
