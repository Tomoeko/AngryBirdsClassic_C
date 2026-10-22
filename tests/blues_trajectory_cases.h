#ifndef AB_TEST_BLUES_TRAJECTORY_CASES_H
#define AB_TEST_BLUES_TRAJECTORY_CASES_H

#include <stdint.h>

enum { BLUES_CASES = 512, BLUES_INPUT_WORDS = 14, BLUES_EVENT_WORDS = 27 };

typedef struct {
    uint32_t tag, value;
} BluesValue;

typedef struct {
    BluesValue fields[5], flying, others;
    uint32_t get_calls, mutated;
} BluesEnvironment;

static inline uint32_t blues_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state ^ (*state >> 13);
}

static inline void blues_case(uint32_t sample, uint32_t input[BLUES_INPUT_WORDS]) {
    uint32_t state = sample ^ UINT32_C(0x520b1e55);
    input[0] = sample;
    input[1] = sample % 5;
    for (uint32_t field = 2; field < 5; ++field)
        input[field] = blues_random(&state) % 6;
    input[5] = blues_random(&state) % 3;
    input[6] = blues_random(&state) % 3;
    input[7] = blues_random(&state) % 6;
    input[8] = blues_random(&state) % 5;
    input[9] = blues_random(&state) % 3;
    input[10] = blues_random(&state) % 4;
    input[11] = blues_random(&state) % 64;
    input[12] = blues_random(&state) % 5;
    input[13] = blues_random(&state) % 4 == 0 ? 2u : 0u;
    if (sample < 160) {
        input[2] = (sample / 5) % 2 ? 2u : 1u;
        input[3] = 2;
        input[4] = 1;
        input[5] = 1;
        input[6] = 2;
        input[7] = 1;
        input[8] = 3;
        input[9] = 1;
        input[10] = (sample / 40) % 4;
        input[11] = sample / 10;
        input[12] = sample < 80 ? 0u : (sample / 10) % 5;
        input[13] = sample < 80 ? 0u : 2u;
    }
}

static inline BluesValue blues_predicate(uint32_t kind) {
    const BluesValue values[] = {{0, 0}, {1, 0}, {1, 1}, {3, 0}, {4, 0}, {5, 8}};
    return values[kind];
}

static inline BluesValue blues_physical(uint32_t role) {
    return (BluesValue){role ? 5u : 0u, role};
}

static inline void blues_environment(const uint32_t input[BLUES_INPUT_WORDS],
                                     BluesEnvironment *environment) {
    *environment = (BluesEnvironment){0};
    for (uint32_t field = 0; field < 3; ++field)
        environment->fields[field] = blues_predicate(input[field + 2]);
    environment->fields[3] = blues_physical(input[5]);
    environment->fields[4] = blues_physical(input[6]);
    environment->flying = blues_physical(input[9]);
    environment->others = (BluesValue){5, 7};
}

/* Controlled service effects only. Original methods independently determine
 * whether/when services execute and which values they freeze between calls. */
static inline void blues_mutate(const uint32_t input[BLUES_INPUT_WORDS],
                                BluesEnvironment *environment) {
    if (environment->mutated)
        return;
    environment->mutated = 1;
    if (input[11] & 1)
        environment->fields[3] = blues_physical(2);
    if (input[11] & 2)
        environment->fields[0] = (BluesValue){1, 0};
    if (input[11] & 4)
        environment->fields[1] = (BluesValue){1, 0};
    if (input[11] & 8)
        environment->fields[4] = (BluesValue){0, 0};
    if (input[11] & 16)
        environment->others = (BluesValue){5, 8};
    if (input[11] & 32)
        environment->flying = blues_physical(2);
}

static inline void blues_snapshot(const BluesEnvironment *environment,
                                  uint32_t output[15]) {
    for (uint32_t field = 0; field < 5; ++field) {
        output[field * 2] = environment->fields[field].tag;
        output[field * 2 + 1] = environment->fields[field].value;
    }
    output[10] = environment->flying.tag;
    output[11] = environment->flying.value;
    output[12] = environment->others.tag;
    output[13] = environment->others.value;
    output[14] = environment->get_calls;
}

#endif
