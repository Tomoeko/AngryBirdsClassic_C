#ifndef AB_TEST_THEME_RECT_CASES_H
#define AB_TEST_THEME_RECT_CASES_H

#include <stdint.h>

static void theme_rect_case(uint32_t sample, uint32_t input[18]) {
    input[0] = 321 + sample * 31 % 2016;
    input[1] = 241 + sample * 67 % 1248;
    for (uint32_t field = 0; field < 8; ++field) {
        input[2 + field * 2] = (sample + field * 3) % 5;
        union {
            float number;
            uint32_t bits;
        } value;
        int32_t raw = (int32_t)((sample * 13 + field * 29) % 4097) - 1024;
        value.number = (float)raw / (field < 4 ? 8 : 512);
        input[3 + field * 2] = value.bits;
    }
    if (sample % 17 == 0) {
        input[8] = 3;
        input[9] = sample & 1 ? UINT32_C(0x80000000) : 0;
    }
}

#endif
