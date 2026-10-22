#include "ab/camera_zoom_fit.h"
#include "camera_zoom_fit_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Camera zoom fit check failed at %d: %s\n", __LINE__,      \
                    #value);                                                           \
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

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (actual[index] != expected) {
            fprintf(stderr,
                    "Zoom-fit sample %u word %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool reference(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_CAMERA_ZOOM_FIT_V1 4096\n"));
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[8];
        camera_zoom_fit_case(sample, input);
        CHECK(words(file, input, 8, sample));
        float zoom;
        CHECK(ab_camera_zoom_fit(number(input[2]), number(input[4]), number(input[5]),
                                 number(input[6]), number(input[7]), &zoom));
        uint32_t output[] = {1, 3, 801, input[3], input[4], bits(zoom)};
        CHECK(words(file, output, 6, sample));
    }
    CHECK(fscanf(file, " %79[^\n]", marker) == 1 &&
          !strcmp(marker, "AB camera zoom fit reference complete"));
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    puts("Matched 57344 zoom-fit words, 4096 original projected numeric calls");
    return true;
}

static bool controls(void) {
    float output = 17;
    CHECK(ab_camera_zoom_fit(0, 0, 2, 320, 100, &output) && output == 2);
    CHECK(ab_camera_zoom_fit(0, -50, 2, 320, 100, &output) && output == 3.2f);
    CHECK(ab_camera_zoom_fit(0, -50, 2, 320, 25, &output) && output == 320.0f / 75.0f);
    CHECK(!ab_camera_zoom_fit(0, INFINITY, 2, 320, 25, &output));
    CHECK(!ab_camera_zoom_fit(0, 0, 2, 320, 25, NULL));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1)
        return controls() ? 0 : 1;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = reference(file);
    if (fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
