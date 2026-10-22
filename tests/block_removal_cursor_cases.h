#ifndef AB_TEST_BLOCK_REMOVAL_CURSOR_CASES_H
#define AB_TEST_BLOCK_REMOVAL_CURSOR_CASES_H

#include <stdint.h>

static uint32_t removal_cursor_pack(uint32_t value, uint32_t width) {
    uint32_t output = 0;
    for (uint32_t index = 0; index < 6; ++index) {
        output |= value << (index * width);
    }
    return output;
}

/* Objects/world kind, dead-table presence/live mask, world values, map hint,
 * rocket mutations, replacement-map live mask, dead values, strength/rocket/
 * timer/name packs, replacement hint and signed-zero/infinity strength bits. */
static void block_removal_cursor_case(uint32_t sample, uint32_t output[16]) {
    output[0] = sample;
    uint32_t random = sample * UINT32_C(1664525) + UINT32_C(1013904223);
    for (uint32_t field = 1; field < 16; ++field) {
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        output[field] = random;
    }
    output[1] %= 7u;
    output[2] %= 7u;
    output[3] = sample % 19u != 0;
    output[4] &= 63u;
    output[5] &= 4095u;
    output[6] %= 9u;
    output[7] &= 255u;
    output[8] &= 63u;
    output[9] &= 4095u;
    output[10] &= UINT32_C(0x3ffff);
    output[11] &= 4095u;
    output[12] &= 4095u;
    output[13] &= 4095u;
    output[14] %= 9u;
    output[15] = (sample / 17u) & 7u;
    if (sample < 2048u) {
        output[1] = output[2] = 6;
        output[3] = 1;
        output[4] = 63;
        output[9] = removal_cursor_pack(3, 2);
    }
    if (sample < 20u) {
        output[5] = output[8] = output[9] = 0;
        output[5] = output[9] = removal_cursor_pack(3, 2);
        output[8] = 63;
        output[10] = removal_cursor_pack(3, 3);
        output[11] = removal_cursor_pack(1, 2);
        output[12] = output[13] = output[7] = 0;
        if (sample == 0) {
            output[2] = output[3] = 0;
        } else if (sample == 1) {
            output[1] = 0;
        } else if (sample == 2 || sample == 3) {
            output[2] = 1;
            output[4] = sample == 2 ? 0u : 63u;
        } else if (sample == 4) {
            output[5] = removal_cursor_pack(1, 2);
            output[9] = 0;
        } else if (sample == 5) {
            output[10] = 0;
        } else if (sample == 6 || sample == 7) {
            output[10] = removal_cursor_pack(sample == 6 ? 5u : 6u, 3);
        } else if (sample == 9 || sample == 14) {
            output[12] = removal_cursor_pack(sample == 9 ? 1u : 2u, 2);
        } else if (sample == 10 || sample == 11 || sample >= 15) {
            static const uint32_t mutations[] = {16, 4, 8, 64, 128};
            output[7] = sample == 10 ? 1u : sample == 11 ? 2u : mutations[sample - 15u];
        } else if (sample == 12) {
            output[10] = removal_cursor_pack(1, 3);
            output[11] = 0;
        } else if (sample == 13) {
            output[11] = 0;
        }
    }
}

#endif
