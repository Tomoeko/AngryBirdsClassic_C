#ifndef AB_TEST_WELD_INITIALIZE_CASES_H
#define AB_TEST_WELD_INITIALIZE_CASES_H

#include <stddef.h>
#include <stdint.h>

/* Two local-center/angle/inverse-mass/inverse-inertia frames; anchors,
 * reference angle, frequency, damping; duration/ratio; impulse; velocities. */
static void weld_initialize_case(uint32_t sample, uint32_t output[28]) {
    uint32_t random = UINT32_C(0x45d9816b) ^ sample * UINT32_C(0x9e3779b9);
    float values[28];
    for (size_t index = 0; index < 28; ++index) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        int32_t value = (int32_t)(random & 1023u) - 512;
        values[index] = (float)value * 0x1p-7f;
    }
    if (sample < 16) {
        for (size_t index = 0; index < 28; ++index) {
            values[index] = ((sample >> (index % 4)) & 1u) ? -0.0f : 0.0f;
        }
        values[3] = sample & 1u ? 1.0f : 0.0f;
        values[4] = sample & 2u ? 1.0f : 0.0f;
        values[8] = sample & 4u ? 2.0f : 0.0f;
        values[9] = sample & 8u ? 2.0f : 0.0f;
    } else {
        values[3] = sample % 5 == 0 ? 0.0f : (float)(sample % 31 + 1) * 0x1p-4f;
        values[4] = sample % 7 == 0 ? 0.0f : (float)(sample % 23 + 1) * 0x1p-4f;
        values[8] = sample % 11 == 0 ? 0.0f : (float)(sample % 29 + 1) * 0x1p-4f;
        values[9] = sample % 7 == 0 ? 0.0f : (float)(sample % 19 + 1) * 0x1p-4f;
    }
    static const float frequencies[] = {0.0f, 0.25f, 4.0f, -1.0f};
    static const float ratios[] = {0.0f, 0.5f, 1.0f, 2.0f};
    values[15] = frequencies[(sample / 2) % 4];
    values[16] = (float)(sample % 9) * 0.125f;
    values[17] = sample % 13 == 0 ? 0.0f : sample % 3 ? 0x1.111112p-5f : 0x1.111112p-6f;
    values[18] = ratios[(sample / 8) % 4];
    for (size_t index = 0; index < 28; ++index) {
        union {
            float number;
            uint32_t bits;
        } value = {.number = values[index]};
        output[index] = value.bits;
    }
}

#endif
