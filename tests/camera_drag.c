#include "ab/camera_drag.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Camera drag failed at %d: %s\n", __LINE__, #condition);   \
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

static void snapshot(const AbCameraDrag *state, uint32_t output[21]) {
    memset(output, 0, 21 * sizeof(*output));
    if (!state->initialized) {
        return;
    }
    output[0] = state->dragging ? 255u : 240u;
    const float values[] = {state->start.x,          state->start.y,
                            state->anchor.x,         state->anchor.y,
                            state->current_screen.x, state->current_screen.y,
                            state->old_screen.x,     state->old_screen.y};
    for (size_t index = 0; index < 8; ++index) {
        if (output[0] & (1u << index)) {
            output[1 + index] = bits(values[index]);
        }
    }
    output[9] = (uint32_t)state->count;
    for (size_t index = 0; index < state->count && index < 5; ++index) {
        output[10 + index * 2] = bits(state->velocity[index].x);
        output[11 + index * 2] = bits(state->velocity[index].y);
    }
    output[20] = bits(5);
}

static bool edge_checks(void) {
    AbCameraDrag state;
    AbVec2 target = {99, 99};
    CHECK(ab_camera_drag_init(&state) && !ab_camera_drag_is_dragging(&state) &&
          !ab_camera_drag_sample(&state, (AbVec2){0}) &&
          !ab_camera_drag_velocity(&state, (AbVec2){480, 320}, &target) &&
          target.x == 99 && !ab_camera_drag_is_dragging(NULL));
    CHECK(ab_camera_drag_start(&state, (AbVec2){10, 20}, (AbVec2){100, 200}) &&
          ab_camera_drag_is_dragging(&state));
    AbCoordinateCamera camera = {{0, 0}, {2, 4}};
    CHECK(ab_camera_drag_update(&state, 0.5f, (AbVec2){11, 23}, &camera, &target) &&
          target.x == 99.5f && target.y == 199.25f && state.count == 1 &&
          state.old_screen.x == 10 && state.current_screen.y == 23 &&
          state.velocity[0].x == 2 && state.velocity[0].y == 6);
    for (size_t index = 0; index < 6; ++index) {
        CHECK(ab_camera_drag_sample(&state, (AbVec2){(float)index, -(float)index}));
    }
    CHECK(state.count == 5 && state.velocity[0].x == 1 && state.velocity[4].x == 5 &&
          ab_camera_drag_velocity(&state, (AbVec2){1, 1}, &target) && target.x == -3 &&
          target.y == 3);
    AbCameraDragRelease released;
    CHECK(ab_camera_drag_release(&state, (AbVec2){1, 1}, &released) &&
          !state.dragging && state.count == 5 && released.screen_position.x == 11 &&
          released.screen_position.y == 23 && released.normalized_velocity.x == -3);
    CHECK(ab_camera_drag_start(&state, (AbVec2){0}, (AbVec2){0}) &&
          ab_camera_drag_release(&state, (AbVec2){480, 320}, &released) &&
          bits(released.normalized_velocity.x) == UINT32_C(0x7fc00000) &&
          bits(released.normalized_velocity.y) == UINT32_C(0x7fc00000));
    CHECK(ab_camera_drag_start(&state, (AbVec2){0}, (AbVec2){0}));
    uint32_t saved[21], changed[21];
    snapshot(&state, saved);
    CHECK(!ab_camera_drag_update(&state, NAN, (AbVec2){1, 2}, &camera, &target) &&
          !ab_camera_drag_update(&state, 1, (AbVec2){NAN, 2}, &camera, &target));
    snapshot(&state, changed);
    CHECK(memcmp(saved, changed, sizeof(saved)) == 0 &&
          !ab_camera_drag_velocity(&state, (AbVec2){0, 1}, &target) &&
          !ab_camera_drag_start(&state, (AbVec2){0}, (AbVec2){0, NAN}) &&
          !ab_camera_drag_init(NULL) &&
          !ab_camera_drag_start(NULL, (AbVec2){0}, (AbVec2){0}) &&
          !ab_camera_drag_release(&state, (AbVec2){1, 1}, NULL));
    CHECK(ab_camera_drag_update(&state, 0, (AbVec2){0, 1}, &camera, &target) &&
          bits(state.velocity[0].x) == UINT32_C(0x7fc00000) &&
          bits(state.velocity[0].y) == UINT32_C(0x7f800000) &&
          ab_camera_drag_velocity(&state, (AbVec2){1, 1}, &target) &&
          bits(target.x) == UINT32_C(0xffc00000) &&
          bits(target.y) == UINT32_C(0xff800000));
    puts("Camera drag FIFO, screen/world deltas, normalized release, empty-buffer "
         "NaNs and invalid-input preservation pass.");
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
          (strcmp(header, "AB_ANDROID_CAMERA_DRAG_V1 128 64\n") == 0 ||
           strcmp(header, "AB_ANDROID_CAMERA_DRAG_V2 128 64\n") == 0));
    for (size_t item = 0; item < 128; ++item) {
        uint32_t setup[2];
        CHECK(words(file, setup, 2));
        AbVec2 pixel_size = {number(setup[0]), number(setup[1])};
        AbCameraDrag state;
        CHECK(ab_camera_drag_init(&state));
        for (size_t step = 0; step < 64; ++step) {
            uint32_t input[10], expected[28], actual[28] = {0};
            CHECK(words(file, input, 10) && words(file, expected, 28) && input[0] <= 5);
            AbVec2 touch = {number(input[2]), number(input[3])};
            AbVec2 anchor = {number(input[4]), number(input[5])};
            AbCoordinateCamera camera = {{number(input[6]), number(input[7])},
                                         {number(input[8]), number(input[9])}};
            AbVec2 target = {0};
            if (input[0] == 0) {
                CHECK(ab_camera_drag_start(&state, touch, anchor));
            } else if (input[0] == 1) {
                CHECK(ab_camera_drag_update(&state, number(input[1]), touch, &camera,
                                            &target));
                actual[21] = 1;
                actual[22] = bits(target.x);
                actual[23] = bits(target.y);
                actual[27] = 1;
            } else if (input[0] == 2) {
                AbCameraDragRelease result;
                CHECK(ab_camera_drag_release(&state, pixel_size, &result));
                actual[21] = 3;
                actual[22] = bits(result.screen_position.x);
                actual[23] = bits(result.screen_position.y);
                actual[24] = bits(result.normalized_velocity.x);
                actual[25] = bits(result.normalized_velocity.y);
            } else if (input[0] == 3) {
                CHECK(ab_camera_drag_sample(&state, touch));
            } else if (input[0] == 4) {
                CHECK(ab_camera_drag_velocity(&state, pixel_size, &target));
                actual[21] = 2;
                actual[24] = bits(target.x);
                actual[25] = bits(target.y);
            } else {
                actual[21] = 4;
                actual[26] = ab_camera_drag_is_dragging(&state);
            }
            snapshot(&state, actual);
            for (size_t field = 0; field < 28; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "Drag case %zu step %zu field %zu: %08" PRIx32
                            " != %08" PRIx32 "\n",
                            item, step, field, actual[field], expected[field]);
                    return false;
                }
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB camera drag reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 8192 original camera drag commands match: field presence, five-sample "
         "FIFO, coordinate deltas, release/average results and conversion count.");
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
