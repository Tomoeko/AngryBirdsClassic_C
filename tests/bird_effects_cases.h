#ifndef AB_TEST_BIRD_EFFECTS_CASES_H
#define AB_TEST_BIRD_EFFECTS_CASES_H

#include <stdint.h>

static const uint32_t bird_effects_numbers[] = {0,
                                                UINT32_C(0x80000000),
                                                UINT32_C(0x3f800000),
                                                UINT32_C(0x3f000000),
                                                UINT32_C(0x41200000),
                                                UINT32_C(0x41200001),
                                                UINT32_C(0x40000000),
                                                UINT32_C(0x41f00000),
                                                UINT32_C(0xbf800000),
                                                UINT32_C(0x42700000),
                                                UINT32_C(0x7f7fffff),
                                                UINT32_C(0x00800000),
                                                UINT32_C(0x3e4ccccd),
                                                UINT32_C(0x3f333333),
                                                UINT32_C(0x447a0000),
                                                UINT32_C(0x0da24260)};

/* Sensor, first-name/bird presence, material, x/y/mass kind, script/query kind,
 * particle collision, first/second collision and damage sounds, resolved audio
 * value, host mutations, force/damage/mass/x/y/threshold/replacement mass, audio
 * binding. Numeric kinds: 0=nil, 1=false, 2=number. */
static void bird_effects_case(uint32_t sample, uint32_t output[24]) {
    output[0] = sample;
    output[1] = (sample / 36u) % 6u;
    output[2] = sample % 19u != 0;
    output[3] = sample % 7u != 0;
    output[4] = (sample / 5u) % 6u;
    for (uint32_t field = 5; field < 8; ++field) {
        output[field] = ((sample * (field * 2u + 1u)) / (field + 1u)) % 3u;
    }
    output[8] = (sample / 7u) % 3u;
    for (uint32_t field = 9; field < 15; ++field) {
        output[field] = ((sample * (field * 2u + 1u)) / (field + 1u)) % 6u;
    }
    output[15] = (sample * 73u + sample / 7u) & 1023u;
    for (uint32_t field = 16; field < 23; ++field) {
        output[field] =
            bird_effects_numbers[((sample * (field * 2u + 1u)) / (field + 1u)) % 16u];
    }
    output[23] = sample % 29u == 0 ? 0u : 1u;
    if (sample < 2048u) {
        output[1] = 0;
        output[2] = output[3] = 1;
        output[4] = sample % 3u + 3u;
        output[5] = output[6] = output[7] = 2;
        output[8] = 1;
    }
}

#endif
