#include "ab/camera_mover.h"
#include "camera_smoothstep_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Camera smoothstep check failed at %d: %s\n", __LINE__,    \
                    #value);                                                           \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (actual[index] != expected) {
            fprintf(stderr,
                    "Smoothstep sample %u word %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool reference(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_CAMERA_SMOOTHSTEP_V1 4096\n"));
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[8], output[2];
        camera_smoothstep_case(sample, input);
        CHECK(words(file, input, 8, sample));
        float smooth = 0, ease = 0;
        CHECK(ab_camera_smoothstep(number(input[1]), number(input[2]), number(input[3]),
                                   &smooth));
        CHECK(ab_camera_tween(AB_CAMERA_TWEEN_SMOOTHSTEP, number(input[4]),
                              number(input[5]), number(input[6]), number(input[7]),
                              &ease));
        output[0] = camera_smooth_word(smooth);
        output[1] = camera_smooth_word(ease);
        CHECK(words(file, output, 2, sample));
    }
    CHECK(fscanf(file, " %79[^\n]", marker) == 1 &&
          !strcmp(marker, "AB camera smoothstep reference complete"));
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    puts("Matched 40960 smoothstep words, 4096 original helper compositions");
    return true;
}

static bool controls(void) {
    float result = 17;
    CHECK(!ab_camera_smoothstep(0, 1, 0.5f, NULL));
    CHECK(ab_camera_smoothstep(0, 1, 0.5f, &result) && result == 0.5f);
    CHECK(ab_camera_smoothstep(0, 1, NAN, &result) && result == 1);
    CHECK(ab_camera_smoothstep(1, 1, 1, &result) && result == 1);
    CHECK(ab_camera_smoothstep(0, 1, -0.0f, &result) &&
          camera_smooth_word(result) == 0);
    CHECK(ab_camera_tween(AB_CAMERA_TWEEN_SMOOTHSTEP, 0, 10, 8, 0, &result) &&
          result == 18);
    CHECK(ab_camera_tween(AB_CAMERA_TWEEN_SMOOTHSTEP, 0.25f, 10, 8, 1, &result) &&
          result == 11.25f);
    CHECK(!ab_camera_tween(AB_CAMERA_TWEEN_SMOOTHSTEP, 0.5f, INFINITY, 8, 1, &result) &&
          result == 11.25f);
    AbCameraMover mover;
    CHECK(ab_camera_mover_init(&mover) &&
          ab_camera_mover_tweens(&mover, AB_CAMERA_TWEEN_SMOOTHSTEP,
                                 AB_CAMERA_TWEEN_SMOOTHSTEP) &&
          ab_camera_mover_position(&mover, (AbVec2){8, 16}, 1) &&
          ab_camera_mover_zoom(&mover, 3, 1) && ab_camera_mover_update(&mover, 0.5f));
    CHECK(mover.current_position.x == 4 && mover.current_position.y == 8 &&
          mover.current_zoom == 2);
    CHECK(ab_camera_mover_update(&mover, 1) && mover.current_position.x == 8 &&
          mover.current_position.y == 16 && mover.current_zoom == 3);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return controls() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = reference(file);
    if (fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
