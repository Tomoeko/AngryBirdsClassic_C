#include "ab/camera_mover.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Camera mover failed at %d: %s\n", __LINE__, #condition);  \
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
    AbCameraMover mover;
    CHECK(ab_camera_mover_init(&mover) && mover.current_zoom == 1 &&
          mover.target_zoom == 1 && mover.target_position_present &&
          !mover.start_position_present && !mover.start_zoom_scale_present &&
          !mover.start_zoom_present);
    CHECK(!ab_camera_mover_init(NULL) && !ab_camera_mover_reset(NULL) &&
          !ab_camera_mover_clear_targets(NULL) &&
          !ab_camera_mover_position(NULL, (AbVec2){0, 0}, 0) &&
          !ab_camera_mover_zoom(NULL, 1, 0) && !ab_camera_mover_update(NULL, 0));
    CHECK(ab_camera_mover_update(&mover, NAN));
    CHECK(ab_camera_mover_position(&mover, (AbVec2){8, -8}, 1) &&
          ab_camera_mover_zoom(&mover, 5, 1) && ab_camera_mover_update(&mover, 0.5f) &&
          mover.current_position.x == 4 && mover.current_position.y == -4 &&
          mover.current_zoom == 3);
    CHECK(ab_camera_mover_update(&mover, 0.75f) && mover.current_position.x == 8 &&
          mover.current_zoom == 5 && mover.movement_timer == 1 &&
          mover.zoom_timer == 1 && mover.movement_duration == 1 &&
          mover.zoom_duration == 1);
    CHECK(ab_camera_mover_update(&mover, -0.25f) && mover.current_position.x == 7.5f &&
          mover.current_zoom == 4.75f);
    CHECK(ab_camera_mover_zoom(&mover, 7, -1) && mover.current_zoom == 7 &&
          mover.start_zoom == 7 && mover.start_zoom_present &&
          mover.start_zoom_scale == 1 && mover.start_zoom_scale_present &&
          bits(mover.zoom_duration) == 0);
    CHECK(ab_camera_mover_clear_targets(&mover) &&
          ab_camera_mover_update(&mover, INFINITY) && mover.movement_timer == 0.75f);
    CHECK(ab_camera_mover_reset(&mover) && mover.current_zoom == 1 &&
          mover.start_zoom == 7 && mover.start_zoom_scale == 1 &&
          mover.start_position_present);
    CHECK(ab_camera_mover_position(&mover, (AbVec2){-0.0f, 3}, -0.0f) &&
          bits(mover.current_position.x) == UINT32_C(0x80000000) &&
          bits(mover.start_position.x) == UINT32_C(0x80000000) &&
          bits(mover.movement_duration) == 0);
    uint32_t saved[16], current[16];
    snapshot(&mover, saved);
    CHECK(!ab_camera_mover_position(&mover, (AbVec2){NAN, 0}, 1) &&
          !ab_camera_mover_zoom(&mover, 1, INFINITY));
    snapshot(&mover, current);
    CHECK(memcmp(saved, current, sizeof(saved)) == 0);
    CHECK(ab_camera_mover_position(&mover, (AbVec2){8, -8}, 1));
    snapshot(&mover, saved);
    CHECK(!ab_camera_mover_update(&mover, NAN));
    snapshot(&mover, current);
    CHECK(memcmp(saved, current, sizeof(saved)) == 0);
    float result = 99;
    CHECK(ab_camera_tween_cubic(0, 10, 10, 1, &result) && result == 10);
    CHECK(ab_camera_tween_cubic(0.5f, 10, 10, 1, &result) && result == 15);
    CHECK(ab_camera_tween_cubic(1, 10, 10, 1, &result) && result == 20);
    CHECK(ab_camera_tween_cubic(2, 10, 10, 1, &result) && result == 60);
    CHECK(ab_camera_tween_cubic(-0.5f, 10, 10, 1, &result) && result == 5);
    CHECK(!ab_camera_tween_cubic(1, 10, 10, 0, &result) && result == 5 &&
          !ab_camera_tween_cubic(1, 10, 10, 0x1p-149f, &result) && result == 5 &&
          !ab_camera_tween_cubic(FLT_MAX, 10, 10, 1, &result) && result == 5 &&
          !ab_camera_tween_cubic(1, 10, INFINITY, 1, &result) && result == 5 &&
          !ab_camera_tween_cubic(0, 0, 0, 1, NULL));
    puts("Camera reset, target lifetime, tween bounds, timers and signed zero pass.");
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
          strcmp(header, "AB_ANDROID_CAMERA_MOVER_V1 128 64\n") == 0);
    size_t operations[6] = {0};
    for (size_t item = 0; item < 128; ++item) {
        AbCameraMover mover;
        CHECK(ab_camera_mover_init(&mover));
        for (size_t step = 0; step < 64; ++step) {
            uint32_t input[6], expected[17], actual[17] = {0};
            CHECK(words(file, input, 6) && words(file, expected, 17) && input[0] < 6);
            ++operations[input[0]];
            bool okay;
            switch (input[0]) {
                case 0:
                    okay = ab_camera_mover_reset(&mover);
                    break;
                case 1:
                    okay = ab_camera_mover_position(
                        &mover, (AbVec2){number(input[1]), number(input[2])},
                        number(input[4]));
                    break;
                case 2:
                    okay = ab_camera_mover_zoom(&mover, number(input[3]),
                                                number(input[4]));
                    break;
                case 3:
                    okay = ab_camera_mover_clear_targets(&mover);
                    break;
                case 4:
                    okay = ab_camera_mover_update(&mover, number(input[5]));
                    break;
                default: {
                    float value;
                    okay = ab_camera_tween_cubic(number(input[1]), number(input[2]),
                                                 number(input[3]), number(input[4]),
                                                 &value);
                    if (okay) {
                        actual[16] = bits(value);
                    }
                    break;
                }
            }
            CHECK(okay);
            snapshot(&mover, actual);
            for (size_t field = 0; field < 17; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "Native mover case %zu step %zu op %u field %zu: "
                            "%08" PRIx32 " != %08" PRIx32 "\n",
                            item, step, input[0], field, actual[field],
                            expected[field]);
                    return false;
                }
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB camera mover reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 8192 native camera commands/state snapshots match: %zu resets, "
           "%zu position, %zu zoom, %zu clears, %zu updates and %zu cubic calls.\n",
           operations[0], operations[1], operations[2], operations[3], operations[4],
           operations[5]);
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
