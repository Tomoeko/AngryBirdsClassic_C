#ifndef AB_TEST_VP8_BOOLEAN_CASES_H
#define AB_TEST_VP8_BOOLEAN_CASES_H

#include <stddef.h>
#include <stdint.h>

static uint32_t vp8_boolean_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state >> 16;
}

static void vp8_boolean_case(size_t sample, uint32_t input[34], uint8_t bytes[512]) {
    uint32_t state = UINT32_C(0x520b0018) ^ (uint32_t)sample;
    input[0] = 128 + vp8_boolean_random(&state) % 385;
    for (size_t index = 0; index < 512; ++index) {
        bytes[index] = (uint8_t)vp8_boolean_random(&state);
    }
    /* The initial code must be inside the initial arithmetic interval. */
    if (bytes[0] == 255) {
        bytes[0] = 254;
    }
    input[1] = UINT32_C(2166136261);
    for (size_t index = 0; index < input[0]; ++index) {
        input[1] = (input[1] ^ bytes[index]) * UINT32_C(16777619);
    }
    for (size_t index = 2; index < 34; ++index) {
        uint32_t count = vp8_boolean_random(&state) % 25;
        uint32_t sign = vp8_boolean_random(&state) & 1u;
        input[index] = count | (sign << 5);
    }
}

#endif
