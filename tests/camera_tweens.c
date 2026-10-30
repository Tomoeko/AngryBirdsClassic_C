#include "ab/camera_mover.h"
#include "ab/camera_system.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Camera tweens failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float number(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static void snapshot(const AbCameraMover *mover, uint32_t output[16]) {
    output[0] = (mover->target_position_present ? 1u : 0u) |
                (mover->target_zoom_present ? 2u : 0u) |
                (mover->start_position_present ? 4u : 0u) |
                (mover->start_zoom_scale_present ? 8u : 0u) |
                (mover->start_zoom_present ? 16u : 0u);
    const float values[] = {
        mover->current_position.x,
        mover->current_position.y,
        mover->target_position_present ? mover->target_position.x : 0,
        mover->target_position_present ? mover->target_position.y : 0,
        mover->start_position_present ? mover->start_position.x : 0,
        mover->start_position_present ? mover->start_position.y : 0,
        mover->current_zoom,
        mover->target_zoom_present ? mover->target_zoom : 0,
        mover->zoom_velocity,
        mover->start_zoom_scale_present ? mover->start_zoom_scale : 0,
        mover->start_zoom_present ? mover->start_zoom : 0,
        mover->movement_duration,
        mover->movement_timer,
        mover->zoom_duration,
        mover->zoom_timer};
    for (size_t index = 0; index < 15; ++index) {
        output[index + 1] = bits(values[index]);
    }
}

static bool edge_checks(void) {
    float value = 99;
    CHECK(ab_camera_tween(AB_CAMERA_TWEEN_CUBIC_OUT, 0.5f, 10, 8, 1, &value) &&
          value == 17);
    CHECK(ab_camera_tween(AB_CAMERA_TWEEN_CUBIC_IN_OUT, 0.5f, 10, 8, 1, &value) &&
          value == 14);
    CHECK(ab_camera_tween(AB_CAMERA_TWEEN_END, NAN, 10, 8, NAN, &value) && value == 18);
    CHECK(ab_camera_tween(AB_CAMERA_TWEEN_COSINE_IN_OUT, 0.5f, 10, 8, 1, &value) &&
          value == 14);
    CHECK(ab_camera_tween(AB_CAMERA_TWEEN_COSINE_IN_OUT, 1, 10, 8, 1, &value) &&
          value == 18);
    CHECK(!ab_camera_tween(AB_CAMERA_TWEEN_CUBIC_OUT, 0, 0, 1, 0, &value) &&
          value == 18 && !ab_camera_tween((AbCameraTween)5, 0, 0, 1, 1, &value) &&
          value == 18);
    AbCameraMover mover;
    CHECK(ab_camera_mover_init(&mover) &&
          ab_camera_mover_tweens(&mover, AB_CAMERA_TWEEN_CUBIC_OUT,
                                 AB_CAMERA_TWEEN_END) &&
          ab_camera_mover_position(&mover, (AbVec2){8, 16}, 1) &&
          ab_camera_mover_zoom(&mover, 3, 1) && ab_camera_mover_update(&mover, 0.5f) &&
          mover.current_position.x == 7 && mover.current_position.y == 14 &&
          mover.current_zoom == 3 && mover.zoom_timer == 0.5f);
    CHECK(
        ab_camera_mover_reset(&mover) &&
        mover.movement_tween == AB_CAMERA_TWEEN_CUBIC_OUT &&
        mover.zoom_tween == AB_CAMERA_TWEEN_END &&
        !ab_camera_mover_tweens(&mover, (AbCameraTween)5, AB_CAMERA_TWEEN_CUBIC_OUT) &&
        mover.movement_tween == AB_CAMERA_TWEEN_CUBIC_OUT);
    AbCameraSnap snaps[] = {{{0, 0}, 1}, {{1000, 0}, 1}};
    AbCameraSystem *system = NULL;
    CHECK(ab_camera_system_open(snaps, 2, (AbVec2){480, 320}, 200, 200, SIZE_MAX,
                                &system, NULL));
    const char *unlock = "unlockCameraLimits";
    AbCameraSequenceStage stage = {.action = {(const uint8_t *)unlock, strlen(unlock)}};
    AbCameraSequence sequence = {&stage, 1};
    bool okay = ab_camera_system_start(system, &sequence, NULL, NULL, NULL);
    AbCameraSystemInput input = {
        .targets_present = true,
        .targets = {.values = {{8, 16}, 3, 7},
                    .movement_duration = 1,
                    .zoom_duration = 1,
                    .movement_tween = AB_CAMERA_TWEEN_CUBIC_OUT,
                    .zoom_tween = AB_CAMERA_TWEEN_END,
                    .movement_tween_present = true,
                    .zoom_tween_present = true}};
    okay = okay && ab_camera_system_update(system, 0.5f, &input, NULL);
    AbCameraSystemView view = ab_camera_system_view(system);
    okay = okay && view.mover->current_position.x == 7 &&
           view.mover->current_position.y == 14 && view.mover->current_zoom == 3;
    input.targets.movement_tween_present = input.targets.zoom_tween_present = false;
    okay = okay && ab_camera_system_update(system, 0, &input, NULL);
    view = ab_camera_system_view(system);
    okay = okay && view.mover->movement_tween == AB_CAMERA_TWEEN_CUBIC_IN_OUT &&
           view.mover->zoom_tween == AB_CAMERA_TWEEN_CUBIC_IN_OUT;
    ab_camera_system_close(system);
    CHECK(okay);
    puts("Builtin camera tween arithmetic, reset-preserved selection and resolved "
         "target fallback pass.");
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
          (strcmp(header, "AB_ANDROID_CAMERA_TWEENS_V1 128 64\n") == 0 ||
           strcmp(header, "AB_ANDROID_CAMERA_TWEENS_V2 128 64\n") == 0));
    size_t evaluations[4] = {0};
    for (size_t item = 0; item < 128; ++item) {
        AbCameraMover mover;
        CHECK(ab_camera_mover_init(&mover));
        for (size_t step = 0; step < 64; ++step) {
            uint32_t input[6], expected[20], actual[20] = {0};
            CHECK(words(file, input, 6) && words(file, expected, 20) && input[0] <= 6);
            switch (input[0]) {
                case 0:
                    CHECK(ab_camera_mover_reset(&mover));
                    break;
                case 1:
                    CHECK(ab_camera_mover_position(
                        &mover, (AbVec2){number(input[1]), number(input[2])},
                        number(input[4])));
                    break;
                case 2:
                    CHECK(ab_camera_mover_zoom(&mover, number(input[3]),
                                               number(input[4])));
                    break;
                case 3:
                    CHECK(ab_camera_mover_clear_targets(&mover));
                    break;
                case 4:
                    CHECK(ab_camera_mover_update(&mover, number(input[5])));
                    break;
                case 5: {
                    float result = 0.0f;
                    CHECK(input[5] < 4 &&
                          ab_camera_tween((AbCameraTween)input[5], number(input[1]),
                                          number(input[2]), number(input[3]),
                                          number(input[4]), &result));
                    actual[18] = 1;
                    actual[19] = bits(result);
                    ++evaluations[input[5]];
                    break;
                }
                case 6: {
                    CHECK(input[1] <= 4 && input[2] <= 4);
                    AbCameraTween movement = input[1] ? (AbCameraTween)(input[1] - 1)
                                                      : AB_CAMERA_TWEEN_CUBIC_IN_OUT;
                    AbCameraTween zoom =
                        input[2] ? (AbCameraTween)(input[2] - 1) : movement;
                    CHECK(ab_camera_mover_tweens(&mover, movement, zoom));
                    break;
                }
            }
            snapshot(&mover, actual);
            actual[16] = (uint32_t)mover.movement_tween;
            actual[17] = (uint32_t)mover.zoom_tween;
            for (size_t field = 0; field < 20; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "Tweens case %zu step %zu field %zu: %08" PRIx32
                            " != %08" PRIx32 "\n",
                            item, step, field, actual[field], expected[field]);
                    return false;
                }
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB camera tweens reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 8192 original mover/tween commands match; %zu cubic in/out, %zu cubic "
           "out, %zu end-function and %zu cosine evaluations match.\n",
           evaluations[0], evaluations[1], evaluations[2], evaluations[3]);
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
    bool okay = file && compare_native(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
