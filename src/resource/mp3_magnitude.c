#include "ab/mp3.h"

#include <float.h>
#include <string.h>

_Static_assert(sizeof(float) == sizeof(uint32_t) && FLT_RADIX == 2 &&
                   FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128,
               "MP3 magnitude reconstruction requires binary32 float");

typedef struct {
    uint64_t high, low;
} MagnitudeWide;

/* At most a 25-bit significand is cubed. Split its square before the last
 * multiplication so this remains ordinary portable C11 integer arithmetic. */
static MagnitudeWide cube_significand(uint32_t value) {
    uint64_t square = (uint64_t)value * value;
    uint64_t low = (square & UINT32_MAX) * value;
    uint64_t upper = (square >> 32) * value + (low >> 32);
    return (MagnitudeWide){upper >> 32, (upper << 32) | (low & UINT32_MAX)};
}

static MagnitudeWide shifted_fourth(uint64_t value, unsigned shift) {
    if (shift >= 64) {
        return (MagnitudeWide){value << (shift - 64), 0};
    }
    return (MagnitudeWide){shift ? value >> (64 - shift) : 0, value << shift};
}

static int compare_wide(MagnitudeWide left, MagnitudeWide right) {
    if (left.high != right.high) {
        return left.high < right.high ? -1 : 1;
    }
    if (left.low != right.low) {
        return left.low < right.low ? -1 : 1;
    }
    return 0;
}

static int compare_cube(uint32_t bits, uint64_t fourth, bool midpoint) {
    unsigned exponent = (bits >> 23) - 127u;
    uint32_t significand = (bits & UINT32_C(0x7fffff)) | UINT32_C(0x800000);
    if (midpoint) {
        significand = significand * 2 + 1;
    }
    /* The significand has 23 fraction bits, or 24 at an adjacent-float
     * midpoint. Cubing therefore introduces 69 or 72 binary scale bits. */
    unsigned shift = (midpoint ? 72u : 69u) - exponent * 3;
    return compare_wide(cube_significand(significand), shifted_fourth(fourth, shift));
}

bool ab_mp3_magnitude_power(uint16_t magnitude, float *output, AbError *error) {
    if (magnitude > 8206 || !output) {
        return ab_fail(error, "Invalid MP3 magnitude power input or output");
    }
    uint32_t result = 0;
    if (magnitude) {
        uint64_t square = (uint64_t)magnitude * magnitude;
        uint64_t fourth = square * square;
        /* The bounded result lies in [1, 2^18). Positive binary32 encodings are
         * monotone. Find the lower neighbor using exact cubed comparisons. */
        uint32_t first = UINT32_C(0x3f800000), end = UINT32_C(0x48800000);
        while (first < end) {
            uint32_t middle = first + (end - first + 1) / 2;
            if (compare_cube(middle, fourth, false) <= 0) {
                first = middle;
            } else {
                end = middle - 1;
            }
        }
        int comparison = compare_cube(first, fourth, true);
        result =
            first + (comparison < 0 || (comparison == 0 && (first & 1u)) ? 1u : 0u);
    }
    memcpy(output, &result, sizeof(result));
    return true;
}

bool ab_mp3_gain_power(int quarter_steps, float *output, AbError *error) {
    if (quarter_steps < -46 || quarter_steps > 331 || !output) {
        return ab_fail(error, "Invalid MP3 quarter-step gain or output");
    }
    static const uint32_t fractions[] = {UINT32_C(0x3f800000), UINT32_C(0x3f5744fd),
                                         UINT32_C(0x3f3504f3), UINT32_C(0x3f1837f0)};
    int exponent = quarter_steps / 4;
    int remainder = quarter_steps % 4;
    if (remainder < 0) {
        remainder += 4;
        --exponent;
    }
    uint32_t fraction = fractions[remainder];
    uint32_t adjusted = (uint32_t)((int)(fraction >> 23) - exponent);
    uint32_t bits = (fraction & UINT32_C(0x7fffff)) | adjusted << 23;
    memcpy(output, &bits, sizeof(bits));
    return true;
}
