#ifndef AB_CAMERA_SELECTION_H
#define AB_CAMERA_SELECTION_H

#include "ab/camera_limits.h"

/* CameraSystem snap/zoom queries. Arrays stay borrowed; outputs are zero-based
 * indices. Euclidean distances retain separate subtraction/square/add/sqrt
 * operations and strict ties select the first supplied snap. Position inputs
 * must be finite; zoom metadata is not used by these position queries. Empty or
 * invalid input preserves output. No allocation, copying or controller actions. */
bool ab_camera_snap_closest(const AbCameraSnap *snaps, size_t count, AbVec2 position,
                            size_t *output);
/* Select the nearest snap strictly in the requested horizontal direction;
 * velocity <= zero chooses left, otherwise right. Preserve the original dot's
 * vertical zero product, including overflow effects. Fall back to the closest
 * snap to current position when that side is empty. Velocity may be nonfinite;
 * Y velocity is unused. */
bool ab_camera_snap_direction(const AbCameraSnap *snaps, size_t count, AbVec2 position,
                              float velocity_x, size_t *output);

/* Original 1-(current-far)/(close-far), without clamping. Finite inputs can yield
 * infinity/NaN on equal limits; newly invalid arithmetic uses target default
 * quiet NaN. Floating status flags are outside this numeric contract. */
bool ab_camera_relative_zoom(float current, float close, float far, float *output);
/* A truthy native snapZoom field suppresses selection and returns current.
 * Otherwise positive velocity selects close,
 * negative selects far, and zero/unordered velocity compares relative zoom
 * strictly below one-half. A half tie selects far. Finite zoom inputs required;
 * velocity may be nonfinite. No limit enforcement or movement is applied. */
bool ab_camera_snap_zoom(float current, float close, float far, bool suppress,
                         float velocity, float *output);

#endif
