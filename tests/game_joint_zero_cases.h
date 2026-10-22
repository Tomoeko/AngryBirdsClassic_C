#ifndef AB_TEST_GAME_JOINT_ZERO_CASES_H
#define AB_TEST_GAME_JOINT_ZERO_CASES_H

#include "game_joint_cases.h"

static void game_joint_zero_case(uint32_t sample, uint32_t input[28]) {
    game_joint_case(sample, input);
    input[2] = UINT32_C(0x7fff);
    for (size_t field = 4; field < 14; ++field) {
        input[field] = ((sample >> (field - 2)) & 1u) ? UINT32_C(0x80000000) : 0;
    }
    input[14] = 0;
    input[15] = UINT32_C(0x3f000000);
    input[16] = sample & 16u ? UINT32_C(0x80000000) : 0;
    input[17] = 0;
    input[18] = sample & 32u ? UINT32_C(0x80000000) : 0;
    input[19] = sample & 64u ? UINT32_C(0x80000000) : 0;
    input[20] = sample & 128u ? UINT32_C(0x80000000) : 0;
    input[21] = sample & 256u ? UINT32_C(0x80000000) : 0;
    for (size_t field = 24; field < 28; ++field) {
        input[field] = ((sample >> (field - 22)) & 1u) ? UINT32_C(0x80000000) : 0;
    }
}

#endif
