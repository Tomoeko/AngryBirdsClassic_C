#ifndef AB_TEST_JOINT_GEOMETRY_CASES_H
#define AB_TEST_JOINT_GEOMETRY_CASES_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

static uint32_t joint_case_next(uint32_t *state) {
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

/* Two position/sine/cosine/angle frames, two anchors and one world axis. */
static void joint_geometry_case(uint32_t sample, uint32_t output[16]) {
    float values[16];
    uint32_t state = UINT32_C(0x31ac760d) ^ sample * UINT32_C(0x9e3779b9);
    for (size_t index = 0; index < 16; ++index) {
        int32_t signed_value = (int32_t)(joint_case_next(&state) & 4095u) - 2048;
        bool rotation = index == 2 || index == 3 || index == 7 || index == 8;
        values[index] = (float)signed_value * (rotation ? 0x1p-11f : 0x1p-4f);
    }
    if (sample < 16) {
        for (size_t index = 0; index < 16; ++index) {
            values[index] = ((sample >> (index % 4)) & 1u) ? -0.0f : 0.0f;
        }
        values[3] = values[8] = 1.0f;
    } else if (sample < 32) {
        values[2] = sample & 1u ? -1.0f : 1.0f;
        values[3] = 0.0f;
        values[7] = 0.0f;
        values[8] = sample & 2u ? -1.0f : 1.0f;
    }
    if (sample % 13 == 0) {
        values[12] = values[10];
        values[13] = values[11];
    }
    if (sample % 19 == 0) {
        for (size_t index = 0; index < 5; ++index) {
            values[5 + index] = values[index];
        }
    }
    if (sample % 23 == 0) {
        values[14] = values[15] = 0.0f;
    }
    for (size_t index = 0; index < 16; ++index) {
        union {
            float number;
            uint32_t bits;
        } value = {.number = values[index]};
        output[index] = value.bits;
    }
    if (sample >= 32 && sample < 64) {
        /* ARMv7 NEON setup arithmetic can flush subnormal products. Construct
         * the controlled inputs as words, independent of that execution mode. */
        for (size_t index = 0; index < 16; ++index) {
            output[index] = (output[index] & UINT32_C(0x80000000)) |
                            ((output[index] & UINT32_C(0x007fffff)) >> 4);
        }
        output[2] = output[7] = 0;
        output[3] = output[8] = UINT32_C(0x3f800000);
    }
}

#endif
