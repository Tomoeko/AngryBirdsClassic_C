#ifndef AB_TEST_BUBBLE_CONTACT_CASES_H
#define AB_TEST_BUBBLE_CONTACT_CASES_H

#include <stdint.h>

static uint32_t bubble_contact_random(uint32_t *state) {
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

static uint32_t bubble_contact_float(uint32_t *state) {
    uint32_t value = bubble_contact_random(state);
    return (value & UINT32_C(0x807fffff)) | ((1u + ((value >> 23) % 254u)) << 23);
}

/* Identity, inBubble/shot types, fixture order, body velocity, prior pending
 * flag/vector, initial scalar pattern and trapped-callback mutations. */
static void bubble_contact_case(uint32_t sample, uint32_t output[11]) {
    uint32_t state = UINT32_C(0x520babb1) ^ (sample + 1u);
    output[0] = sample;
    output[1] = sample % 6u;
    output[2] = (sample / 6u) % 6u;
    output[3] = (sample / 36u) % 2u;
    output[4] = bubble_contact_float(&state);
    output[5] = bubble_contact_float(&state);
    output[6] = sample & 1u;
    output[7] = bubble_contact_float(&state);
    output[8] = bubble_contact_float(&state);
    output[9] = (sample / 72u) % 6u;
    output[10] = (sample / 432u) % 8u;
    if (sample % 17u == 0) {
        static const uint32_t edges[] = {0,
                                         UINT32_C(0x80000000),
                                         1,
                                         UINT32_C(0x80000001),
                                         UINT32_C(0x7f7fffff),
                                         UINT32_C(0xff7fffff)};
        output[4] = edges[(sample / 17u) % 6u];
        output[5] = edges[(sample / 17u + 1u) % 6u];
    }
}

#endif
