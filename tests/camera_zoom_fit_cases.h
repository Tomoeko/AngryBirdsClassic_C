#ifndef AB_TEST_CAMERA_ZOOM_FIT_CASES_H
#define AB_TEST_CAMERA_ZOOM_FIT_CASES_H

#include <stdint.h>

static void camera_zoom_fit_case(uint32_t sample, uint32_t output[8]) {
    static const uint32_t special[] = {0,
                                       UINT32_C(0x80000000),
                                       UINT32_C(0x3f800000),
                                       UINT32_C(0xbf800000),
                                       UINT32_C(0x3f000000),
                                       UINT32_C(0x40000000),
                                       UINT32_C(0xc0000000),
                                       UINT32_C(0x7f7fffff),
                                       UINT32_C(0xff7fffff),
                                       1,
                                       UINT32_C(0x80000001),
                                       UINT32_C(0x00800000),
                                       UINT32_C(0x80800000),
                                       UINT32_C(0x007fffff),
                                       UINT32_C(0x807fffff),
                                       UINT32_C(0x43a00000)};
    output[0] = sample;
    uint32_t seed = sample * UINT32_C(0x9e3779b9) + 1u;
    for (uint32_t index = 1; index < 8; ++index) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        uint32_t word = sample < 2048 ? special[seed % 16u] : seed;
        if ((word & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000)) {
            word ^= UINT32_C(0x00800000);
        }
        output[index] = word;
    }
    if (sample % 5u == 0) {
        output[2] = output[4];
    }
}

#endif
