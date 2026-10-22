#ifndef AB_TEST_GAME_OBJECT_REMOVE_CASES_H
#define AB_TEST_GAME_OBJECT_REMOVE_CASES_H

#include <stdint.h>

static void game_object_remove_case(uint32_t sample, uint32_t output[11]) {
    static const uint32_t numbers[] = {0,          0x80000000, 0x41880000, 0x7f800000,
                                       0x7fc12345, 0xffc12345, 0x3e800000, 0xbf800000};
    output[0] = sample;
    output[1] = (sample / 4u) % 12u;
    output[2] = sample % 3u;
    output[3] = (sample / 3u) % 4u;
    output[4] = 0;
    for (uint32_t slot = 0; slot < 3; ++slot) {
        output[4] |= ((sample / (slot + 1u) + slot) % 2u + 1u) << (slot * 2u);
    }
    output[5] = (sample / 5u) % 4u;
    output[6] = (sample / 7u) % 4u;
    output[7] = (sample / 8u) % 2u;
    output[8] = numbers[sample % 8u];
    output[9] = numbers[(sample / 3u) % 8u];
    output[10] = numbers[(sample / 11u) % 8u];
    if (sample < 32) {
        output[1] = sample % 2u ? 3u : 1u;
        output[2] = (sample / 2u) % 3u;
        output[3] = 3;
        output[4] =
            sample % 2u ? 2u | (1u << 2u) | (1u << 4u) : 1u | (2u << 2u) | (1u << 4u);
    }
}

#endif
