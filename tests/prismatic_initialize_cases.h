#ifndef AB_TEST_PRISMATIC_INITIALIZE_CASES_H
#define AB_TEST_PRISMATIC_INITIALIZE_CASES_H

#include "revolute_initialize_cases.h"

/* Revolute-style body/anchor/limit/step/impulse/motor words, raw local axis,
 * then world-center Ax/Ay/Bx/By. Word 31 is the integer preceding limit state. */
static void prismatic_initialize_case(uint32_t sample, uint32_t output[38]) {
    revolute_initialize_case(sample, output);
    uint32_t random = UINT32_C(0x6a18d29f) ^ sample * UINT32_C(0x9e3779b9);
    for (size_t field = 32; field < 38; ++field) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        union {
            float number;
            uint32_t bits;
        } value = {.number = (float)((int32_t)(random & 1023u) - 512) * 0x1p-7f};
        output[field] = value.bits;
    }
    if (sample < 128) {
        static const uint32_t axes[] = {
            0,
            UINT32_C(0x80000000),
            1,
            UINT32_C(0x80000001),
            UINT32_C(0x007fffff),
            UINT32_C(0x00800000),
            UINT32_C(0x33ffffff),
            UINT32_C(0x34000000),
            UINT32_C(0x34000001),
            UINT32_C(0xb3ffffff),
            UINT32_C(0xb4000000),
            UINT32_C(0xb4000001),
            UINT32_C(0x3f800000),
            UINT32_C(0xbf800000),
            UINT32_C(0x40400000),
            UINT32_C(0xc0800000),
        };
        output[32] = axes[(sample / 8) % 16];
        output[33] = sample & 16u ? output[32] : 0;
        output[15] = 0;
        output[16] = UINT32_C(0x3dccccc5) + (sample / 8) % 16;
        if (sample & 32u) {
            output[16] |= UINT32_C(0x80000000);
        }
        if (sample & 8u) {
            output[4] = output[9] = 0;
        }
    }
}

#endif
