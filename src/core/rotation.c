#include "ab/rotation.h"

#include <float.h>
#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   DBL_MANT_DIG == 53,
               "Reference rotation arithmetic requires binary32 and binary64");

/* Independently authored range reduction and polynomial evaluation. Numeric
 * parameters and evaluation precision are verified against the recorded Android
 * C-library binary and live results; see docs/android-reference.md.
 */
static float sine_polynomial(double angle) {
    double squared = angle * angle;
    double cubic = angle * squared;
    double fourth = squared * squared;
    double lower = squared * 0x1.11110896efbb2p-7 - 0x1.5555554cbac77p-3;
    double upper = squared * 0x1.6cd878c3b46a7p-19 - 0x1.a00f9e2cae774p-13;
    double result = angle + cubic * lower;
    double seventh = fourth * cubic;
    return (float)(result + upper * seventh);
}

static float cosine_polynomial(double angle) {
    double squared = angle * angle;
    double result = 1.0 + squared * (-0x1.ffffffd0c5e81p-2);
    double fourth = squared * squared;
    double upper = squared * 0x1.99342e0ee5069p-16 - 0x1.6c087e80f1e27p-10;
    result = result + fourth * 0x1.55553e1053a42p-5;
    double sixth = squared * fourth;
    return (float)(result + upper * sixth);
}

static double reduce_angle(float angle, uint32_t bits, uint32_t magnitude,
                           int32_t *quadrant) {
    if (magnitude <= UINT32_C(0x3f490fda)) {
        *quadrant = 0;
        return angle;
    }
    if (magnitude <= UINT32_C(0x40e231d5)) {
        int32_t sector = magnitude <= UINT32_C(0x4016cbe3)   ? 1
                         : magnitude <= UINT32_C(0x407b53d1) ? 2
                         : magnitude <= UINT32_C(0x40afeddf) ? 3
                                                             : 4;
        *quadrant = bits >> 31 ? -sector : sector;
        return (double)angle - (double)*quadrant * 0x1.921fb54442d18p+0;
    }
    /* At the accepted bound the rounded multiple fits int32_t. The binary64
     * addition/subtraction rounds that multiple to an integer in nearest mode.
     */
    double nearest = (double)angle * 0x1.45f306dc9c883p-1;
    nearest = (nearest + 0x1.8p52) - 0x1.8p52;
    *quadrant = (int32_t)nearest;
    double reduced = (double)angle - nearest * 0x1.921fb5p+0;
    return reduced - nearest * 0x1.110b4611a6263p-26;
}

bool ab_rotation_from_angle(float angle, AbRotation *output) {
    uint32_t bits;
    memcpy(&bits, &angle, sizeof(bits));
    uint32_t magnitude = bits & UINT32_C(0x7fffffff);
    if (!output || magnitude > UINT32_C(0x4dc90fda)) {
        return false;
    }
    if (magnitude < UINT32_C(0x39800000)) {
        *output = (AbRotation){angle, 1};
        return true;
    }
    int32_t quadrant;
    double reduced = reduce_angle(angle, bits, magnitude, &quadrant);
    float sine = sine_polynomial(reduced);
    float cosine = cosine_polynomial(reduced);
    switch ((uint32_t)quadrant & 3) {
        case 0:
            *output = (AbRotation){sine, cosine};
            break;
        case 1:
            *output = (AbRotation){cosine, -sine};
            break;
        case 2:
            *output = (AbRotation){-sine, -cosine};
            break;
        default:
            *output = (AbRotation){-cosine, sine};
            break;
    }
    return true;
}
