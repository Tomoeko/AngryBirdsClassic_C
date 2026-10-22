#ifndef AB_TEST_BLOCK_REMOVAL_CLEANUP_CASES_H
#define AB_TEST_BLOCK_REMOVAL_CLEANUP_CASES_H

#include <stdint.h>

static void block_removal_cleanup_case(uint32_t sample, uint32_t output[15]) {
    output[0] = sample;
    for (uint32_t field = 1; field < 11; ++field) {
        output[field] = (sample * (field * 2u + 1u) + sample / (field + 1u)) % 6u;
    }
    output[4] = sample % 13u != 0;
    output[5] = sample % 17u != 0;
    output[11] = (sample / 19u) & 3u;
    output[12] = (sample * 23u + sample / 7u) & 63u;
    output[13] = (sample / 11u) & 15u;
    output[14] = (sample * 29u + sample / 13u) & 7u;
    if (sample < 2048) {
        output[1] = output[2] = output[3] = 5;
        output[4] = output[5] = 1;
        output[11] = 3;
    }
    if (sample < 256) {
        output[6] = sample % 6u;
        output[7] = (sample / 6u) % 6u;
        output[12] = output[14] = 0;
    }
}

#endif
