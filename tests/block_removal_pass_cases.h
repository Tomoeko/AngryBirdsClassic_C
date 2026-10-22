#ifndef AB_TEST_BLOCK_REMOVAL_PASS_CASES_H
#define AB_TEST_BLOCK_REMOVAL_PASS_CASES_H

#include <stdint.h>

static void block_removal_pass_case(uint32_t sample, uint32_t input[5]) {
    input[0] = sample;
    input[1] = sample % 256u;
    input[2] = sample / 256u;
    input[3] = (sample / 7u) % 5u + 1u;
    input[4] = (sample / 3u) % 2u;
}

#endif
