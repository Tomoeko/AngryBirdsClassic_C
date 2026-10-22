#include "ab/theme_layer.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Theme layer transform failed at %d\n", __LINE__);         \
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

static bool edges(void) {
    AbThemeLayerTransform transform = {{{0, NAN}, {1, 0}}, 480, 1};
    float output;
    CHECK(ab_theme_screen_to_layer_x(&transform, 0, &output) && output == 0 &&
          ab_theme_layer_to_screen_x(&transform, 0, &output) && output == 0);
    transform.viewport_width = 0;
    output = 91;
    CHECK(!ab_theme_screen_to_layer_x(&transform, 0, &output) && output == 91);
    transform.viewport_width = 480;
    transform.camera.top_left.x = 0x1p100f;
    CHECK(ab_theme_screen_to_layer_x(&transform, 0, &output) &&
          output == transform.camera.top_left.x);
    output = 91;
    CHECK(!ab_theme_layer_to_screen_x(&transform, 0, &output) && output == 91);
    transform.camera.top_left.x = 0;
    transform.camera.zoom.x = 0;
    CHECK(!ab_theme_screen_to_layer_x(&transform, 0, &output) && output == 91);
    transform.camera.zoom.x = 1;
    CHECK(!ab_theme_screen_to_layer_x(&transform, NAN, &output) && output == 91);
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
          strcmp(header, "AB_ANDROID_THEME_LAYER_TRANSFORMS_V1 8192\n") == 0);
    size_t accepted = 0, nonfinite = 0;
    for (size_t item = 0; item < 8192; ++item) {
        uint32_t input[8], expected[4];
        CHECK(words(file, input, 8) && words(file, expected, 4) &&
              input[0] <= INT32_MAX && input[7] < 2 && expected[1] == 1000 &&
              expected[2] == 1 && expected[3] <= 1);
        AbThemeLayerTransform transform = {{{number(input[2]), number(input[3])},
                                            {number(input[4]), number(input[5])}},
                                           (int32_t)input[0],
                                           number(input[6])};
        float output = 91;
        bool okay =
            input[7]
                ? ab_theme_layer_to_screen_x(&transform, number(input[1]), &output)
                : ab_theme_screen_to_layer_x(&transform, number(input[1]), &output);
        if (okay != (expected[3] != 0) || (okay && bits(output) != expected[0]) ||
            (!okay && output != 91)) {
            fprintf(stderr,
                    "Theme transform %zu direction %u: %u/%08" PRIx32
                    " expected %u/%08" PRIx32 "\n",
                    item, input[7], okay, bits(output), expected[3], expected[0]);
            return false;
        }
        accepted += okay;
        nonfinite += !okay;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB theme layer transforms reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 8192 original theme transforms match: %zu finite results and %zu "
           "unsupported nonfinite inverse results.\n",
           accepted, nonfinite);
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
