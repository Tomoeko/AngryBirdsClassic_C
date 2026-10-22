#ifndef AB_TEST_GAME_BEHAVIOR_ADD_CASES_H
#define AB_TEST_GAME_BEHAVIOR_ADD_CASES_H

#include <stdint.h>

enum {
    BEHAVIOR_ADD_CASES = 2048,
    BEHAVIOR_ADD_INPUTS = 16,
    BEHAVIOR_ADD_OUTPUTS = 52,
    BEHAVIOR_ADD_GROUPS = 6,
    BEHAVIOR_ADD_SLOTS = 4,
};

static const char *const behavior_add_names[] = {
    "MuseumBirdTrajectoryRenderer", "MuseumBirdCameraFollowBehavior", "Aa", "BB"};

static inline uint32_t behavior_add_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state ^ (*state >> 13);
}

static inline void behavior_add_case(uint32_t sample, uint32_t input[16]) {
    uint32_t state = sample ^ UINT32_C(0x197b369d);
    const uint32_t hints[] = {0, 2, 4};
    const uint32_t numbers[] = {0, UINT32_C(0x80000000), UINT32_C(0x3f800000),
                                UINT32_C(0xc1234567)};
    input[0] = sample;
    input[1] = hints[behavior_add_random(&state) % 3u];
    input[2] = behavior_add_random(&state) % 5u;
    input[3] = behavior_add_random(&state) % 4u;
    input[4] = behavior_add_random(&state) % 2u;
    input[5] = behavior_add_random(&state) % 4u;
    input[6] = behavior_add_random(&state) % 3u;
    input[7] = behavior_add_random(&state) % 6u;
    input[8] = numbers[behavior_add_random(&state) % 4u];
    input[9] = behavior_add_random(&state) % 4u != 0;
    input[10] = behavior_add_random(&state) % 8u;
    input[11] = 91;
    input[12] = behavior_add_random(&state) % 3u;
    input[13] = 1;
    input[14] = input[15] = 0;
}

#endif
