#ifndef AB_TEST_REVOLUTE_INITIALIZE_CASES_H
#define AB_TEST_REVOLUTE_INITIALIZE_CASES_H

#include "weld_initialize_cases.h"

/* Two solver body frames, anchors, reference/lower/upper angles, duration,
 * warm ratio, retained impulse, velocities, motor impulse/torque/speed,
 * and one integer preceding limit-state word. */
static void revolute_initialize_case(uint32_t sample, uint32_t output[32]) {
    weld_initialize_case(sample, output);
    union {
        float number;
        uint32_t bits;
    } angle_a = {.bits = output[2]}, angle_b = {.bits = output[7]},
      reference = {.bits = output[14]}, lower, upper;
    float relative = angle_b.number - angle_a.number;
    relative -= reference.number;
    switch ((sample / 8) % 5) {
        case 0:
            lower.number = upper.number = 0;
            break;
        case 1:
            lower.number = relative + 0.5f;
            upper.number = relative + 1.5f;
            break;
        case 2:
            lower.number = relative - 1.5f;
            upper.number = relative - 0.5f;
            break;
        case 3:
            lower.number = relative - 1;
            upper.number = relative + 1;
            break;
        default:
            lower.number = 1;
            upper.number = -1;
            break;
    }
    output[15] = lower.bits;
    output[16] = upper.bits;
    output[28] = output[22];
    output[29] = output[23] & UINT32_C(0x7fffffff);
    output[30] = output[24];
    output[31] = (sample / 16) % 4;
    if (sample < 128) {
        for (size_t field = 0; field < 31; ++field) {
            output[field] = 0;
        }
        output[3] = output[4] = output[8] = output[9] = UINT32_C(0x3f800000);
        output[18] = UINT32_C(0x3f800000);
        output[19] = UINT32_C(0x3f800000);
        output[20] = UINT32_C(0x3e800000);
        output[21] = UINT32_C(0xbf000000);
        output[28] = UINT32_C(0x3f000000);
        output[16] = UINT32_C(0x3d8efa2e) + sample % 16;
        if (sample >= 64) {
            output[7] = sample & 32u ? UINT32_C(0xbe000000) : UINT32_C(0x3e000000);
        }
    }
}

#endif
