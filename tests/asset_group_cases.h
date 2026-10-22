#ifndef AB_TEST_ASSET_GROUP_CASES_H
#define AB_TEST_ASSET_GROUP_CASES_H

#include <stdint.h>

static const char *const asset_group_names[] = {"alpha", "beta", "gamma", "",
                                                "missing"};
static const char *const asset_group_profiles[] = {"folder", "wide_folder"};
static const char *const asset_group_files[] = {"sheet.dat", "composite.dat", "",
                                                "sub/sheet.dat"};

static void asset_group_case(uint32_t sample, uint32_t input[7], uint32_t loaded[6],
                             uint32_t kinds[4]) {
    input[0] = sample;
    input[1] = (sample >> 3) & 1u;
    input[2] = sample % 5;
    input[3] = (sample >> 4) & 1u;
    input[4] = (sample >> 5) % 7;
    input[5] = (sample >> 8) % 5;
    input[6] = (sample >> 11) & 1u;
    for (uint32_t index = 0; index < 6; ++index) {
        loaded[index] = sample & 128u && index < 2
                            ? input[2]
                            : ((sample >> (index % 4)) + index) % 5;
    }
    for (uint32_t index = 0; index < 4; ++index) {
        kinds[index] = ((sample >> (index + 4)) + index) % 4;
    }
}

#endif
