#ifndef AB_TEST_BIRD_SPECIALTY_CASES_H
#define AB_TEST_BIRD_SPECIALTY_CASES_H

#include <stdint.h>

static uint32_t bird_specialty_word(uint32_t sample, uint32_t salt) {
    uint32_t value = (sample + salt) * UINT32_C(1664525) + UINT32_C(1013904223);
    return (value & UINT32_C(0x807fffff)) | ((1u + ((value >> 23) % 254u)) << 23);
}

/* Flying identity; second/first sensors; collision; specialty; handler key;
 * two initial binding kinds; availability-after, sprite, update guard, timer,
 * special sprite, bubble/pink/availability fields; hat mode; definition present;
 * host mutations; force/damage/normal and property numeric payload. */
static void bird_specialty_case(uint32_t sample, uint32_t output[25]) {
    output[0] = sample;
    for (uint32_t field = 1; field < 20; ++field) {
        output[field] = ((sample * (field * 2u + 1u)) / (field + 1u)) % 6u;
    }
    output[1] = sample % 4u;
    output[5] = (sample / 3u) % 9u;
    output[6] = (sample / 5u) % 7u;
    output[7] = (sample / 7u) % 7u;
    output[8] = (sample / 11u) % 7u;
    output[17] = (sample / 13u) % 3u;
    output[18] = (sample / 17u) & 1u;
    output[19] = (sample * 733u + sample / 7u) & 32767u;
    for (uint32_t field = 20; field < 25; ++field) {
        output[field] = bird_specialty_word(sample, field * 701u);
    }
    /* Nonpositive damage keeps the earlier enclosing score phase inert. */
    output[21] |= UINT32_C(0x80000000);
    if (sample % 17u == 0) {
        output[20] = output[22] = 0;
        output[21] = output[23] = UINT32_C(0x80000000);
        output[24] = sample & 1u ? UINT32_C(0x80000000) : 0;
    }
    if (sample < 1024u) {
        output[1] = sample / 512u + 1u;
        output[2] = 0;
        output[4] = sample % 6u;
        output[5] = (sample / 6u) % 9u;
        output[6] = 0;
        output[9] = 0;
        output[10] = output[13] = 4;
        output[11] = 0;
        output[12] = (sample / 54u) % 6u;
        output[14] = (sample / 108u) % 6u;
        output[15] = 0;
        output[16] = 2;
        output[18] = 1;
        output[19] = 0;
    } else if (sample < 3072u) {
        output[1] = sample & 1u ? 1u : 2u;
        output[2] = output[4] = 0;
        output[6] = 4;
        output[7] = 5u + (sample & 1u);
        output[8] = 6u - (sample & 1u);
        output[18] = 1;
        output[19] &= 16383u;
    }
}

#endif
