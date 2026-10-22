#ifndef AB_TEST_VP8_TRANSFORM_CASES_H
#define AB_TEST_VP8_TRANSFORM_CASES_H

#include <stddef.h>
#include <stdint.h>

static uint32_t vp8_transform_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state >> 16;
}

static uint32_t vp8_transform_hash(const uint8_t *bytes, size_t size) {
    uint32_t hash = UINT32_C(2166136261);
    for (size_t index = 0; index < size; ++index) {
        hash = (hash ^ bytes[index]) * UINT32_C(16777619);
    }
    return hash;
}

static void vp8_transform_case(size_t sample, uint32_t input[3], int16_t values[32],
                               uint8_t pixels[528]) {
    uint32_t state = UINT32_C(0x520d0c70) ^ (uint32_t)sample;
    unsigned kind = (unsigned)(sample % 5);
    input[0] = kind;
    for (size_t index = 0; index < 32; ++index) {
        uint32_t random = vp8_transform_random(&state);
        int32_t value =
            kind == 0 ? (int32_t)random - 32768 : (int32_t)(random % 4097) - 2048;
        if (sample % 7 == 0 || (kind == 3 && index) ||
            (kind == 4 && index != 0 && index != 1 && index != 4)) {
            value = 0;
        }
        values[index] = (int16_t)value;
    }
    for (size_t index = 0; index < 528; ++index) {
        uint8_t value = (uint8_t)vp8_transform_random(&state);
        if (sample % 7 == 1) {
            value = 0;
        } else if (sample % 7 == 2) {
            value = 255;
        }
        pixels[index] = value;
    }
    if (kind == 0) {
        for (size_t index = 0; index < 264; ++index) {
            pixels[index * 2] = 0x39;
            pixels[index * 2 + 1] = 0x30;
        }
    }
    uint8_t coefficient_bytes[64];
    for (size_t index = 0; index < 32; ++index) {
        coefficient_bytes[index * 2] = (uint8_t)values[index];
        coefficient_bytes[index * 2 + 1] = (uint8_t)((uint16_t)values[index] >> 8);
    }
    input[1] = vp8_transform_hash(coefficient_bytes, sizeof(coefficient_bytes));
    input[2] = vp8_transform_hash(pixels, 528);
}

#endif
