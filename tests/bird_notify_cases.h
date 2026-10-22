#ifndef AB_TEST_BIRD_NOTIFY_CASES_H
#define AB_TEST_BIRD_NOTIFY_CASES_H

#include <stdint.h>

static uint32_t bird_notify_word(uint32_t sample, uint32_t salt) {
    uint32_t value = (sample + salt) * UINT32_C(1664525) + UINT32_C(1013904223);
    return (value & UINT32_C(0x807fffff)) | ((1u + ((value >> 23) % 254u)) << 23);
}

/* Identity, projectile types, sensor types, second controllable, handler flags,
 * host mutations, six contact numbers, point presence and related-hat fixture.
 * Related hats stop the original outer function immediately after this prefix;
 * that independent guard and all later gameplay are outside this comparison. */
static void bird_notify_case(uint32_t sample, uint32_t output[16]) {
    output[0] = sample;
    output[1] = sample % 6u;
    output[2] = (sample / 6u) % 6u;
    output[3] = (sample / 36u) % 6u;
    output[4] = (sample / 216u) % 6u;
    output[5] = (sample / 1296u) % 6u;
    output[6] = (sample * 73u + sample / 36u) & 127u;
    output[7] = (sample / 13u) % 64u;
    for (uint32_t field = 8; field <= 13; ++field) {
        output[field] = bird_notify_word(sample, field * 701u);
    }
    output[14] = sample & 1u;
    output[15] = (sample / 2u) & 1u;
    if (sample < 512) {
        output[1] = output[2] = 0;
        output[3] = sample % 6u;
        output[4] = (sample / 6u) % 6u;
        output[5] = (sample / 36u) % 6u;
        output[6] = 127;
    }
    if (sample % 17u == 0) {
        output[10] = output[12] = 0;
        output[11] = output[13] = UINT32_C(0x80000000);
    }
}

#endif
