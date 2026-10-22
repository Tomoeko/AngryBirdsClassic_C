#ifndef AB_TEST_BLOCK_REMOVAL_EFFECTS_CASES_H
#define AB_TEST_BLOCK_REMOVAL_EFFECTS_CASES_H

#include <stdint.h>

static void block_removal_effects_case(uint32_t sample, uint32_t output[22]) {
    output[0] = sample;
    for (uint32_t field = 1; field < 22; ++field) {
        output[field] = (sample / (field + 1u) + sample * (field * 2u + 1u)) % 6u;
    }
    output[5] = (sample / 3u) % 9u;
    output[11] = (sample / 7u) % 3u;
    output[13] = (sample / 19u) & 63u;
    output[14] = (sample / 11u) % 3u;
    output[15] = (sample * 13u + sample / 7u) & 63u;
    output[16] = (sample * 19u + sample / 11u) & 63u;
    output[17] = (sample * 23u + sample / 13u) & 127u;
    output[18] = (sample * 29u + sample / 17u) & 31u;
    output[19] = (sample * 31u + sample / 23u) & 15u;
    output[21] = sample % 2u + 1u;
    if (sample < 2048) {
        output[11] = 1;
        output[12] = 4;
        output[13] = 63;
    }
    if (sample < 512) {
        output[1] = (sample / 12u) % 6u;
        output[2] = (sample / 24u) % 6u;
        output[3] = (sample / 6u) % 6u;
        output[5] = sample % 9u;
        output[14] = output[21] = 1;
        output[15] = output[16] = output[17] = output[18] = output[19] = 0;
    }
}

#endif
