#ifndef AB_TEST_WEBP_UPSAMPLE_CASES_H
#define AB_TEST_WEBP_UPSAMPLE_CASES_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t top_y[96], bottom_y[96];
    uint8_t top_u[48], top_v[48], bottom_u[48], bottom_v[48];
} WebpUpsampleCase;

static uint32_t webp_upsample_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state >> 16;
}

static void webp_upsample_case(size_t sample, uint32_t input[4],
                               WebpUpsampleCase *pixels) {
    uint32_t state = UINT32_C(0x520c0104) ^ (uint32_t)sample;
    input[0] = 1 + webp_upsample_random(&state) % 65;
    /* The native line-pair entry requires a top luma row; only the bottom row
     * is optional. Its first-pixel path dereferences top_y unconditionally. */
    input[1] = 1u | (((uint32_t)sample >> 4) & 1u) << 1;
    input[2] = ((uint32_t)sample >> 2) & 3;
    input[3] = UINT32_C(2166136261);
    uint8_t *planes[6] = {pixels->top_y, pixels->bottom_y, pixels->top_u,
                          pixels->top_v, pixels->bottom_u, pixels->bottom_v};
    static const uint8_t edges[6] = {0, 16, 128, 235, 240, 255};
    for (size_t plane = 0; plane < 6; ++plane) {
        size_t count = plane < 2 ? 96 : 48;
        for (size_t index = 0; index < count; ++index) {
            uint8_t value = (uint8_t)webp_upsample_random(&state);
            if (sample % 16 == 0) {
                value = edges[(sample / 16 + plane) % 6];
            }
            planes[plane][index] = value;
        }
    }
    if (input[2] & 1u) {
        for (size_t index = 0; index < 48; ++index) {
            pixels->bottom_u[index] = pixels->top_u[index];
            pixels->bottom_v[index] = pixels->top_v[index];
        }
    }
    if (input[2] & 2u) {
        for (size_t plane = 2; plane < 6; ++plane) {
            for (size_t index = 1; index < 48; ++index) {
                planes[plane][index] = planes[plane][0];
            }
        }
    }
    for (size_t plane = 0; plane < 6; ++plane) {
        size_t count = plane < 2 ? 96 : 48;
        for (size_t index = 0; index < count; ++index) {
            input[3] = (input[3] ^ planes[plane][index]) * UINT32_C(16777619);
        }
    }
}

#endif
