#ifndef AB_TEST_CAMERA_SMOOTHSTEP_CASES_H
#define AB_TEST_CAMERA_SMOOTHSTEP_CASES_H

#include <stdint.h>

static uint32_t camera_smooth_word(float number) {
    union {
        float number;
        uint32_t word;
    } value = {.number = number};
    return value.word;
}

static uint32_t camera_smooth_mix(uint32_t value) {
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    return value ^ (value >> 16);
}

static void camera_smoothstep_case(uint32_t sample, uint32_t output[8]) {
    static const uint32_t special[] = {0,
                                       UINT32_C(0x80000000),
                                       UINT32_C(0x3f800000),
                                       UINT32_C(0xbf800000),
                                       UINT32_C(0x3f000000),
                                       UINT32_C(0x3e800000),
                                       UINT32_C(0x3f7fffff),
                                       UINT32_C(0x3f800001),
                                       UINT32_C(0x40000000),
                                       UINT32_C(0xc0000000),
                                       UINT32_C(0x7f800000),
                                       UINT32_C(0xff800000),
                                       UINT32_C(0x7fc12345),
                                       UINT32_C(0xffc54321),
                                       UINT32_C(0x7f800001),
                                       UINT32_C(0xff800001),
                                       UINT32_C(0x7f7fffff),
                                       UINT32_C(0xff7fffff),
                                       1,
                                       UINT32_C(0x80000001),
                                       UINT32_C(0x00800000),
                                       UINT32_C(0x80800000),
                                       UINT32_C(0x007fffff),
                                       UINT32_C(0x807fffff)};
    output[0] = sample;
    for (uint32_t index = 1; index < 8; ++index) {
        uint32_t seed = camera_smooth_mix(sample * 13u + index * 101u + 1u);
        output[index] = sample < 2048 ? special[seed % 24u] : seed;
    }
    if (sample % 7u == 0) {
        output[2] = output[1];
    }
    output[5] = camera_smooth_word((float)((int32_t)(sample % 257u) - 128) * 0.125f);
    output[6] = camera_smooth_word((float)((int32_t)(sample % 113u) - 56) * 0.25f);
}

#endif
