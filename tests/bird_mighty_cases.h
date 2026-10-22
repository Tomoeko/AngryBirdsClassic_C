#ifndef AB_TEST_BIRD_MIGHTY_CASES_H
#define AB_TEST_BIRD_MIGHTY_CASES_H

#include <stdint.h>

/* Flying binding: nil/first/second/third/missing world. Bait/eagle/sensors use
 * nil/false/true/zero/string/table; collision binding uses
 * nil/false/zero/table/function1/function2. Last word controls callback mutations. */
static void bird_mighty_case(uint32_t sample, uint32_t output[8]) {
    output[0] = sample;
    output[1] = sample % 5u;
    for (uint32_t field = 2; field <= 6; ++field) {
        output[field] = ((sample * (field * 2u + 1u)) / (field + 1u)) % 6u;
    }
    output[7] = (sample / 7u) & 63u;
    if (sample < 432u) {
        output[1] = (sample / 216u) + 1u;
        output[2] = sample % 6u;
        output[3] = (sample / 6u) % 6u;
        output[4] = output[5] = (sample / 36u) % 6u;
        output[6] = 4u + (sample & 1u);
    }
}

#endif
