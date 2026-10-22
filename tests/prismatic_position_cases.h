#ifndef AB_TEST_PRISMATIC_POSITION_CASES_H
#define AB_TEST_PRISMATIC_POSITION_CASES_H

#include "prismatic_initialize_cases.h"

static uint32_t prismatic_position_limit_enabled(uint32_t sample) {
    return sample < 64 ? 0 : sample < 512 ? 1 : (sample >> 2) & 1u;
}

static void prismatic_position_case(uint32_t sample, uint32_t output[38]) {
    prismatic_initialize_case(sample, output);
    if (sample >= 512) {
        return;
    }
    for (size_t field = 0; field < 38; ++field) {
        output[field] = 0;
    }
    output[3] = output[4] = output[8] = output[9] = UINT32_C(0x3f800000);
    output[18] = output[32] = UINT32_C(0x3f800000);
    if (sample < 64) {
        output[37] = UINT32_C(0x3d4cccc5) + sample % 16;
        if (sample & 16u) {
            output[37] |= UINT32_C(0x80000000);
        }
    } else if (sample < 128) {
        output[7] = UINT32_C(0x3d0efa2e) + sample % 16;
        if (sample & 16u) {
            output[7] |= UINT32_C(0x80000000);
        }
    } else if (sample < 256) {
        output[36] =
            (sample < 192 ? UINT32_C(0x3d4cccc5) : UINT32_C(0x3e4cccc5)) + sample % 16;
        if (sample & 16u) {
            output[36] |= UINT32_C(0x80000000);
        }
    } else if (sample < 320) {
        output[15] = output[16] = output[36] = UINT32_C(0x40000000);
    } else if (sample < 384) {
        output[16] = UINT32_C(0x3f800000);
        output[36] = ((sample & 32u ? UINT32_C(0x3d4cccc5) : UINT32_C(0x3e4cccc5)) +
                      sample % 16) |
                     UINT32_C(0x80000000);
    } else if (sample < 448) {
        output[15] = UINT32_C(0xbf800000);
        output[36] =
            (sample & 32u ? UINT32_C(0x3d4cccc5) : UINT32_C(0x3e4cccc5)) + sample % 16;
    } else {
        output[16] = UINT32_C(0x3dccccc5) + (sample / 4) % 16;
        output[36] = UINT32_C(0x3e000000);
        if (sample & 16u) {
            output[32] = sample & 8u ? 1 : 0;
        }
    }
}

#endif
