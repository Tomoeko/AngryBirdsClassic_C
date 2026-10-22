#include "ab/rotation.h"

#include <stdint.h>
#include <string.h>

static uint32_t script_component_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double script_component_magnitude(uint32_t bits) {
    bits &= UINT32_C(0x7fffffff);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Our series follows atan's addition identity, independently of the reference
 * libm implementation. Reduction limits |r| to sqrt(2)-1. The omitted term
 * after degree 51 is below 1e-22 there; binary64 evaluation is rounded to the
 * script's binary32 result. This is sampled native-result evidence, not an
 * exhaustive correct-rounding proof for every pair of finite components.
 */
static double script_acute_angle(double ratio) {
    double base = 0;
    if (ratio > 0x1.a827999fcef32p-2) {
        ratio = (ratio - 1.0) / (ratio + 1.0);
        base = 0x1.921fb54442d18p-1;
    }
    double squared = ratio * ratio;
    double series = -1.0 / 51.0;
    for (int term = 24; term >= 0; --term) {
        double coefficient = (term & 1 ? -1.0 : 1.0) / (2 * term + 1);
        series = series * squared + coefficient;
    }
    return base + ratio * series;
}

bool ab_script_angle_from_components(float vertical, float horizontal, float *output) {
    uint32_t y = script_component_bits(vertical), x = script_component_bits(horizontal);
    if (!output || (y & UINT32_C(0x7fffffff)) >= UINT32_C(0x7f800000) ||
        (x & UINT32_C(0x7fffffff)) >= UINT32_C(0x7f800000)) {
        return false;
    }
    bool negative_y = (y >> 31) != 0, negative_x = (x >> 31) != 0;
    const double pi = 0x1.921fb54442d18p+1;
    if (!(y & UINT32_C(0x7fffffff))) {
        *output = negative_x ? (float)(negative_y ? -pi : pi) : vertical;
        return true;
    }
    if (!(x & UINT32_C(0x7fffffff))) {
        *output = (float)(negative_y ? -pi * 0.5 : pi * 0.5);
        return true;
    }
    double positive_y = script_component_magnitude(y);
    double positive_x = script_component_magnitude(x);
    double angle = positive_y > positive_x
                       ? pi * 0.5 - script_acute_angle(positive_x / positive_y)
                       : script_acute_angle(positive_y / positive_x);
    if (negative_x) {
        angle = pi - angle;
    }
    *output = (float)(negative_y ? -angle : angle);
    return true;
}
