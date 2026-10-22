#ifndef AB_TEST_MATRIX_SOLVE_CASES_H
#define AB_TEST_MATRIX_SOLVE_CASES_H

#include "matrix_inverse_cases.h"

static void matrix_solve_case(uint32_t sample, uint32_t output[9]) {
    matrix_inverse_case(sample, output);
    uint32_t random = UINT32_C(0xb31d9751) ^ sample * UINT32_C(0x9e3779b9);
    for (size_t index = 0; index < 3; ++index) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        int32_t integer = (int32_t)(random & 1023u) - 512;
        float number = (float)integer * 0.125f;
        if (sample < 8) {
            number = (sample >> index) & 1u ? -0.0f : 0.0f;
        }
        union {
            float number;
            uint32_t bits;
        } value = {.number = number};
        output[6 + index] = value.bits;
    }
}

#endif
