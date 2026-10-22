#ifndef AB_TEST_VP8_COEFFICIENT_CASES_H
#define AB_TEST_VP8_COEFFICIENT_CASES_H

#include <stddef.h>
#include <stdint.h>

static uint32_t vp8_coefficient_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state >> 16;
}

static uint32_t vp8_coefficient_hash(const uint8_t *bytes, size_t size) {
    uint32_t value = UINT32_C(2166136261);
    for (size_t index = 0; index < size; ++index) {
        value = (value ^ bytes[index]) * UINT32_C(16777619);
    }
    return value;
}

static void vp8_coefficient_case(size_t sample, uint32_t input[13], uint8_t bytes[8192],
                                 uint8_t probabilities[264]) {
    uint32_t state = UINT32_C(0x520c0eff) ^ (uint32_t)sample;
    input[0] = 8192;
    for (size_t index = 0; index < 8192; ++index) {
        uint32_t value = vp8_coefficient_random(&state);
        bytes[index] = sample % 16 == 0 ? 0 : sample % 16 == 1 ? 254 : (uint8_t)value;
    }
    if (bytes[0] == 255) {
        bytes[0] = 254;
    }
    for (size_t index = 0; index < 264; ++index) {
        uint8_t value = (uint8_t)vp8_coefficient_random(&state);
        switch (sample % 8) {
            case 0:
                value = 128;
                break;
            case 1:
                value = 1;
                break;
            case 2:
                value = 254;
                break;
            case 3:
                value = index % 2 ? 0 : 255;
                break;
            default:
                break;
        }
        probabilities[index] = value;
    }
    input[1] = vp8_coefficient_hash(bytes, 8192);
    input[2] = vp8_coefficient_hash(probabilities, 264);
    input[3] = vp8_coefficient_random(&state) % 441;
    input[4] = vp8_coefficient_random(&state) % 441;
    for (size_t index = 0; index < 8; ++index) {
        input[5 + index] = vp8_coefficient_random(&state) % 6;
    }
}

#endif
