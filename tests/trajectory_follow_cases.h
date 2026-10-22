#ifndef AB_TEST_TRAJECTORY_FOLLOW_CASES_H
#define AB_TEST_TRAJECTORY_FOLLOW_CASES_H

#include <stdint.h>

enum { TRAJECTORY_SCENES = 256, TRAJECTORY_STEPS = 32, TRAJECTORY_CAPACITY = 8 };

static inline uint32_t trajectory_word(float value) {
    union {
        float number;
        uint32_t bits;
    } result = {.number = value};
    return result.bits;
}

static inline float trajectory_number(uint32_t value) {
    union {
        float number;
        uint32_t bits;
    } result = {.bits = value};
    return result.number;
}

static inline void trajectory_command(uint32_t scene, uint32_t step, uint32_t count,
                                      uint32_t output[12]) {
    uint32_t state = UINT32_C(0x5207a11) ^ (scene * UINT32_C(0x9e3779b9)) ^ step;
    for (uint32_t index = 0; index < 12; ++index) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        output[index] = state;
    }
    output[0] %= 4;
    if (output[0] == 3)
        output[0] = 2;
    output[1] = output[1] % 3 + 1;
    output[2] = (scene + step) % 4;
    output[3] %= 3;
    for (uint32_t index = 4; index < 10; ++index) {
        float value = (float)((int32_t)(output[index] % 4097) - 2048) * 0.0625f;
        output[index] = trajectory_word(value);
    }
    output[10] = output[10] % 7 != 0;
    output[11] = step;
    if (step < 6) {
        const uint32_t operations[] = {0, 0, 0, 1, 1, 2};
        const uint32_t objects[] = {1, 2, 1, 1, 3, 1};
        output[0] = operations[step];
        output[1] = objects[step];
        output[2] = scene % 4;
        output[10] = 1;
    }
    if (scene < 32 && step >= 5 && step <= 12) {
        output[0] = 2;
        output[10] = 1;
        for (uint32_t index = 4; index < 10; ++index)
            output[index] = 0;
        const uint32_t world_x[] = {0,
                                    UINT32_C(0x41fcfb71),
                                    UINT32_C(0x41fcfb72),
                                    UINT32_C(0x41fcfb73),
                                    UINT32_C(0x41fcfb73),
                                    0,
                                    UINT32_C(0x80000000),
                                    UINT32_C(0xc1fcfb72)};
        const float physics_x[] = {0, 1.5f, 1.55f, 1.6f, 1.6f, 0, -0.0f, -1.6f};
        output[4] =
            (scene & 1u) ? world_x[step - 5] : trajectory_word(physics_x[step - 5]);
    }
    if (count == TRAJECTORY_CAPACITY && !output[0])
        output[0] = 1;
}

#endif
