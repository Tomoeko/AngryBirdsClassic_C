#ifndef AB_TEST_DAMAGE_PARTICLES_CASES_H
#define AB_TEST_DAMAGE_PARTICLES_CASES_H

#include <stdint.h>

static uint32_t damage_particles_word(uint32_t sample, uint32_t salt) {
    uint32_t value = (sample + salt) * UINT32_C(1664525) + UINT32_C(1013904223);
    return (value & UINT32_C(0x807fffff)) | ((1u + ((value >> 23) % 254u)) << 23);
}

/* Identity, descriptor type, physics/world positions, original/replaced amount,
 * transform-host mutations and initial name. */
static void damage_particles_case(uint32_t sample, uint32_t output[10]) {
    output[0] = sample;
    output[1] = sample % 6u;
    for (uint32_t field = 2; field < 8; ++field) {
        output[field] = damage_particles_word(sample, field * 701u);
    }
    output[8] = (sample / 6u) % 8u;
    output[9] = (sample / 48u) % 2u;
    if (sample % 17u == 0) {
        output[2] = output[4] = 0;
        output[3] = output[5] = UINT32_C(0x80000000);
    }
}

#endif
