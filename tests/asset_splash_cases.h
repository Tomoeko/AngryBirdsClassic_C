#ifndef AB_TEST_ASSET_SPLASH_CASES_H
#define AB_TEST_ASSET_SPLASH_CASES_H

#include <stdint.h>

static const char *const splash_profiles[] = {"1024x768_splash", "1440x960_splash",
                                              "1024x600_splash", "1024x550_splash"};

static void asset_splash_case(uint32_t index, uint32_t input[2]) {
    static const uint32_t boundaries[] = {0x3faa3d71, 0x3fc00000, 0x3fe66666};
    static const uint32_t sizes[] = {0,          0x80000000, 1,          0x80000001,
                                     0x007fffff, 0x00800000, 0x3f800000, 0x43a00000,
                                     0x43f00000, 0x44400000, 0x44800000, 0x44b40000,
                                     0x44700000, 0x7f7fffff, 0xff7fffff, 0xbf800000};
    if (index < 18) {
        input[0] = boundaries[(index / 3) % 3] + index % 3 - 1;
        input[1] = 0x3f800000;
        if (index >= 9) {
            input[0] ^= 0x80000000;
            input[1] ^= 0x80000000;
        }
    } else if (index < 274) {
        input[0] = sizes[(index - 18) % 16];
        input[1] = sizes[(index - 18) / 16];
    } else {
        uint32_t state = UINT32_C(0x5205f1a5) ^ index;
        for (uint32_t field = 0; field < 2; ++field) {
            state = state * UINT32_C(1664525) + UINT32_C(1013904223);
            input[field] = state;
            if ((state & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000)) {
                input[field] ^= UINT32_C(0x00800000);
            }
        }
    }
}

#endif
