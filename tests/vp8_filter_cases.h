#ifndef AB_TEST_VP8_FILTER_CASES_H
#define AB_TEST_VP8_FILTER_CASES_H

#include <stddef.h>
#include <stdint.h>

static uint32_t vp8_filter_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state >> 16;
}

static void vp8_filter_case(size_t sample, uint32_t input[5], uint8_t pixels[2048]) {
    uint32_t state = UINT32_C(0x520f117e) ^ (uint32_t)sample;
    input[0] = (uint32_t)(sample % 12);
    input[1] = vp8_filter_random(&state) % 128;
    input[2] = vp8_filter_random(&state) % 64;
    input[3] = vp8_filter_random(&state) % 8;
    uint32_t hash = UINT32_C(2166136261);
    for (size_t index = 0; index < 2048; ++index) {
        uint32_t random = vp8_filter_random(&state);
        unsigned row = (unsigned)(index % 1024 / 32);
        unsigned column = (unsigned)(index % 32);
        uint8_t value;
        /* Vary patterns independently of the twelve dispatch variants. */
        switch ((sample / 12) % 8) {
            case 0:
                value = (uint8_t)(120 + random % 17);
                break;
            case 1:
                value = (uint8_t)(120 + (column >= 8 ? 8 : 0));
                break;
            case 2:
                value = (uint8_t)(120 + (row >= 8 ? 8 : 0));
                break;
            case 3:
                value = (uint8_t)(row + column);
                break;
            case 4:
                value = (uint8_t)(random % 17);
                break;
            case 5:
                value = (uint8_t)(239 + random % 17);
                break;
            case 6:
                value = 128;
                break;
            default:
                value = (uint8_t)random;
                break;
        }
        pixels[index] = value;
        hash = (hash ^ value) * UINT32_C(16777619);
    }
    input[4] = hash;
}

#endif
