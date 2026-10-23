#ifndef AB_MASK_DRAW_CASES_H
#define AB_MASK_DRAW_CASES_H

#include <stdint.h>

enum { AB_MASK_DRAW_CASES = 2048, AB_MASK_DRAW_INPUT_WORDS = 24 };

static uint32_t mask_draw_float(float value) {
    union {
        float value;
        uint32_t bits;
    } converted = {.value = value};
    return converted.bits;
}

static void mask_draw_case(uint32_t sample, uint32_t output[AB_MASK_DRAW_INPUT_WORDS]) {
    static const uint32_t widths[] = {480, 1024, 1920, 2560};
    static const uint32_t heights[] = {320, 768, 1080, 1440};
    output[0] = widths[sample % 4];
    output[1] = heights[(sample / 4) % 4];
    output[2] = 256 + 64 * (sample % 5);
    output[3] = 128 + 32 * (sample % 7);
    output[4] = 512;
    output[5] = 512;
    output[6] = sample % 97;
    output[7] = sample % 79;
    output[8] = 20 + sample % 193;
    output[9] = 20 + sample % 196;
    output[10] = mask_draw_float((float)((int32_t)(sample % 83) - 41));
    output[11] = mask_draw_float((float)((int32_t)(sample % 71) - 35));
    output[12] = mask_draw_float((float)((int32_t)(sample % 401) - 120));
    output[13] = mask_draw_float((float)((int32_t)(sample % 307) - 90));
    output[14] = mask_draw_float((float)((int32_t)(sample % 89) - 44) / 4);
    output[15] = mask_draw_float((float)((int32_t)(sample % 67) - 33) / 4);
    output[16] = mask_draw_float((float)(sample % 17 + 1) / 4);
    output[17] = mask_draw_float((float)(sample % 13 + 1) / 4);
    static const float cosines[] = {1, 0, -1, 0.8f};
    static const float sines[] = {0, 1, 0, -0.6f};
    output[18] = mask_draw_float(cosines[sample % 4]);
    output[19] = mask_draw_float(sines[sample % 4]);
    output[20] = 0;
    output[21] = mask_draw_float((float)(sample % 11) / 4);
    output[22] = mask_draw_float((float)(sample % 9) / 4);
    output[23] = mask_draw_float((float)(sample % 5 + 1) / 5);
    if (sample == 0) {
        output[6] = output[7] = 1;
        output[8] = 193;
        output[9] = 196;
        output[10] = mask_draw_float(97);
        output[11] = mask_draw_float(98);
        output[12] = mask_draw_float(240);
        output[13] = mask_draw_float(160);
        output[14] = output[15] = output[19] = output[20] = output[21] = output[22] = 0;
        output[16] = output[17] = output[23] = mask_draw_float(1);
        output[18] = mask_draw_float(1);
    }
}

#endif
