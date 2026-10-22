#include "ab/object_body.h"

#include <stdint.h>
#include <string.h>

static uint32_t positive_remainder(uint32_t magnitude) {
    if (magnitude < UINT32_C(0x40c90fdb)) {
        return magnitude;
    }
    /* Both operands are normal here. Reduce integer significands, introducing
     * one exponent bit at a time. The remainder stays below twice the divisor;
     * no floating-point division or platform remainder implementation is used.
     */
    uint32_t exponent_difference = (magnitude >> 23) - 129;
    uint32_t remainder = (magnitude & UINT32_C(0x7fffff)) | UINT32_C(0x800000);
    for (uint32_t bit = 0;; ++bit) {
        if (remainder >= UINT32_C(0xc90fdb)) {
            remainder -= UINT32_C(0xc90fdb);
        }
        if (bit == exponent_difference) {
            break;
        }
        remainder <<= 1;
    }
    if (!remainder) {
        return 0;
    }
    uint32_t exponent = 129;
    while (remainder < UINT32_C(0x800000)) {
        remainder <<= 1;
        --exponent;
    }
    return (exponent << 23) | (remainder & UINT32_C(0x7fffff));
}

bool ab_game_angle_wrap(float angle, float *output) {
    uint32_t bits;
    memcpy(&bits, &angle, sizeof(bits));
    uint32_t magnitude = bits & UINT32_C(0x7fffffff);
    if (!output || magnitude >= UINT32_C(0x7f800000)) {
        return false;
    }
    uint32_t remainder = positive_remainder(magnitude) | (bits & UINT32_C(0x80000000));
    float result;
    memcpy(&result, &remainder, sizeof(result));
    if (result < 0) {
        result = result + 0x1.921fb6p+2f;
    }
    *output = result;
    return true;
}
