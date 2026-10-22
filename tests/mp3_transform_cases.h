#ifndef AB_TEST_MP3_TRANSFORM_CASES_H
#define AB_TEST_MP3_TRANSFORM_CASES_H

#include <stddef.h>
#include <stdint.h>

enum { AB_TEST_MP3_TRANSFORM_CASES = 1024 };

static uint32_t mp3_transform_next(uint32_t *state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static void mp3_transform_case(size_t sample, float frequency[18], float previous[18],
                               float window[36]) {
    uint32_t state = UINT32_C(0x36d520ab) ^ (uint32_t)sample;
    for (size_t index = 0; index < 18; ++index) {
        frequency[index] = sample == 1 ? -0.0f : 0.0f;
        previous[index] = sample == 1 ? -0.0f : 0.0f;
        if (sample >= 38) {
            int32_t value = (int32_t)(mp3_transform_next(&state) & 65535u) - 32768;
            frequency[index] = (float)value * (sample & 1u ? 32.0f : 0.0009765625f);
            value = (int32_t)(mp3_transform_next(&state) & 65535u) - 32768;
            previous[index] = (float)value * 0.0078125f;
        }
    }
    if (sample >= 2 && sample < 38) {
        frequency[(sample - 2) % 18] = sample < 20 ? 1.0f : -1.0f;
    }
    for (size_t index = 0; index < 36; ++index) {
        int32_t value = (int32_t)(mp3_transform_next(&state) & 1023u) - 512;
        window[index] = sample < 76 ? 1.0f : (float)value * 0.00390625f;
    }
}

#endif
