#ifndef AB_TEST_WORLD_RAYCAST_CASES_H
#define AB_TEST_WORLD_RAYCAST_CASES_H

#include <stdint.h>

enum {
    WORLD_RAY_BODIES = 12,
    WORLD_RAY_BODY_WORDS = 13,
    WORLD_RAY_INPUT_WORDS = 8,
    WORLD_RAY_OUTPUT_WORDS = 16,
    WORLD_RAY_EVENT_WORDS = 8
};

static uint32_t world_ray_word(float value) {
    union {
        float value;
        uint32_t word;
    } converted = {.value = value};
    return converted.word;
}

static inline void world_raycast_body(uint32_t scene, uint32_t index,
                                      uint32_t output[13]) {
    static const uint32_t angles[] = {0, UINT32_C(0x3e800000), UINT32_C(0xbf000000),
                                      UINT32_C(0x3fc90fdb)};
    output[0] = index + 1u;
    output[1] = index % 3u;
    uint32_t geometry = (scene & 8u) && index == 11 ? 10u : index;
    float x = -3.0f + (float)(geometry % 4u) * 2.0f;
    float y = -2.0f + (float)(geometry / 4u) * 2.0f;
    if (scene % 4u == 1) {
        x = 0;
    }
    if (scene % 4u == 2) {
        y = 0;
    }
    if (scene % 4u == 3) {
        x = y = 0;
    }
    output[2] = world_ray_word(x);
    output[3] = world_ray_word(y);
    output[4] = angles[(geometry + scene) % 4u];
    output[5] = !(scene & 4u) || index % 5u != 0;
    output[6] = (geometry + scene) & 1u ? 2u : 0u;
    output[7] = output[6] ? UINT32_C(0x3c23d70a)
                          : world_ray_word(0.25f + (float)(geometry % 3u) * 0.125f);
    output[8] = world_ray_word(output[6] ? 0.5f + (float)(geometry % 3u) * 0.25f
                                         : (float)(geometry % 3u) * 0.125f);
    output[9] = world_ray_word(output[6] ? 0.375f + (float)(geometry % 4u) * 0.125f
                                         : (float)(geometry % 2u) * -0.125f);
    output[10] = (index + scene) % 3u == 0;
    output[11] = scene & 8u ? 0u : 1u << (index % 4u);
    output[12] = index % 3u ? 65535u : 0u;
}

static inline void world_raycast_case(uint32_t scene, uint32_t index,
                                      uint32_t output[8]) {
    static const uint32_t fractions[] = {0, UINT32_C(0x3e800000), UINT32_C(0x3f000000),
                                         UINT32_C(0x3f800000), UINT32_C(0x40000000)};
    output[0] = scene;
    output[1] = index;
    float offset = ((float)(index % 13u) - 6.0f) * 0.5f;
    float first_x = -10, first_y = offset, second_x = 10, second_y = offset;
    switch (index % 8u) {
        case 1:
            first_x = 10;
            second_x = -10;
            break;
        case 2:
            first_x = second_x = offset;
            first_y = -10;
            second_y = 10;
            break;
        case 3:
            first_x = second_x = offset;
            first_y = 10;
            second_y = -10;
            break;
        case 4:
            first_y = -8;
            second_y = 8;
            break;
        case 5:
            first_y = 8;
            second_y = -8;
            break;
        case 6:
            first_x = second_x = 0;
            first_y = second_y = 0;
            break;
        case 7:
            first_x = -0.5f;
            first_y = 0;
            second_x = 0.5f;
            second_y = 0;
            break;
    }
    if (index == 63) {
        first_x = -3.25f;
        first_y = second_y = -2;
        second_x = -2;
    }
    output[2] = world_ray_word(first_x);
    output[3] = world_ray_word(first_y);
    output[4] = world_ray_word(second_x);
    output[5] = world_ray_word(second_y);
    output[6] = fractions[(scene + index / 8u) % 5u];
    output[7] = (scene + index) % 5u;
}

static inline uint32_t world_raycast_return(uint32_t mode, uint32_t fixture,
                                            uint32_t fraction) {
    if (mode == 1) {
        return UINT32_C(0x3f800000);
    }
    if (mode == 2 && fixture % 2u) {
        return UINT32_C(0xbf800000);
    }
    if (mode == 3) {
        return 0;
    }
    if (mode == 4 && fixture % 2u) {
        return UINT32_C(0x7fc12345);
    }
    return fraction;
}

#endif
