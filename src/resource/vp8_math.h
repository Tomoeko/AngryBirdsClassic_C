#ifndef AB_VP8_MATH_H
#define AB_VP8_MATH_H

#include <stdint.h>

/* Fixed codec shifts, 0..16, with ARM arithmetic-shift rounding. */
static inline int32_t ab_vp8_floor_shift(int32_t value, unsigned shift) {
    int32_t divisor = INT32_C(1) << shift;
    return value / divisor - (value % divisor < 0);
}

static inline int16_t ab_vp8_narrow(int32_t value) {
    uint32_t low = (uint32_t)value & UINT32_C(65535);
    return (int16_t)(low < 32768 ? (int32_t)low : (int32_t)low - 65536);
}

#endif
