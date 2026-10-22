#ifndef AB_TEST_SLING_LAUNCH_CASES_H
#define AB_TEST_SLING_LAUNCH_CASES_H

#include <stdbool.h>
#include <stdint.h>

enum { SLING_LAUNCH_INPUT_WORDS = 20, SLING_LAUNCH_EVENT_WORDS = 21 };

typedef struct {
    float direction[2], band[2], mass[2];
    float default_force, scale, shots, backwards;
    uint32_t role, mutations;
    bool eagle, special, alias_band;
} SlingLaunchFixture;

static inline uint32_t sling_launch_word(float value) {
    union {
        float number;
        uint32_t word;
    } result = {.number = value};
    return result.word;
}

static inline float sling_launch_number(uint32_t value) {
    union {
        float number;
        uint32_t word;
    } result = {.word = value};
    return result.number;
}

static inline uint32_t sling_launch_random(uint32_t *state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static inline void sling_launch_case(uint32_t sample, uint32_t output[20]) {
    uint32_t state = sample ^ UINT32_C(0x5201a04b);
    for (uint32_t index = 0; index < 12; ++index) {
        uint32_t random = sling_launch_random(&state);
        output[index] = ((124 + random % 6) << 23) | (random & UINT32_C(0x7fffff));
        if (index == 0 || index == 1 || index == 3 || index == 9 || index >= 10)
            output[index] |= random & UINT32_C(0x80000000);
    }
    output[12] = sample % 5;
    output[13] = (sample / 5) % 5;
    output[14] = (sample / 25) % 5;
    output[15] = (sample / 3) % 4;
    output[16] = sample % 512;
    output[17] = sample % 3;
    output[18] = sample % 7 == 0 ? (sample / 7) % 10 : 0;
    output[19] = sample % 11 == 0 ? (sample / 11) % 4 : 0;
    if (sample % 17 == 0)
        output[0] = sample & 1 ? UINT32_C(0x80000000) : 0;
    if (sample % 31 == 0)
        output[9] = sample & 2 ? UINT32_C(0x80000000) : 0;
    if (output[17] == 2 && !(output[16] & 8) && sample % 19 == 0)
        output[3] = UINT32_C(0x7fc12345);
    if (!(output[16] & 4) && sample % 23 == 0)
        output[6] = UINT32_C(0x7fc54321);
}

static inline SlingLaunchFixture sling_launch_fixture(const uint32_t input[20]) {
    return (SlingLaunchFixture){
        .direction = {sling_launch_number(input[0]), sling_launch_number(input[1])},
        .band = {sling_launch_number(input[10]), sling_launch_number(input[11])},
        .mass = {sling_launch_number(input[5]), sling_launch_number(input[6])},
        .default_force = sling_launch_number(input[3]),
        .scale = sling_launch_number(input[4]),
        .shots = 10,
        .backwards = 4,
        .mutations = input[16],
        .eagle = input[12] == 2,
        .special = input[13] >= 2,
        .alias_band = (input[15] & 2) != 0};
}

static inline float *sling_launch_band(SlingLaunchFixture *fixture) {
    return fixture->alias_band ? fixture->direction : fixture->band;
}

static inline void sling_launch_snapshot(SlingLaunchFixture *fixture,
                                         uint32_t output[13]) {
    float *band = sling_launch_band(fixture);
    output[0] = fixture->eagle;
    output[1] = fixture->special;
    output[2] = fixture->role;
    const float values[] = {fixture->mass[0],
                            fixture->mass[1],
                            fixture->default_force,
                            fixture->scale,
                            fixture->direction[0],
                            fixture->direction[1],
                            band[0],
                            band[1],
                            fixture->shots,
                            fixture->backwards};
    for (uint32_t index = 0; index < 10; ++index)
        output[index + 3] = sling_launch_word(values[index]);
}

/* Controlled environment effects shared by the C and native observers. The
 * original launch function independently selects their order and arithmetic. */
static inline void sling_launch_effect(SlingLaunchFixture *fixture, uint32_t operation,
                                       float impulse[2]) {
    uint32_t flags = fixture->mutations;
    if (operation == 2 && (flags & 1)) {
        fixture->direction[0] += 0.25f;
        fixture->eagle = !fixture->eagle;
    }
    if (operation == 3) {
        fixture->backwards += 1;
        if (flags & 2) {
            fixture->direction[1] -= 0.5f;
            fixture->eagle = fixture->special = true;
        }
    }
    if (operation == 4) {
        fixture->shots += 1;
        if (flags & 4)
            fixture->role = 1;
    }
    if (operation == 5 && (flags & 8)) {
        fixture->default_force *= 0.5f;
        fixture->scale += 0.125f;
        fixture->mass[fixture->role] *= 2;
    }
    if (operation == 6 && (flags & 16)) {
        fixture->mass[fixture->role] += 0.25f;
        fixture->direction[0] = -fixture->direction[0];
    }
    if (operation == 7 && (flags & 32)) {
        fixture->mass[fixture->role] *= 0.5f;
        fixture->scale *= 2;
        fixture->direction[1] += 0.75f;
    }
    if (operation == 8 && (flags & 64)) {
        sling_launch_band(fixture)[0] += 2;
        fixture->direction[0] += 3;
    }
    if (operation == 9 && (flags & 256)) {
        sling_launch_band(fixture)[1] += 1;
        impulse[0] -= 0.5f;
    }
}

#endif
