#ifndef AB_TEST_DISTANCE_CASES_H
#define AB_TEST_DISTANCE_CASES_H

#include "weld_position_cases.h"

/* Body frames, anchors, length/frequency/damping, duration/ratio, impulse,
 * two reserved zero words, velocities and world centers. */
static void distance_case(uint32_t sample, uint32_t output[32]) {
    weld_position_case(sample + 192, output);
    output[14] &= UINT32_C(0x7fffffff);
    output[20] = output[21] = 0;
    if (sample < 192) {
        for (size_t field = 0; field < 32; ++field) {
            output[field] = 0;
        }
        output[3] = output[4] = output[8] = output[9] = UINT32_C(0x3f800000);
        output[18] = UINT32_C(0x3f800000);
        if (sample < 64) {
            output[30] = UINT32_C(0x3d4cccc5) + sample % 16;
            if (sample & 16u) {
                output[30] |= UINT32_C(0x80000000);
            }
            if (sample >= 32) {
                output[14] = UINT32_C(0x3d4ccccd);
            }
        } else if (sample < 96) {
            output[14] = UINT32_C(0x3f800000);
            output[30] = UINT32_C(0x33fffff8) + sample % 16;
            if (sample & 16u) {
                output[30] |= UINT32_C(0x80000000);
            }
        } else if (sample < 128) {
            output[30] = UINT32_C(0x3e4cccc5) + sample % 16;
            if (sample & 16u) {
                output[30] |= UINT32_C(0x80000000);
            }
        } else if (sample < 160) {
            output[14] = UINT32_C(0x3f800000);
            output[30] = sample & 1u ? UINT32_C(0x80000000) : 0;
        } else {
            output[14] = UINT32_C(0x3f800000);
            output[15] = UINT32_C(0x40800000);
            output[16] = UINT32_C(0x3f000000);
            output[17] = sample & 1u ? UINT32_C(0x3c888889) : 0;
            output[30] = UINT32_C(0x3d4cccc5) + sample % 16;
        }
    }
}

#endif
