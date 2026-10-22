#ifndef AB_TEST_GAME_JOINT_LIMIT_CASES_H
#define AB_TEST_GAME_JOINT_LIMIT_CASES_H

#include "game_joint_cases.h"

static void game_joint_limit_case(uint32_t sample, uint32_t input[28]) {
    game_joint_case(sample, input);
    input[1] = 3 + sample % 2;
    if (sample < 128) {
        for (size_t field = 4; field < 14; ++field) {
            input[field] = ((sample >> (field - 2)) & 1u) ? UINT32_C(0x80000000) : 0;
        }
    }
    if (sample >= 128 && sample < 384) {
        const uint32_t boundaries[] = {UINT32_C(0x80000001),
                                       UINT32_C(0x80000000),
                                       0,
                                       1,
                                       UINT32_C(0x3f7fffff),
                                       UINT32_C(0x3f800000),
                                       UINT32_C(0x3f800001),
                                       UINT32_C(0x3f800002)};
        uint32_t coordinate = boundaries[(sample / 2) % 8];
        input[2] = UINT32_C(0x7fff);
        input[23] |= 6u;
        for (size_t field = 4; field < 14; ++field) {
            input[field] = 0;
        }
        input[16] = sample & 16u ? UINT32_C(0xbf800000) : UINT32_C(0x3f800000);
        input[18] = 0;
        input[19] = UINT32_C(0x3f800000);
        input[20] = UINT32_C(0x3f800000);
        input[21] = 0;
        input[24] = input[25] = input[26] = input[27] = 0;
        input[input[1] == 3 ? 25 : 26] = coordinate;
    }
}

#endif
