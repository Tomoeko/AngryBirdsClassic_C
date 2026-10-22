#ifndef AB_TEST_BLOCK_REMOVAL_DETACH_CASES_H
#define AB_TEST_BLOCK_REMOVAL_DETACH_CASES_H

#include <stdint.h>

static void block_removal_detach_case(uint32_t sample, uint32_t output[11]) {
    output[0] = sample;
    for (uint32_t field = 1; field < 9; ++field) {
        output[field] = (sample * (field * 2u + 1u) + sample / (field + 1u)) % 6u;
    }
    output[9] = sample % 11u != 0;
    output[10] = (sample * 19u + sample / 7u) & 31u;
    if (sample < 2048) {
        output[1] = output[2] = output[4] = 5;
        output[3] = 4;
    }
}

#endif
