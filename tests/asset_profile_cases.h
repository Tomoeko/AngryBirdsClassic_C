#ifndef AB_TEST_ASSET_PROFILE_CASES_H
#define AB_TEST_ASSET_PROFILE_CASES_H

#include <stdint.h>

static const char *const asset_profile_groups[] = {"INGAME",
                                                   "INGAME_PIGS_NEW",
                                                   "BLOCKS_BIRDDAY",
                                                   "BLOCKS_GREEN",
                                                   "INGAME_EPISODE_RED_BIRD",
                                                   "MATTEL_INGAME",
                                                   "MENU",
                                                   "BUTTONS",
                                                   "SPLASHES",
                                                   "CUTSCENES",
                                                   "CUTSCENE_BLACKBIRD",
                                                   "EGGDEFENDER_CUTSCENES",
                                                   "THEME_1",
                                                   "THEME",
                                                   "THEMEX",
                                                   "THEM",
                                                   "theme_1",
                                                   "MENU_CHINA",
                                                   "_CHINA",
                                                   "A_CHINA",
                                                   "_CHINAX",
                                                   "CHINA",
                                                   "MENU_SHOP",
                                                   "MENU_UPGRADES",
                                                   "INGAME_BIRDS",
                                                   "INGAME_BLOCKS",
                                                   "THEME_BEACH",
                                                   "INGAME_HUD",
                                                   "MENU_LONG_CHINA",
                                                   "THEME_CHINA",
                                                   "UNKNOWN",
                                                   ""};
static const char *const asset_profile_splashes[] = {"selected_splash_a",
                                                     "selected_splash_b"};

static void asset_profile_case(uint32_t sample, uint32_t input[5]) {
    static const uint32_t widths[] = {
        UINT32_C(0xbf800000), UINT32_C(0x80000000), 0,
        UINT32_C(0x3f800000), UINT32_C(0x43efffff), UINT32_C(0x43f00000),
        UINT32_C(0x43f00001), UINT32_C(0x44200000), UINT32_C(0x447fffff),
        UINT32_C(0x44800000), UINT32_C(0x44800001), UINT32_C(0x44f00000),
        UINT32_C(0x45800000), UINT32_C(0x7f7fffff), 1,
        UINT32_C(0x80000001)};
    input[0] = sample;
    input[1] = widths[(sample >> 5) & 15u];
    input[2] = (sample >> 9) & 1u;
    input[3] = (sample >> 10) & 1u;
    input[4] = sample & 31u;
}

#endif
