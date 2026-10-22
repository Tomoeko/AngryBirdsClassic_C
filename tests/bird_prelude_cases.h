#ifndef AB_TEST_BIRD_PRELUDE_CASES_H
#define AB_TEST_BIRD_PRELUDE_CASES_H

#include <stdint.h>

/* Hat mode, four object truth values, two mode values, score-entry presence,
 * callback mutations, damage/increment/scale/total/positions/hat fields and
 * optional achievement method. First object.name differs from incoming name
 * to distinguish score-table keys from registry keys. */
static void bird_prelude_case(uint32_t sample, uint32_t output[21]) {
    output[0] = sample;
    output[1] = sample % 7u;
    for (uint32_t field = 2; field <= 7; ++field) {
        output[field] = ((sample * (field * 2u + 1u)) / (field + 1u)) % 6u;
    }
    output[8] = (sample / 3u) & 1u;
    output[9] = (sample / 7u) & 255u;
    for (uint32_t field = 10; field <= 19; ++field) {
        uint32_t word =
            (sample + field * 701u) * UINT32_C(1664525) + UINT32_C(1013904223);
        output[field] =
            (word & UINT32_C(0x807fffff)) | ((120u + ((word >> 23) % 16u)) << 23);
    }
    output[20] = (sample / 5u) & 1u;
    if (sample < 216u) {
        output[1] = 0;
        output[2] = sample % 6u;
        output[3] = (sample / 6u) % 6u;
        output[4] = (sample / 36u) % 6u;
        output[5] = 0;
        output[6] = output[7] = 2;
        output[9] = 0;
        output[10] = UINT32_C(0x3f800000);
    }
    if (sample % 13u == 0) {
        output[10] = 0;
    } else if (sample % 17u == 0) {
        output[10] = UINT32_C(0x80000000);
    }
    if (sample % 11u == 0) {
        output[11] = UINT32_C(0x80000000);
    }
}

#endif
