#ifndef AB_TEST_WEBP_BITS_CASES_H
#define AB_TEST_WEBP_BITS_CASES_H

#include <stddef.h>
#include <stdint.h>

static uint32_t webp_bits_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state >> 16;
}

static void webp_bits_case(size_t sample, uint32_t input[34], uint8_t bytes[512]) {
    uint32_t state = UINT32_C(0x520b1750) ^ (uint32_t)sample;
    input[0] = 256 + webp_bits_random(&state) % 257;
    input[1] = UINT32_C(2166136261);
    for (size_t index = 0; index < 512; ++index) {
        bytes[index] = (uint8_t)webp_bits_random(&state);
        if (index < input[0]) {
            input[1] = (input[1] ^ bytes[index]) * UINT32_C(16777619);
        }
    }
    for (size_t index = 2; index < 34; ++index) {
        input[index] = webp_bits_random(&state) % 25;
    }
}

#endif
