#include "ab/camera_shake.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Camera shake failed at %d: %s\n", __LINE__, #condition);  \
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

static void snapshot(AbCameraShake state, AbRandom48 random, uint32_t output[8]) {
    output[0] = state.present;
    output[1] = state.present & AB_CAMERA_SHAKE_DURATION ? bits(state.duration) : 0;
    output[2] = state.present & AB_CAMERA_SHAKE_AMOUNT ? bits(state.amount) : 0;
    output[3] = state.present & AB_CAMERA_SHAKE_AMPLITUDE ? bits(state.amplitude) : 0;
    output[4] = bits(state.offset.x);
    output[5] = bits(state.offset.y);
    output[6] = (uint32_t)random.value;
    output[7] = (uint32_t)(random.value >> 32);
}

static bool edge_checks(void) {
    AbCameraShake state;
    AbRandom48 random, expected;
    CHECK(ab_camera_shake_init(&state) && state.present == AB_CAMERA_SHAKE_AMPLITUDE &&
          ab_camera_shake_update(&state, NAN, NULL) && bits(state.offset.x) == 0 &&
          bits(state.offset.y) == 0);
    CHECK(ab_random48_seed(&random, 520));
    expected = random;
    uint32_t draw;
    CHECK(ab_random48_next(&expected, &draw) && ab_random48_next(&expected, &draw));
    CHECK(ab_camera_shake_add(&state, 0.125f, 0.25f) &&
          ab_camera_shake_update(&state, 0.125f, &random) && state.duration == 0 &&
          state.amplitude == 0.25f && random.value == expected.value &&
          state.offset.x == 0 && state.offset.y == 0);
    CHECK(ab_camera_shake_update(&state, NAN, NULL) &&
          !(state.present & AB_CAMERA_SHAKE_DURATION) && state.amplitude == 0 &&
          state.amount == 0.25f);
    state.amplitude = 4;
    CHECK(ab_camera_shake_add(&state, 0.125f, 8) &&
          ab_camera_shake_update(&state, 0.25f, &random) && state.duration == -0.125f &&
          state.amplitude == 4);
    AbVec2 offset = state.offset;
    CHECK(ab_camera_shake_remove(&state) && state.duration == 0 && state.amount == 0 &&
          state.amplitude == 4 && state.offset.x == offset.x &&
          state.offset.y == offset.y);
    CHECK(ab_camera_shake_update(&state, NAN, NULL) && state.amplitude == 0);
    state = (AbCameraShake){.duration = 1, .present = AB_CAMERA_SHAKE_DURATION};
    CHECK(ab_camera_shake_update(&state, 0.5f, NULL) && state.duration == 0.5f &&
          !(state.present & AB_CAMERA_SHAKE_AMPLITUDE));
    state = (AbCameraShake){.amplitude = -0.5f, .present = AB_CAMERA_SHAKE_AMPLITUDE};
    expected = random;
    CHECK(ab_random48_next(&expected, &draw) && ab_random48_next(&expected, &draw) &&
          ab_camera_shake_update(&state, NAN, &random) &&
          random.value == expected.value && state.offset.x == 0 && state.offset.y == 0);
    uint32_t saved[8], changed[8];
    snapshot(state, random, saved);
    CHECK(!ab_camera_shake_update(&state, 0, NULL));
    snapshot(state, random, changed);
    CHECK(memcmp(saved, changed, sizeof(saved)) == 0);
    state.amplitude = 0x1p30f;
    snapshot(state, random, saved);
    CHECK(!ab_camera_shake_update(&state, 0, &random));
    snapshot(state, random, changed);
    CHECK(memcmp(saved, changed, sizeof(saved)) == 0);
    CHECK(!ab_camera_shake_add(&state, INFINITY, 1) &&
          !ab_camera_shake_add(&state, 1, NAN) && !ab_camera_shake_init(NULL) &&
          !ab_camera_shake_add(NULL, 0, 0) && !ab_camera_shake_remove(NULL) &&
          !ab_camera_shake_update(NULL, 0, &random));
    puts("Camera shake expiry, replacement, signed/partial state, fractional "
         "two-draw behavior and invalid-input preservation pass.");
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
          strcmp(header, "AB_ANDROID_CAMERA_SHAKE_V1 128 64\n") == 0);
    for (size_t item = 0; item < 128; ++item) {
        uint32_t seed;
        CHECK(words(file, &seed, 1));
        AbRandom48 random;
        AbCameraShake state;
        CHECK(ab_random48_seed(&random, seed) && ab_camera_shake_init(&state));
        for (size_t step = 0; step < 64; ++step) {
            uint32_t input[3], expected[8], actual[8];
            CHECK(words(file, input, 3) && words(file, expected, 8) && input[0] <= 3);
            bool okay;
            if (input[0] == 0) {
                okay = ab_camera_shake_add(&state, number(input[1]), number(input[2]));
            } else if (input[0] == 1) {
                okay = ab_camera_shake_remove(&state);
            } else if (input[0] == 2) {
                okay = ab_camera_shake_update(&state, number(input[1]), &random);
            } else {
                state.amplitude = number(input[1]);
                state.present = (state.present & ~(uint32_t)AB_CAMERA_SHAKE_AMPLITUDE) |
                                (input[2] ? AB_CAMERA_SHAKE_AMPLITUDE : 0);
                okay = true;
            }
            CHECK(okay);
            snapshot(state, random, actual);
            for (size_t field = 0; field < 8; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "Shake case %zu step %zu field %zu: %08" PRIx32
                            " != %08" PRIx32 "\n",
                            item, step, field, actual[field], expected[field]);
                    return false;
                }
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB camera shake reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 8192 original camera shake commands match, including numeric/presence "
         "state, X/Y output and shared 48-bit random state.");
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
