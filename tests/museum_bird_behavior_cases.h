#ifndef AB_TEST_MUSEUM_BIRD_BEHAVIOR_CASES_H
#define AB_TEST_MUSEUM_BIRD_BEHAVIOR_CASES_H

#include <stdint.h>

enum { MUSEUM_BEHAVIOR_CASES = 2048, MUSEUM_BEHAVIOR_EVENT_WORDS = 8 };

typedef struct {
    uint32_t tag, value;
} MuseumCaseValue;

static inline uint32_t museum_case_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state ^ (*state >> 13);
}

static inline void museum_behavior_case(uint32_t sample, uint32_t input[16]) {
    uint32_t state = sample ^ UINT32_C(0x56a27c91);
    const uint32_t numbers[] = {0,
                                UINT32_C(0x80000000),
                                UINT32_C(0x3f800000),
                                UINT32_C(0xc0000000),
                                UINT32_C(0x7fc12345),
                                UINT32_C(0x7f800000),
                                UINT32_C(0xff800000)};
    input[0] = sample;
    input[1] = museum_case_random(&state) % 2u;
    input[2] = museum_case_random(&state) % 4u;
    for (uint32_t index = 3; index < 8; ++index)
        input[index] = museum_case_random(&state) % 6u;
    input[8] = numbers[museum_case_random(&state) % 7u];
    input[9] = museum_case_random(&state) % 4u;
    input[10] = museum_case_random(&state) % 6u;
    input[11] = museum_case_random(&state) % 5u;
    input[12] = numbers[museum_case_random(&state) % 7u];
    input[13] = museum_case_random(&state) % 5u;
    input[14] = museum_case_random(&state) % 64u;
    input[15] = museum_case_random(&state) % 32u;
    if (sample < 256) {
        input[1] = sample < 128 ? 0u : 1u;
        const uint32_t trajectory_events[] = {1, 2, 0, 3};
        input[2] = sample < 128   ? trajectory_events[sample / 32u]
                   : sample < 192 ? 3u
                                  : 0u;
        input[3] = 5;
        input[4] = 0;
        input[5] = 4;
        input[6] = 1;
        input[7] = 2;
        input[9] = 1;
        input[10] = 1;
        input[11] = 0;
        const uint32_t launch_words[] = {UINT32_C(0xc0000000), UINT32_C(0x80000000),
                                         UINT32_C(0x7fc12345), UINT32_C(0xff800000),
                                         UINT32_C(0x3f800000), UINT32_C(0x7f800000),
                                         UINT32_C(0xc0000000), UINT32_C(0xff800000)};
        input[12] = launch_words[(sample / 8u) % 8u];
        input[13] = 2;
        input[14] = sample % 64u;
        input[15] = 31;
    }
}

static inline MuseumCaseValue museum_scalar(uint32_t kind, uint32_t bits,
                                            uint32_t table) {
    const uint32_t tags[] = {0, 1, 1, 3, 4, 5};
    return (MuseumCaseValue){tags[kind], kind < 2    ? 0u
                                         : kind == 2 ? 1u
                                         : kind == 3 ? bits
                                         : kind == 4 ? 2u
                                                     : table};
}

static inline MuseumCaseValue museum_definition(const uint32_t input[16],
                                                uint32_t role) {
    if (role == 2)
        return (MuseumCaseValue){4, input[9] == 0 ? 2u : 1u};
    return input[9] < 2    ? (MuseumCaseValue){4, input[9] + 1u}
           : input[9] == 2 ? (MuseumCaseValue){0, 0}
                           : (MuseumCaseValue){1, 0};
}

static inline MuseumCaseValue museum_specialty(const uint32_t input[16],
                                               uint32_t role) {
    if (role == 4)
        return (MuseumCaseValue){4, input[10] == 1 ? 2u : 3u};
    const MuseumCaseValue values[] = {{0, 0}, {4, 3},        {4, 2},
                                      {1, 0}, {3, input[8]}, {5, 8}};
    return values[input[10]];
}

static inline MuseumCaseValue museum_launch(const uint32_t input[16], uint32_t role) {
    const MuseumCaseValue values[] = {
        {5, role == 4 ? 7u : 6u}, {0, 0}, {1, 0}, {3, input[8]}, {4, 2}};
    return values[input[11]];
}

static inline MuseumCaseValue museum_launch_x(const uint32_t input[16], uint32_t role) {
    const MuseumCaseValue values[] = {
        {0, 0},
        {1, 0},
        {3, input[12] ^ (role == 7 ? UINT32_C(0x80000000) : 0u)},
        {4, 2},
        {5, 8}};
    return values[input[13]];
}

#endif
