#ifndef AB_TEST_THEME_ASSET_CASES_H
#define AB_TEST_THEME_ASSET_CASES_H

#include <stdint.h>

static const char *const theme_asset_names[] = {
    "alpha", "beta", "gamma", "delta", "", "INGAME_EPISODE_RED_BIRD"};

static uint32_t theme_asset_random(uint32_t *state) {
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state >> 16;
}

/* Target string/table, editor value kind, target/current alias, old/new lengths,
 * callback failure index (zero disables), six old and six candidate name IDs.
 * Editor kinds are false, true, numeric zero, empty string and nil. */
static void theme_asset_case(uint32_t sample, uint32_t input[18]) {
    uint32_t state = UINT32_C(0x5207a11e) ^ sample;
    input[0] = sample % 3 == 0;
    input[1] = sample % 5;
    input[2] = (sample / 3) % 5 == 0;
    input[3] = theme_asset_random(&state) % 7;
    input[4] = input[0] ? 1 : theme_asset_random(&state) % 7;
    input[5] = sample % 7 ? 0 : 1 + theme_asset_random(&state) % 13;
    for (uint32_t index = 0; index < 6; ++index) {
        input[6 + index] = theme_asset_random(&state) % 6;
        input[12 + index] = theme_asset_random(&state) % 6;
    }
    if (sample % 8 == 0) {
        input[3] = 6;
        input[4] = input[0] ? 1 : 6;
        input[6] = input[7] = input[8] = 0;
        input[12] = input[13] = input[14] = 1;
    }
}

#endif
