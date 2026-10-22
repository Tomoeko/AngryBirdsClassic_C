#ifndef AB_ORACLE_MATRIX_SOLVE_H
#define AB_ORACLE_MATRIX_SOLVE_H

#include "../../tests/matrix_solve_cases.h"

static void probe_matrix_solve(void) {
    typedef void (*Solve)(void *, const void *, const void *);
    Solve functions[] = {
        (Solve)find_function("_ZNK7b2Mat337Solve22ERK6b2Vec2"),
        (Solve)find_function("_ZNK7b2Mat337Solve33ERK6b2Vec3"),
    };
    semihost(4, "AB_ANDROID_MATRIX_SOLVE_V1 4096 2\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[9];
        matrix_solve_case(sample, input);
        uint32_t matrix[] = {input[0], input[1], input[2], input[1], input[3],
                             input[4], input[2], input[4], input[5]};
        uint32_t before[9];
        for (size_t index = 0; index < 9; ++index) {
            before[index] = matrix[index];
        }
        uint32_t right[] = {input[6], input[7], input[8]};
        print_words(&sample, 1);
        print_words(input, 9);
        for (size_t kind = 0; kind < 2; ++kind) {
            uint32_t guarded[5];
            for (size_t index = 0; index < 5; ++index) {
                guarded[index] = UINT32_C(0x4f123456);
            }
            functions[kind](guarded + 1, matrix, right);
            size_t count = kind == 0 ? 2 : 3;
            if (guarded[0] != UINT32_C(0x4f123456) ||
                guarded[count + 1] != UINT32_C(0x4f123456)) {
                fail("Original matrix solve exceeded output bounds\n");
            }
            for (size_t index = 0; index < 9; ++index) {
                if (matrix[index] != before[index]) {
                    fail("Original matrix solve modified its input\n");
                }
            }
            for (size_t index = 0; index < 3; ++index) {
                if (right[index] != input[6 + index]) {
                    fail("Original matrix solve modified its right-hand side\n");
                }
            }
            print_words(guarded + 1, count);
        }
    }
    semihost(4, "AB matrix solve reference complete\n");
}

#endif
