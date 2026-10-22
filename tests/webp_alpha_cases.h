#ifndef AB_TEST_WEBP_ALPHA_CASES_H
#define AB_TEST_WEBP_ALPHA_CASES_H

#include <stddef.h>
#include <stdint.h>

static uint32_t webp_alpha_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state >> 16;
}

static size_t webp_alpha_case(size_t sample, uint32_t input[6], uint8_t pixels[640]) {
    uint32_t state = UINT32_C(0x520a1f14) ^ (uint32_t)sample;
    input[0] = 1 + webp_alpha_random(&state) % 32;
    input[1] = 1 + webp_alpha_random(&state) % 16;
    input[2] = input[0] + webp_alpha_random(&state) % 8;
    input[3] = (uint32_t)sample % 4;
    input[4] = webp_alpha_random(&state) % (input[1] + 1);
    input[5] = UINT32_C(2166136261);
    size_t size = (size_t)input[2] * input[1];
    for (size_t index = 0; index < 640; ++index) {
        pixels[index] = (uint8_t)webp_alpha_random(&state);
        if (index < size) {
            input[5] = (input[5] ^ pixels[index]) * UINT32_C(16777619);
        }
    }
    return size;
}

#endif
