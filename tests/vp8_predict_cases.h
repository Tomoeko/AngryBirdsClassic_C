#ifndef AB_TEST_VP8_PREDICT_CASES_H
#define AB_TEST_VP8_PREDICT_CASES_H

#include <stddef.h>
#include <stdint.h>

static uint32_t vp8_predict_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state >> 16;
}

static void vp8_predict_case(size_t sample, uint32_t input[3], uint8_t pixels[640]) {
    unsigned command = (unsigned)(sample % 24);
    input[0] = command < 7 ? 0 : command < 14 ? 1 : 2;
    input[1] = command < 7 ? command : command < 14 ? command - 7 : command - 14;
    uint32_t state = UINT32_C(0x5200d1c7) ^ (uint32_t)sample;
    uint32_t hash = UINT32_C(2166136261);
    for (size_t index = 0; index < 640; ++index) {
        uint8_t value = (uint8_t)vp8_predict_random(&state);
        if (sample % 7 == 0) {
            value = 0;
        } else if (sample % 7 == 1) {
            value = 255;
        } else if (sample % 7 == 2) {
            value = (uint8_t)index;
        }
        pixels[index] = value;
        hash = (hash ^ value) * UINT32_C(16777619);
    }
    input[2] = hash;
}

#endif
