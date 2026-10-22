#ifndef AB_ROTATION_H
#define AB_ROTATION_H

#include <stdbool.h>

typedef struct {
    float sine;
    float cosine;
} AbRotation;

/* Numeric results for the measured Android reference math domain:
 * |angle| <= 0x1.921fb4p+28 radians, nearest rounding, gradual underflow, and
 * disabled floating-point traps. Other inputs fail without changing output.
 * Floating-point exception/status flags are not part of this result contract.
 * No platform sine/cosine function or approximation fallback is used.
 */
bool ab_rotation_from_angle(float angle, AbRotation *output);

/* Android-reference atan2f result for finite components, including signed zero.
 * The first component is the vertical/sine argument, the second horizontal/cosine.
 * Return an angle in [-pi, pi] using authored binary32 arithmetic. The same
 * rounding/underflow/status contract applies; invalid input preserves output.
 */
bool ab_angle_from_components(float vertical, float horizontal, float *output);

/* Lua math.atan2 promotes both float arguments to double, then rounds its
 * result back to float. Keep this separate from the physics atan2f sequence.
 * Finite components only, with the same output-preservation/status contract.
 */
bool ab_script_angle_from_components(float vertical, float horizontal, float *output);

#endif
