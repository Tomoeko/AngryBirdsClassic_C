#include "vp8_transform.h"
#include "vp8_math.h"

#include <stddef.h>

void ab_vp8_walsh_inverse(const int16_t input[16], int16_t output[16]) {
    static const int8_t basis[4][4] = {
        {1, 1, 1, 1}, {1, 1, -1, -1}, {1, -1, -1, 1}, {1, -1, 1, -1}};
    int32_t vertical[16];
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            int32_t sum = 0;
            for (size_t sample = 0; sample < 4; ++sample) {
                sum += basis[row][sample] * (int32_t)input[sample * 4 + column];
            }
            vertical[row * 4 + column] = sum;
        }
    }
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            int32_t sum = 3;
            for (size_t sample = 0; sample < 4; ++sample) {
                sum += basis[column][sample] * vertical[row * 4 + sample];
            }
            output[row * 4 + column] = ab_vp8_narrow(ab_vp8_floor_shift(sum, 3));
        }
    }
}

static int32_t cosine_product(int32_t value, int32_t constant) {
    int64_t product = (int64_t)value * constant;
    return (int32_t)(product / 65536 - (product % 65536 < 0));
}

static void inverse_cosine_axis(const int32_t input[4], int32_t output[4]) {
    int32_t even_sum = input[0] + input[2];
    int32_t even_difference = input[0] - input[2];
    int32_t odd_sine =
        cosine_product(input[1], 35468) - input[3] - cosine_product(input[3], 20091);
    int32_t odd_cosine =
        input[1] + cosine_product(input[1], 20091) + cosine_product(input[3], 35468);
    output[0] = even_sum + odd_cosine;
    output[1] = even_difference + odd_sine;
    output[2] = even_difference - odd_sine;
    output[3] = even_sum - odd_cosine;
}

void ab_vp8_dct_add(const int16_t input[16], uint8_t *pixels, size_t stride) {
    int32_t vertical[16];
    for (size_t column = 0; column < 4; ++column) {
        int32_t samples[4], result[4];
        for (size_t row = 0; row < 4; ++row) {
            samples[row] = input[row * 4 + column];
        }
        inverse_cosine_axis(samples, result);
        for (size_t row = 0; row < 4; ++row) {
            vertical[row * 4 + column] = result[row];
        }
    }
    for (size_t row = 0; row < 4; ++row) {
        int32_t residual[4];
        inverse_cosine_axis(vertical + row * 4, residual);
        for (size_t column = 0; column < 4; ++column) {
            int32_t value = pixels[row * stride + column] +
                            ab_vp8_floor_shift(residual[column] + 4, 3);
            pixels[row * stride + column] = (uint8_t)(value < 0     ? 0
                                                      : value > 255 ? 255
                                                                    : value);
        }
    }
}
