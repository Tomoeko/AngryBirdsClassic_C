#ifndef AB_TEST_BLOCK_REMOVAL_SCORE_CASES_H
#define AB_TEST_BLOCK_REMOVAL_SCORE_CASES_H

#include <stdint.h>

static const uint32_t removal_score_numbers[] = {0,
                                                 UINT32_C(0x80000000),
                                                 UINT32_C(0x3f800000),
                                                 UINT32_C(0x41880000),
                                                 UINT32_C(0x459c4000),
                                                 UINT32_C(0x459c4800),
                                                 UINT32_C(0x7f7fffff),
                                                 UINT32_C(0x00800000),
                                                 UINT32_C(0x7f800000),
                                                 UINT32_C(0xff800000),
                                                 UINT32_C(0x7fc12345),
                                                 UINT32_C(0xffc12345),
                                                 UINT32_C(0x7f812345),
                                                 UINT32_C(0xff812345),
                                                 UINT32_C(0x3e800000),
                                                 UINT32_C(0xbf800000)};

/* Goal, definition/glyph/sound kinds; score/scale/total presence+bits, dense
 * array presence, x/y and resolved audio values, audio binding, destroy/query,
 * audio/floating mutations, replacement numbers, required host mask, destroy
 * binding kind and breaking-score callback mutations. */
static void block_removal_score_case(uint32_t sample, uint32_t output[25]) {
    output[0] = sample;
    for (uint32_t field = 1; field < 25; ++field) {
        output[field] = ((sample * (field * 2u + 1u)) / (field + 1u)) % 6u;
    }
    output[2] = (sample / 3u) % 7u;
    output[5] = sample % 11u != 0;
    output[7] = sample % 13u != 0;
    output[9] = sample % 19u != 0;
    output[11] = sample % 17u != 0;
    output[15] = (sample / 7u) % 3u;
    output[16] = (sample * 73u + sample / 7u) & 1023u;
    output[17] = (sample * 37u + sample / 11u) & 255u;
    output[18] = (sample * 19u + sample / 13u) & 127u;
    output[22] = (sample / 23u) & 15u;
    output[23] = (sample / 29u) & 3u;
    output[24] = (sample * 53u + sample / 17u) & 255u;
    const uint32_t fields[] = {6, 8, 10, 19, 20, 21};
    for (uint32_t index = 0; index < 6; ++index) {
        output[fields[index]] =
            removal_score_numbers[(sample / (index + 1u) + index * 3u) % 16u];
    }
    if (sample < 2048u) {
        output[2] = (sample / 5u) % 2u + 5u;
        output[22] = 15;
        output[23] = 3;
    }
    if (sample < 32u) {
        output[1] = sample % 2u ? 2u : 0u;
        output[2] = 5;
        output[3] = sample % 6u;
        output[4] = 4;
        output[5] = sample >= 16u;
        output[6] = removal_score_numbers[sample % 16u];
        output[7] = output[9] = output[11] = 1;
        output[8] = UINT32_C(0x3f800000);
        output[10] = 0;
        output[15] = 1;
        output[16] = output[17] = output[18] = output[24] = 0;
    }
    if (sample >= 32u && sample < 288u) {
        uint32_t pair = sample - 32u;
        output[1] = (pair / 16u) % 2u ? 2u : 0u;
        output[2] = 5;
        output[3] = (pair / 32u) % 6u;
        output[4] = pair % 6u;
        output[5] = output[7] = output[9] = output[11] = 1;
        output[6] = removal_score_numbers[pair % 16u];
        output[8] = removal_score_numbers[pair / 16u];
        output[10] = removal_score_numbers[(pair * 3u + pair / 16u) % 16u];
        output[15] = 1;
        output[16] = output[17] = output[18] = output[24] = 0;
    }
}

#endif
