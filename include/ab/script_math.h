#ifndef AB_SCRIPT_MATH_H
#define AB_SCRIPT_MATH_H

#include "ab/physics_geometry.h"

#include <stdbool.h>

/* Original Lua float -> binary64 sine -> float numeric boundary, independently
 * evaluated in C. Finite |angle| <= 1024, nearest rounding and gradual underflow.
 * Unsupported input preserves output. This is distinct from physics sinf. */
bool ab_script_sine(float angle, float *output);
/* Matching float -> binary64 cosine -> float boundary, same finite arc domain. */
bool ab_script_cosine(float angle, float *output);

/* Multiply binary64 cosine/sine by a promoted float magnitude before rounding
 * each coordinate to float. This does not round the trigonometric result first.
 * Same finite arc domain; magnitude and resulting coordinates must be finite.
 * Failure preserves output. */
bool ab_script_polar(float angle, float magnitude, AbVec2 *output);

#endif
