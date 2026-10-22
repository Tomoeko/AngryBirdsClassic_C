#ifndef AB_TEST_GAME_RAYCAST_CASES_H
#define AB_TEST_GAME_RAYCAST_CASES_H

#include "world_raycast_cases.h"

enum { GAME_RAY_OUTPUT_WORDS = 12 };

typedef struct {
    const char *data;
    uint32_t size;
} GameRayName;

static GameRayName game_raycast_name(uint32_t index) {
    static const GameRayName names[WORLD_RAY_BODIES] = {
        {"", 0},        {"ray\0object", 10}, {"\xc3\xa9\xe9\xb3\xa5", 5},
        {"wood", 4},    {"stone", 5},        {"pig", 3},
        {"bird", 4},    {"ground", 6},       {"sensor", 6},
        {"overlap", 7}, {"sameA", 5},        {"sameB", 5}};
    return names[index];
}

static inline const char *game_raycast_key(uint32_t index) {
    static const char *const keys[WORLD_RAY_BODIES] = {
        "ray00", "ray01", "ray02", "ray03", "ray04", "ray05",
        "ray06", "ray07", "ray08", "ray09", "ray10", "ray11"};
    return keys[index];
}

static void game_raycast_case(uint32_t scene, uint32_t index, uint32_t output[8]) {
    world_raycast_case(scene, index, output);
#if defined(AB_TEST_GAME_RAYCAST_NUMERIC) || defined(AB_ORACLE_GAME_RAYCAST_NUMERIC)
    static const uint32_t values[] = {UINT32_C(0x7fc12345),
                                      UINT32_C(0xffc23456),
                                      UINT32_C(0x7f812345),
                                      UINT32_C(0xff823456),
                                      UINT32_C(0x7f800000),
                                      UINT32_C(0xff800000),
                                      UINT32_C(0x7f7fffff),
                                      UINT32_C(0xff7fffff),
                                      UINT32_C(0x00800000),
                                      UINT32_C(0x80800000),
                                      UINT32_C(0x00000001),
                                      UINT32_C(0x80000001),
                                      0,
                                      UINT32_C(0x80000000),
                                      UINT32_C(0x41200000),
                                      UINT32_C(0xc1200000)};
    uint32_t value = values[index % 16u];
    if (index < 16) {
        for (uint32_t axis = 2; axis < 6; ++axis)
            output[axis] = value;
    } else if (index < 32) {
        output[2 + scene % 4u] = value;
    } else if (index < 48) {
        output[4] = value;
        output[5] = values[(index + scene) % 16u];
    } else {
        output[2] = value;
        output[3] = values[(index + scene) % 16u];
    }
#endif
}

static void game_raycast_pack(const char *data, uint32_t length, uint32_t output[4]) {
    for (uint32_t index = 0; index < 4; ++index) {
        output[index] = 0;
    }
    for (uint32_t index = 0; index < length; ++index) {
        output[index / 4u] |= (uint32_t)(uint8_t)data[index] << (index % 4u * 8u);
    }
}

#endif
