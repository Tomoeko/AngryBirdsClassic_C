#ifndef AB_TEST_OBJECT_REMOVED_CASES_H
#define AB_TEST_OBJECT_REMOVED_CASES_H

#include <stdint.h>

enum {
    OBJECT_REMOVED_INPUT_WORDS = 13,
    OBJECT_REMOVED_STATE_WORDS = 34,
    OBJECT_REMOVED_EVENT_WORDS = 40
};

/* World value; controllable/bait/eagle; exited/notify/check bindings;
 * check result; callback mutations; dt/time/score words. Scalar kinds are
 * nil, false, true, zero, text and table. Callable bindings use kind five. */
static void object_removed_case(uint32_t sample, uint32_t output[13]) {
    static const uint32_t numbers[] = {0,
                                       UINT32_C(0x80000000),
                                       UINT32_C(0x3d088889),
                                       UINT32_C(0xc1200000),
                                       UINT32_C(0x7f800000),
                                       UINT32_C(0xff800000),
                                       UINT32_C(0x7fc12345),
                                       UINT32_C(0xffc54321),
                                       UINT32_C(0x00000001),
                                       UINT32_C(0x4b800001)};
    output[0] = sample;
    for (uint32_t index = 1; index <= 8; ++index) {
        output[index] = (sample / (index * 3u + 1u) + sample * (index * 2u + 1u)) % 6u;
    }
    output[6] %= 3u;
    output[9] = (sample * 73u + sample / 11u) & 255u;
    for (uint32_t index = 10; index < 13; ++index) {
        output[index] = numbers[(sample / (index - 8u) + index) % 10u];
    }
    if (sample < 2048) {
        output[1] = 5;
        output[2] = sample < 1024 ? sample % 2u : (sample / 17u) % 6u;
        output[5] = output[7] = 5;
        output[6] = 2;
    }
    if (sample < 256) {
        output[2] = 0;
        output[8] = 2;
        output[9] = sample;
    }
    if (sample >= 256 && sample < 512) {
        output[2] = 2;
        output[3] = (sample / 6u) % 6u;
        output[4] = sample % 6u;
        output[9] = 0;
    }
}

#endif
