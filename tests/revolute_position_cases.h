#ifndef AB_TEST_REVOLUTE_POSITION_CASES_H
#define AB_TEST_REVOLUTE_POSITION_CASES_H

#include "revolute_initialize_cases.h"

static uint32_t revolute_position_limit_enabled(uint32_t sample) {
    return sample < 256 ? 1 : (sample >> 2) & 1u;
}

/* Initializer words followed by world-center Ax/Ay/Bx/By. */
static void revolute_position_case(uint32_t sample, uint32_t output[36]) {
    revolute_initialize_case(sample, output);
    uint32_t random = UINT32_C(0x4f92618d) ^ sample * UINT32_C(0x9e3779b9);
    for (size_t field = 0; field < 4; ++field) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        union {
            float number;
            uint32_t bits;
        } value = {.number = (float)((int32_t)(random & 1023u) - 512) * 0x1p-7f};
        output[32 + field] = value.bits;
    }
    if (sample < 256) {
        for (size_t field = 0; field < 36; ++field) {
            output[field] = 0;
        }
        output[3] = output[4] = output[8] = output[9] = UINT32_C(0x3f800000);
        output[18] = UINT32_C(0x3f800000);
        if (sample < 64) {
            output[34] = UINT32_C(0x3d4cccc5) + sample % 16;
            if (sample & 16u) {
                output[34] |= UINT32_C(0x80000000);
            }
        } else if (sample < 128) {
            output[7] = UINT32_C(0x3d0efa2e) + sample % 16;
            if (sample & 16u) {
                output[7] |= UINT32_C(0x80000000);
            }
        } else if (sample < 192) {
            output[7] = UINT32_C(0x3e0efa2e) + sample % 16;
            if (sample & 16u) {
                output[7] |= UINT32_C(0x80000000);
            }
        } else if (sample < 224) {
            output[7] = (UINT32_C(0x3d0efa2e) + sample % 16) | UINT32_C(0x80000000);
            output[16] = UINT32_C(0x3f800000);
        } else {
            output[7] = UINT32_C(0x3d0efa2e) + sample % 16;
            output[15] = UINT32_C(0xbf800000);
        }
    }
}

#endif
