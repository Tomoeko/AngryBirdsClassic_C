#ifndef AB_LEVEL_CLOCK_CASES_H
#define AB_LEVEL_CLOCK_CASES_H

#include <stdint.h>

enum { AB_LEVEL_CLOCK_CASES = 4096 };

static const char *const level_clock_names[] = {
    "StaticBlockTheme17_06_1", "WoodBlock1_1", "WoodBlock1_2", "LightBlock1_1"};

typedef struct {
    float minutes, seconds, positions[8];
    uint32_t absent;
} LevelClockCase;

static LevelClockCase level_clock_case(uint32_t sample) {
    LevelClockCase value = {.minutes = (float)((sample % 3600) / 60),
                            .seconds = (float)(sample % 60),
                            .absent = sample < 3600 ? 4 : sample % 4};
    uint32_t random = sample + UINT32_C(0x49f376b1);
    for (uint32_t index = 0; index < 8; ++index) {
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        int32_t numerator = (int32_t)(random & UINT32_C(0xffffff)) - INT32_C(0x800000);
        value.positions[index] = (float)numerator / 256.0f;
    }
    return value;
}

#endif
