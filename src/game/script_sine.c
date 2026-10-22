#include "ab/script_math.h"

#include <math.h>
#include <stdint.h>

/* Lua math.sin promotes its float argument to binary64 and rounds the result
 * back to float (native wrapper at analysis 0x8300bc). Physics sinf uses a
 * different approximation. Independently evaluate Taylor polynomials after
 * split-pi reduction; at |reduced| <= pi/4 their omitted terms are below 1e-19.
 * Keep the supported arc domain explicit instead of changing physics math.
 */
static double arc_sine_polynomial(double value) {
    double squared = value * value;
    double polynomial = 1.0 / 355687428096000.0;
    polynomial = polynomial * squared - 1.0 / 1307674368000.0;
    polynomial = polynomial * squared + 1.0 / 6227020800.0;
    polynomial = polynomial * squared - 1.0 / 39916800.0;
    polynomial = polynomial * squared + 1.0 / 362880.0;
    polynomial = polynomial * squared - 1.0 / 5040.0;
    polynomial = polynomial * squared + 1.0 / 120.0;
    polynomial = polynomial * squared - 1.0 / 6.0;
    return value + (value * squared) * polynomial;
}

static double arc_cosine_polynomial(double value) {
    double squared = value * value;
    double polynomial = -1.0 / 6402373705728000.0;
    polynomial = polynomial * squared + 1.0 / 20922789888000.0;
    polynomial = polynomial * squared - 1.0 / 87178291200.0;
    polynomial = polynomial * squared + 1.0 / 479001600.0;
    polynomial = polynomial * squared - 1.0 / 3628800.0;
    polynomial = polynomial * squared + 1.0 / 40320.0;
    polynomial = polynomial * squared - 1.0 / 720.0;
    polynomial = polynomial * squared + 1.0 / 24.0;
    polynomial = polynomial * squared - 1.0 / 2.0;
    return 1.0 + squared * polynomial;
}

static double reduce_arc(float angle, int32_t *quadrant) {
    double nearest = (double)angle * 0x1.45f306dc9c883p-1;
    *quadrant = (int32_t)(nearest + (nearest < 0 ? -0.5 : 0.5));
    double reduced = (double)angle - (double)*quadrant * 0x1.921fb5p+0;
    return reduced - (double)*quadrant * 0x1.110b4611a6263p-26;
}

bool ab_script_sine(float angle, float *output) {
    if (!output || !isfinite(angle) || angle < -1024 || angle > 1024) {
        return false;
    }
    if (angle > -0x1p-12f && angle < 0x1p-12f) {
        *output = angle;
        return true;
    }
    int32_t quadrant;
    double reduced = reduce_arc(angle, &quadrant);
    double value =
        (quadrant & 1) ? arc_cosine_polynomial(reduced) : arc_sine_polynomial(reduced);
    *output = (float)(((uint32_t)quadrant & 2) ? -value : value);
    return true;
}

bool ab_script_cosine(float angle, float *output) {
    if (!output || !isfinite(angle) || angle < -1024 || angle > 1024) {
        return false;
    }
    int32_t quadrant;
    double reduced = reduce_arc(angle, &quadrant);
    double value =
        (quadrant & 1) ? arc_sine_polynomial(reduced) : arc_cosine_polynomial(reduced);
    *output = (float)((((uint32_t)quadrant + 1) & 2) ? -value : value);
    return true;
}

bool ab_script_polar(float angle, float magnitude, AbVec2 *output) {
    if (!output || !isfinite(angle) || angle < -1024 || angle > 1024 ||
        !isfinite(magnitude)) {
        return false;
    }
    int32_t quadrant;
    double reduced = reduce_arc(angle, &quadrant);
    double sine = angle == 0 ? (double)angle : arc_sine_polynomial(reduced);
    double cosine = arc_cosine_polynomial(reduced);
    if ((uint32_t)quadrant & 1) {
        double saved = sine;
        sine = cosine;
        cosine = saved;
    }
    if ((uint32_t)quadrant & 2) {
        sine = -sine;
    }
    if (((uint32_t)quadrant + 1) & 2) {
        cosine = -cosine;
    }
    AbVec2 result = {(float)((double)magnitude * cosine),
                     (float)((double)magnitude * sine)};
    if (!isfinite(result.x) || !isfinite(result.y)) {
        return false;
    }
    *output = result;
    return true;
}
