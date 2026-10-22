#ifndef AB_ORACLE_MATRIX_INVERSE_H
#define AB_ORACLE_MATRIX_INVERSE_H

#include "../../tests/matrix_inverse_cases.h"

static void probe_matrix_inverse(void) {
    typedef void (*Inverse)(const void *, void *);
    Inverse functions[] = {
        (Inverse)find_function("_ZNK7b2Mat3312GetInverse22EPS_"),
        (Inverse)find_function("_ZNK7b2Mat3315GetSymInverse33EPS_"),
    };
    semihost(4, "AB_ANDROID_MATRIX_INVERSE_V1 4096 2\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[6];
        matrix_inverse_case(sample, input);
        uint32_t matrix[] = {input[0], input[1], input[2], input[1], input[3],
                             input[4], input[2], input[4], input[5]};
        uint32_t before[9];
        for (size_t index = 0; index < 9; ++index) {
            before[index] = matrix[index];
        }
        print_words(&sample, 1);
        print_words(input, 6);
        for (size_t kind = 0; kind < 2; ++kind) {
            uint32_t guarded[11];
            for (size_t index = 0; index < 11; ++index) {
                guarded[index] = UINT32_C(0x4f123456);
            }
            functions[kind](matrix, guarded + 1);
            if (guarded[0] != UINT32_C(0x4f123456) ||
                guarded[10] != UINT32_C(0x4f123456)) {
                fail("Original matrix inverse exceeded output bounds\n");
            }
            for (size_t index = 0; index < 9; ++index) {
                if (matrix[index] != before[index]) {
                    fail("Original matrix inverse modified its input\n");
                }
            }
            print_words(guarded + 1, 9);
        }
    }
    semihost(4, "AB matrix inverse reference complete\n");
}

#endif
