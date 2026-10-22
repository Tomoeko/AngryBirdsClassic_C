#ifndef AB_TEST_LUA_ASSET_CONSTRUCT_CASES_H
#define AB_TEST_LUA_ASSET_CONSTRUCT_CASES_H

#include "scene_id_cases.h"

static inline void asset_construct_case(uint32_t sample, uint32_t output[3]) {
    const uint32_t layers[] = {
        UINT32_C(0xc0700000), UINT32_C(0x80000000), UINT32_C(0x40200000),
        UINT32_C(0x40800000), UINT32_C(0x501502f9), UINT32_C(0xd01502f9),
        UINT32_C(0x7fc12345), UINT32_C(0x7f800000), UINT32_C(0xff800000)};
    output[0] = scene_id_case(sample);
    output[1] = layers[sample % (sizeof(layers) / sizeof(*layers))];
    output[2] = sample % 5 != 0;
}

#endif
