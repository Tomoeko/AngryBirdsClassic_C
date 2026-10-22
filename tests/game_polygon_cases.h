#ifndef AB_TEST_GAME_POLYGON_CASES_H
#define AB_TEST_GAME_POLYGON_CASES_H

#include <stddef.h>
#include <stdint.h>

/* Sample, explicit dimensions, material, three normalized points, sprite
 * dimensions/scale, optional per-axis factors, presence mask and position.
 * Triangles are deliberately nondegenerate after scaling and native welding.
 */
static void game_polygon_case(uint32_t sample, uint32_t output[20]) {
    uint32_t random = UINT32_C(0x5203a79b) ^ sample * UINT32_C(0x9e3779b9);
    for (size_t field = 0; field < 20; ++field) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        union {
            float number;
            uint32_t bits;
        } value = {.number = (float)(1 + random % 128) * 0.25f};
        output[field] = value.bits;
    }
    output[0] = sample;
    if (sample % 4 == 0) {
        output[3] = 0;
    }
    output[4] = UINT32_C(0x3e99999a);
    output[5] = UINT32_C(0x3dcccccd);
    const uint32_t points[4][6] = {
        {0, 0, UINT32_C(0x3f800000), 0, 0, UINT32_C(0x3f800000)},
        {UINT32_C(0x3f800000), 0, 0, UINT32_C(0x3f800000), 0, 0},
        {0, UINT32_C(0x3f800000), UINT32_C(0x3f800000), 0, UINT32_C(0x3f800000),
         UINT32_C(0x3f800000)},
        {UINT32_C(0x3f800000), UINT32_C(0x3f800000), UINT32_C(0x3e800000), 0, 0,
         UINT32_C(0x3f000000)}};
    for (size_t field = 0; field < 6; ++field) {
        output[6 + field] = points[(sample / 8) % 4][field];
        if (!output[6 + field] && (sample & 32u)) {
            output[6 + field] = UINT32_C(0x80000000);
        }
    }
    output[12] = UINT32_C(0x42a00000);
    output[13] = UINT32_C(0x42700000);
    output[14] = UINT32_C(0x3d4ccccd);
    output[15] = UINT32_C(0x3f000000);
    output[16] = UINT32_C(0x3fc00000);
    output[17] = sample & 7u;
}

/* Keep the ordinary triangle corpus unchanged. Exercise native circles at zero,
 * signed zero, underflow, and the smallest normal sprite-width boundary. */
static void game_circle_zero_case(uint32_t sample, uint32_t output[20]) {
    game_polygon_case(sample, output);
    uint32_t variant = sample % 8u;
    output[17] = variant < 2 ? 1u : variant < 4 ? 0u : variant < 6 ? 2u : 0u;
    if (variant < 2) {
        output[1] = variant ? UINT32_C(0x80000000) : 0;
    } else if (variant < 4) {
        output[12] = variant & 1u ? UINT32_C(0x80000000) : 0;
    } else if (variant < 6) {
        output[15] = variant & 1u ? UINT32_C(0x80000000) : 0;
    } else {
        output[12] = variant & 1u ? UINT32_C(0x00800000) : UINT32_C(1);
    }
}

#endif
