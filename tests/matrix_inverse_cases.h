#ifndef AB_TEST_MATRIX_INVERSE_CASES_H
#define AB_TEST_MATRIX_INVERSE_CASES_H

#include <stddef.h>
#include <stdint.h>

/* xx, xy, xz, yy, yz, zz; the native input duplicates symmetric elements. */
static void matrix_inverse_case(uint32_t sample, uint32_t output[6]) {
    uint32_t state = UINT32_C(0x70d1ea93) ^ sample * UINT32_C(0x9e3779b9);
    float values[6];
    for (size_t index = 0; index < 6; ++index) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        int32_t value = (int32_t)(state & 1023u) - 512;
        values[index] = (float)value * 0x1p-6f;
    }
    if (sample < 8) {
        for (size_t index = 0; index < 6; ++index) {
            values[index] = ((sample >> (index % 3)) & 1u) ? -0.0f : 0.0f;
        }
    } else if (sample < 16) {
        values[0] = sample & 1u ? 0.0f : 1.0f;
        values[3] = sample & 2u ? 0.0f : 2.0f;
        values[5] = sample & 4u ? 0.0f : 4.0f;
        values[1] = values[2] = values[4] = 0.0f;
    } else if (sample < 32) {
        float x = (float)(sample - 15) * 0.25f;
        float y = -2.0f;
        float z = 0.5f;
        values[0] = x * x;
        values[1] = x * y;
        values[2] = x * z;
        values[3] = y * y;
        values[4] = y * z;
        values[5] = z * z;
    } else if (sample >= 64 && sample < 96) {
        values[0] = values[3] = values[5] = 1.0f;
        values[1] = 1.0f - (float)(sample - 63) * 0x1p-24f;
        values[2] = values[4] = 0.0f;
    }
    for (size_t index = 0; index < 6; ++index) {
        union {
            float number;
            uint32_t bits;
        } value = {.number = values[index]};
        output[index] = value.bits;
    }
    if (sample >= 32 && sample < 64) {
        uint32_t exponent = 127u - ((sample - 32) % 21) * 2;
        for (size_t index = 0; index < 6; ++index) {
            output[index] = index == 0 || index == 3 || index == 5 ? exponent << 23 : 0;
        }
    }
}

#endif
