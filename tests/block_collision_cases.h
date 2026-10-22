#ifndef AB_TEST_BLOCK_COLLISION_CASES_H
#define AB_TEST_BLOCK_COLLISION_CASES_H

#include <stdint.h>

enum {
    BLOCK_EVENT_LEVEL = 1,
    BLOCK_EVENT_DEFINITION,
    BLOCK_EVENT_NAMED_FIRST,
    BLOCK_EVENT_NAMED_SECOND,
    BLOCK_EVENT_SCRIPTS,
    BLOCK_EVENT_GAME_SCRIPT,
    BLOCK_EVENT_SPRITES,
    BLOCK_EVENT_DAMAGE_SOUND,
    BLOCK_EVENT_CONTACT_SOUND,
    BLOCK_EVENT_AUDIO_NAME,
    BLOCK_EVENT_AUDIO,
    BLOCK_EVENT_TRANSFORM,
    BLOCK_EVENT_EMIT,
    BLOCK_EVENT_SCORE,
    BLOCK_EVENT_AUDIO_REPLACED,
};

static uint32_t block_collision_bits(float value) {
    union {
        float number;
        uint32_t bits;
    } word = {.number = value};
    return word.bits;
}

static uint32_t block_collision_word(uint32_t sample, uint32_t salt) {
    uint32_t value = (sample + salt) * UINT32_C(1664525) + UINT32_C(1013904223);
    return (value & UINT32_C(0x807fffff)) | ((1u + ((value >> 23) % 254u)) << 23);
}

/* Identity, sensor types, optional host/state flags, level return type, force,
 * damage, normal/point, initial score, score query, sound flags and mutations. */
static void block_collision_case(uint32_t sample, uint32_t output[15]) {
    static const uint32_t forces[] = {0,
                                      UINT32_C(0x3f000000),
                                      UINT32_C(0x3f800000),
                                      UINT32_C(0x3f800001),
                                      UINT32_C(0x40000000),
                                      UINT32_C(0xbf800000)};
    static const float damages[] = {-0.0f, 0, 0.125f, 1, 1.25f, 100000};
    static const float scores[] = {0, 0.5f, 1, 10, -0.5f, 999, -0.0f};
    output[0] = sample;
    output[1] = sample % 6u;
    output[2] = (sample / 6u) % 6u;
    output[3] = (sample * 73u + sample / 36u) & 511u;
    output[4] = (sample / 36u) % 6u;
    output[5] = forces[(sample / 216u) % 6u];
    output[6] = block_collision_bits(damages[(sample * 7u / 37u) % 6u]);
    for (uint32_t field = 7; field <= 10; ++field) {
        output[field] = block_collision_word(sample, field * 701u);
    }
    output[11] = block_collision_bits((float)((int)(sample % 41u) - 20) * 0.25f);
    output[12] = block_collision_bits(scores[(sample / 13u) % 7u]);
    output[13] = (sample / 3u) % 16u;
    output[14] = (sample / 7u) % 64u;
    if (sample < 36) {
        output[3] = 511;
        output[4] = 2;
        output[5] = UINT32_C(0x40000000);
        output[6] = UINT32_C(0x3f800000);
        output[13] = 15;
        output[14] = 0;
    }
    if (sample % 17u == 0) {
        output[7] = output[9] = 0;
        output[8] = output[10] = UINT32_C(0x80000000);
    }
}

#endif
