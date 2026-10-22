#ifndef AB_TEST_JOINT_ISLAND_CASES_H
#define AB_TEST_JOINT_ISLAND_CASES_H

#include "prismatic_initialize_cases.h"

/* Kind, endpoint indices, and explicit constructor/retained-state words. The
 * generator's body-frame prefix stays reserved; island bodies supply frames. */
static void joint_island_input(uint32_t sample, uint32_t index, uint32_t body_count,
                               uint32_t output[41]) {
    output[0] = (sample / 8 + index) % 4;
    output[1] = (sample / 3 + index) % body_count;
    output[2] = (output[1] + 1 + (sample / 11) % (body_count - 1)) % body_count;
    prismatic_initialize_case(sample * 3 + index, output + 3);
    uint32_t *data = output + 3;
    if (output[0] < 2) {
        static const uint32_t frequencies[] = {
            0, UINT32_C(0x3e800000), UINT32_C(0x40800000), UINT32_C(0xbf800000)};
        data[15] = frequencies[(sample / 2 + index) % 4];
        data[16] = UINT32_C(0x3f000000);
        if (output[0] == 0) {
            data[14] &= UINT32_C(0x7fffffff);
        }
    }
}

#endif
