#ifndef AB_TEST_LUA_ASSET_SPRITE_CASES_H
#define AB_TEST_LUA_ASSET_SPRITE_CASES_H

#include <stdint.h>

enum { LUA_ASSET_SPRITE_CASES = 4096 };

static inline uint32_t asset_sprite_word(float value) {
    union {
        float number;
        uint32_t bits;
    } result = {.number = value};
    return result.bits;
}

static inline float asset_sprite_number(uint32_t value) {
    union {
        float number;
        uint32_t bits;
    } result = {.bits = value};
    return result.number;
}

static inline void asset_sprite_case(uint32_t sample, uint32_t input[16]) {
    uint32_t state = UINT32_C(0x520a55e7) ^ sample * UINT32_C(0x9e3779b9);
    for (uint32_t index = 0; index < 16; ++index) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        input[index] = state;
    }
    input[0] = (input[0] % 1024 + 1) | ((input[1] % 1024 + 1) << 16);
    for (uint32_t index = 1; index < 12; ++index) {
        float value = (float)((int32_t)(input[index] % 65537) - 32768) / 32;
        input[index] = asset_sprite_word(value);
    }
    input[1] = asset_sprite_word((float)((int32_t)(sample % 2049) - 1024));
    input[2] = asset_sprite_word((float)((int32_t)((sample * 17) % 2049) - 1024));
    const float scales[] = {1, -1, 2, 0.5f, 0.33333334f, -0.25f};
    input[5] = asset_sprite_word(scales[sample % 6]);
    input[6] = asset_sprite_word(scales[(sample / 6) % 6]);
    const float angles[] = {0, -0.0f, 0.01f, -0.01f, 1.57079637f, -3.14159274f};
    if (sample % 3)
        input[7] = asset_sprite_word(angles[sample % 6]);
    input[10] = asset_sprite_word((float)(sample % 33 + 1) / 16);
    input[11] = asset_sprite_word((float)((sample / 33) % 33 + 1) / 16);
    input[12] = sample % 7 != 0;
    input[13] = sample % 11 != 0;
    input[14] %= 2;
    input[15] = asset_sprite_word((float)((int32_t)(input[15] % 97) - 32) / 32);
    if (!input[12] || !input[13])
        input[3] = UINT32_C(0x7fc12345);
}

#endif
