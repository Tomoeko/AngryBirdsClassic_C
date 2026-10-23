#ifndef AB_LEVEL_EGG_CASES_H
#define AB_LEVEL_EGG_CASES_H

#include <stdbool.h>
#include <stdint.h>

enum {
    AB_LEVEL_EGG_SCRIPTS = 18,
    AB_LEVEL_EGG_CASES = 1152,
    AB_LEVEL_EGG_MAXIMUM = 32
};

static const char *const level_egg_names[AB_LEVEL_EGG_SCRIPTS][3] = {
    {"Level34", "ExtraBeachBall_1", "LevelGE_2"},
    {"LevelP2_64", "ExtraGoldenEgg_1", "LevelGE_5"},
    {"LevelP2_67", "ExtraGoldenEgg_1", "LevelGE_3"},
    {"LevelP3_220", "StaticBalloon03_2", "LevelGE_6"},
    {"LevelP3_313", "ExtraGoldenEgg_1", "LevelGE_8"},
    {"LevelP4_440", "ExtraGoldenEgg_1", "LevelGE_9"},
    {"LevelP4_444", "ExtraRubberDuck_1", "LevelGE_10"},
    {"LevelP4_478", "ExtraGoldenEgg_1", "LevelGE_11"},
    {"LevelP5_357", "ExtraHolyGrail_4", "LevelGE_16"},
    {"LevelP5_650", "ExtraGoldenEgg_1", "LevelGE_17"},
    {"LevelP5_354", "ExtraSuperBowl_2", "LevelGE_19"},
    {"LevelP5_364", "ExtraGoldenEgg_1", "LevelGE_18"},
    {"Level695", "ExtraGoldenEgg_1", "LevelGE_20"},
    {"Level697", "ExtraGoldenEgg_1", "LevelGE_21"},
    {"LevelP6_7", "ExtraTreasureChest_1", "LevelGE_23"},
    {"LevelH564", "ExtraGoldenEgg_1", "LevelGE_24"},
    {"UGC1Level15_custom", "ExtraGoldenEgg_1", "LevelGE_UGC1"},
    {"Level974", "ExtraGoldenEgg_1", "LevelGE_25"},
};

typedef struct {
    uint32_t script, world, joints, watched, eagle;
    bool full, unlocked, match;
} LevelEggCase;

static LevelEggCase level_egg_case(uint32_t sample) {
    uint32_t flags = sample / AB_LEVEL_EGG_SCRIPTS;
    uint32_t count = 1 + sample * 17 % AB_LEVEL_EGG_MAXIMUM;
    return (LevelEggCase){.script = sample % AB_LEVEL_EGG_SCRIPTS,
                          .world = count,
                          .joints = 1 + sample * 7 % AB_LEVEL_EGG_MAXIMUM,
                          .watched = sample / 32 % count,
                          .eagle = (flags >> 3) % 4,
                          .full = (flags & 2) != 0,
                          .unlocked = (flags & 4) != 0,
                          .match = (flags & 1) != 0};
}

static void level_egg_key(uint32_t index, char output[12]) {
    const char prefix[] = "object_";
    for (unsigned byte = 0; byte < 7; ++byte)
        output[byte] = prefix[byte];
    for (unsigned byte = 0; byte < 4; ++byte)
        output[7 + byte] = (char)('a' + ((index >> (byte * 4)) & 15));
    output[11] = 0;
}

static uint32_t level_egg_capacity(uint32_t count) {
    uint32_t capacity = 1;
    while (capacity < count)
        capacity *= 2;
    return capacity;
}

#endif
