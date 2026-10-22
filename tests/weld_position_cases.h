#ifndef AB_TEST_WELD_POSITION_CASES_H
#define AB_TEST_WELD_POSITION_CASES_H

#include "weld_initialize_cases.h"

/* Initializer inputs plus world-center Ax/Ay/Bx/By. */
static void weld_position_case(uint32_t sample, uint32_t output[32]) {
    weld_initialize_case(sample, output);
    uint32_t random = UINT32_C(0x126b45d9) ^ sample * UINT32_C(0x9e3779b9);
    for (size_t index = 0; index < 4; ++index) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        int32_t integer = (int32_t)(random & 1023u) - 512;
        union {
            float number;
            uint32_t bits;
        } value = {.number = (float)integer * 0x1p-7f};
        output[28 + index] = value.bits;
    }
    /* A body without inverse mass also has no inverse inertia in a world. */
    if ((output[3] & UINT32_C(0x7fffffff)) == 0) {
        output[4] = 0;
    }
    if ((output[8] & UINT32_C(0x7fffffff)) == 0) {
        output[9] = 0;
    }
    if (sample < 96) {
        for (size_t index = 0; index < 32; ++index) {
            output[index] = 0;
        }
        output[3] = output[4] = output[8] = output[9] = UINT32_C(0x3f800000);
        output[18] = UINT32_C(0x3f800000);
        if (sample < 32) {
            uint32_t distance = UINT32_C(0x3d4cccc5) + sample % 16;
            output[30] = sample < 16 ? distance : distance | UINT32_C(0x80000000);
        } else if (sample < 64) {
            uint32_t angle = UINT32_C(0x3d0efa2e) + sample % 16;
            output[7] = sample < 48 ? angle : angle | UINT32_C(0x80000000);
        } else if (sample < 80) {
            output[4] = output[9] = 0;
            output[7] = UINT32_C(0x3f000000);
        } else {
            output[7] = sample & 1u ? UINT32_C(0xbf800000) : UINT32_C(0x3f800000);
            output[15] = UINT32_C(0x40800000);
        }
    }
}

#endif
