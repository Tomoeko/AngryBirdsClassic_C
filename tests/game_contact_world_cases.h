#ifndef AB_TEST_GAME_CONTACT_WORLD_CASES_H
#define AB_TEST_GAME_CONTACT_WORLD_CASES_H

#include <stdint.h>

enum {
    CONTACT_WORLD_FIRST_SCRIPT = 1,
    CONTACT_WORLD_SECOND_SCRIPT = 2,
    CONTACT_WORLD_FIRST_OBJECT = 4,
    CONTACT_WORLD_SECOND_OBJECT = 8,
    CONTACT_WORLD_DISABLE = 16,
    CONTACT_WORLD_OFFER = 32,
    CONTACT_WORLD_MASKED = 64,
    CONTACT_WORLD_BEGIN_DISABLED = 128,
};

static uint32_t contact_world_bits(float value) {
    union {
        float number;
        uint32_t bits;
    } word = {.number = value};
    return word.bits;
}

/* Flags, two circle radii, dynamic origin/velocity, and case identity. These
 * are controlled original-engine inputs, independent of gameplay defaults. */
static void game_contact_world_case(uint32_t sample, uint32_t output[8]) {
    static const uint32_t flags[] = {63, 15, 31, 79, 14, 13, 11, 7, 143};
    static const float radii[] = {1, 0.75f, 0.5f};
    float radius = radii[(sample / 9u) % 3u];
    output[0] = flags[sample % 9u];
    output[1] = contact_world_bits(1);
    output[2] = contact_world_bits(radius);
    output[3] = contact_world_bits(1 + radius - 0.125f);
    output[4] = contact_world_bits((float)((int)((sample / 3u) % 3u) - 1) * 0.125f);
    output[5] = contact_world_bits(-(float)(1u + sample % 3u) * 0.25f);
    output[6] = contact_world_bits((float)((int)((sample / 27u) % 3u) - 1) * 0.125f);
    output[7] = sample;
}

static void game_contact_world_proposal(uint32_t sample, uint32_t frame,
                                        uint32_t output[2]) {
    output[0] = contact_world_bits(-(float)(1u + (sample + frame) % 4u) * 0.25f);
    output[1] = contact_world_bits((float)((int)((sample + frame) % 5u) - 2) * 0.125f);
}

#endif
