#include "ab/rotation.h"

#include <stdint.h>
#include <string.h>

static uint32_t component_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float component_value(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Independently expressed target numeric sequence. The five polynomial
 * parameters and split reference angles are measured from the pinned Android
 * libm binary, not imported source. Retain binary32 intermediate rounding and
 * the negated-accumulator ordering of its VFP multiply/subtract operations.
 */
static float tangent_correction(float reduced) {
    float squared = reduced * reduced;
    float fourth = squared * squared;
    float odd = component_value(UINT32_C(0x3e11f50d)) +
                fourth * component_value(UINT32_C(0x3d7cac25));
    float even = -component_value(UINT32_C(0x3e4cca98)) +
                 fourth * component_value(UINT32_C(0xbdda1247));
    odd = component_value(UINT32_C(0x3eaaaaa9)) + odd * fourth;
    float correction = even * fourth;
    return correction + odd * squared;
}

static float tangent_angle(float value) {
    static const uint32_t reference_high[] = {0x3eed6338, 0x3f490fda, 0x3f7b985e,
                                              0x3fc90fda};
    static const uint32_t reference_low[] = {0x31ac3769, 0x33222168, 0x33140fb4,
                                             0x33a22168};
    uint32_t bits = component_bits(value);
    uint32_t magnitude = bits & UINT32_C(0x7fffffff);
    if (magnitude < UINT32_C(0x39800000)) {
        return value;
    }
    if (magnitude < UINT32_C(0x3ee00000)) {
        return value - value * tangent_correction(value);
    }
    if (magnitude >= UINT32_C(0x4c800000)) {
        float result =
            component_value(reference_high[3]) + component_value(reference_low[3]);
        return bits >> 31 ? -result : result;
    }
    float positive = component_value(magnitude);
    float reduced;
    size_t reference;
    if (magnitude < UINT32_C(0x3f300000)) {
        reduced = (positive + positive - 1.0f) / (positive + 2.0f);
        reference = 0;
    } else if (magnitude < UINT32_C(0x3f980000)) {
        reduced = (positive - 1.0f) / (positive + 1.0f);
        reference = 1;
    } else if (magnitude < UINT32_C(0x401c0000)) {
        float denominator = 1.0f + positive * 1.5f;
        reduced = (positive - 1.5f) / denominator;
        reference = 2;
    } else {
        reduced = -1.0f / positive;
        reference = 3;
    }
    float correction = -component_value(reference_low[reference]) +
                       tangent_correction(reduced) * reduced;
    correction = correction - reduced;
    float result = component_value(reference_high[reference]) - correction;
    return bits >> 31 ? -result : result;
}

bool ab_angle_from_components(float vertical, float horizontal, float *output) {
    uint32_t y_bits = component_bits(vertical), x_bits = component_bits(horizontal);
    uint32_t y_magnitude = y_bits & UINT32_C(0x7fffffff);
    uint32_t x_magnitude = x_bits & UINT32_C(0x7fffffff);
    if (!output || y_magnitude >= UINT32_C(0x7f800000) ||
        x_magnitude >= UINT32_C(0x7f800000)) {
        return false;
    }
    if (x_bits == UINT32_C(0x3f800000)) {
        *output = tangent_angle(vertical);
        return true;
    }
    const float pi = component_value(UINT32_C(0x40490fdb));
    const float half_pi = component_value(UINT32_C(0x3fc90fdb));
    const float pi_low = component_value(UINT32_C(0xb3bbbd2e));
    bool negative_y = (y_bits >> 31) != 0;
    bool negative_x = (x_bits >> 31) != 0;
    if (!y_magnitude) {
        *output = negative_x ? (negative_y ? -pi : pi) : vertical;
        return true;
    }
    if (!x_magnitude) {
        *output = negative_y ? -half_pi : half_pi;
        return true;
    }
    int32_t difference = (int32_t)y_magnitude - (int32_t)x_magnitude;
    int32_t exponent_gap =
        difference >= 0 ? difference / INT32_C(0x800000)
                        : -((-difference + INT32_C(0x7fffff)) / INT32_C(0x800000));
    if (exponent_gap > 26) {
        float result = half_pi + pi_low * 0.5f;
        *output = negative_y ? -result : result;
        return true;
    }
    float acute = 0;
    if (!negative_x || exponent_gap >= -26) {
        float ratio = vertical / horizontal;
        acute = tangent_angle(
            component_value(component_bits(ratio) & UINT32_C(0x7fffffff)));
    }
    if (!negative_x) {
        *output = negative_y ? -acute : acute;
    } else if (negative_y) {
        *output = (acute - pi_low) - pi;
    } else {
        *output = pi - (acute - pi_low);
    }
    return true;
}
