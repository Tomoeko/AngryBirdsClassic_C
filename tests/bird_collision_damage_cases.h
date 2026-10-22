#ifndef AB_TEST_BIRD_COLLISION_DAMAGE_CASES_H
#define AB_TEST_BIRD_COLLISION_DAMAGE_CASES_H

#include <stdint.h>

/* Sensor/control; blockTable kind, blocks value; descriptor presence;
 * new and old sprite/blink/smile values; callback mutations; particle descriptor;
 * physics/world coordinates and original/replacement particle amount. */
static void bird_collision_damage_case(uint32_t sample, uint32_t output[20]) {
    output[0] = sample;
    output[1] = (sample / 36u) % 6u;
    output[2] = (sample / 6u) % 6u;
    output[3] = (sample / 7u) % 7u;
    output[4] = (sample / 11u) % 7u;
    output[5] = sample % 17u != 0;
    for (uint32_t field = 6; field < 12; ++field) {
        output[field] = ((sample * (field * 2u + 1u)) / (field + 1u)) % 9u;
    }
    output[12] = (sample * 73u + sample / 7u) & 1023u;
    output[13] = (sample / 13u) % 6u;
    for (uint32_t field = 14; field < 20; ++field) {
        uint32_t word =
            (sample + field * 701u) * UINT32_C(1664525) + UINT32_C(1013904223);
        output[field] =
            (word & UINT32_C(0x807fffff)) | ((1u + ((word >> 23) % 254u)) << 23);
    }
    if (sample < 2048u) {
        output[1] = 0;
        output[2] = sample % 6u;
        output[3] = output[4] = 6;
    }
    if (sample % 17u == 0) {
        output[14] = output[16] = 0;
        output[15] = output[17] = UINT32_C(0x80000000);
    }
}

#endif
