#include "ab/camera_mover.h"

bool ab_camera_smoothstep(float lower, float upper, float value, float *output) {
    if (!output) {
        return false;
    }
    float numerator = value - lower;
    float denominator = upper - lower;
    float progress = numerator / denominator;
    /* Original math.min starts from one; math.max starts from positive zero.
     * Equal signed zeros retain that first argument, and NaNs never replace it. */
    float capped = progress < 1.0f ? progress : 1.0f;
    float clamped = capped > 0 ? capped : 0;
    float squared = clamped * clamped;
    float twice = 2.0f * clamped;
    float remaining = 3.0f - twice;
    *output = squared * remaining;
    return true;
}
