#ifndef AB_CAMERA_LIMITS_H
#define AB_CAMERA_LIMITS_H

#include "ab/camera_viewport.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    AbVec2 position;
    float zoom;
} AbCameraSnap;

enum {
    AB_CAMERA_VALUE_X = 1,
    AB_CAMERA_VALUE_Y = 2,
    AB_CAMERA_VALUE_ZOOM = 4,
};

typedef struct {
    AbVec2 position;
    float zoom;
    uint32_t present;
} AbCameraValues;

typedef struct {
    /* Borrow the supplied snap array; initialization examines first and last,
     * while projection scans every snap in its supplied order. */
    const AbCameraSnap *snaps;
    size_t snap_count;
    float left;
    float right;
    float top;
    float bottom;
    float close_zoom;
    float far_zoom;
    float absolute_bottom;
    float horizontal_leeway;
    bool enabled;
} AbCameraLimits;

typedef struct {
    AbVec2 pixel_size;
    AbVec2 current_position;
    float current_zoom;
    /* checkPosition compares against globals originalX/originalY rather than
     * its resolved input position. Absent globals have no matching number. */
    AbVec2 original_globals;
    uint32_t original_present;
} AbCameraLimitContext;

/* Default CameraLimitEnforcer numeric paths. All arrays remain borrowed; no
 * allocation, Lua table identity, callbacks or ZoomCameraLimitEnforcer subclass.
 * Used position/geometry inputs and intermediates must be finite; resolved
 * viewport zoom must be finite and nonzero. Supplied zoom may be nonfinite
 * before clamping or when disabled input is copied unchanged.
 * Output is unchanged on failure and may alias a values input. Numeric present
 * flags collapse nil/false to absence; arbitrary truthy nonnumbers reject at
 * the caller's typed-data boundary. */
bool ab_camera_limits_initialize(const AbCameraSnap *snaps, size_t count,
                                 AbVec2 pixel_size, float absolute_bottom,
                                 float horizontal_leeway, AbCameraLimits *output);
bool ab_camera_limits_project(const AbCameraLimits *limits, AbVec2 wanted,
                              AbVec2 *output);
bool ab_camera_limits_zoom(const AbCameraLimits *limits, const AbCameraValues *values,
                           AbCameraValues *output);
/* Hard position correction preserves the original two extent checks and
 * opposing-edge midpoint fallback. check_top controls only the upper edge;
 * the bottom edge is always checked. It does not honor enabled itself. */
bool ab_camera_limits_position(const AbCameraLimits *limits,
                               const AbCameraLimitContext *context,
                               const AbCameraValues *values, bool check_top,
                               AbCameraValues *output);
/* Default enforceUserInputValues clamps a supplied zoom first. When enabled,
 * project Y to the snap line, constrain bottom, and apply the original horizontal
 * rational leeway. Write X/Y only when changed from their resolved inputs.
 * Native lock sets enabled=true; unlock sets false and copies values unchanged. */
bool ab_camera_limits_input(const AbCameraLimits *limits,
                            const AbCameraLimitContext *context,
                            const AbCameraValues *values, AbCameraValues *output);

#endif
