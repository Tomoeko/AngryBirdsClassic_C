#ifndef AB_CAMERA_VIEWPORT_H
#define AB_CAMERA_VIEWPORT_H

#include "ab/coordinates.h"

typedef struct {
    AbVec2 center;
    AbVec2 pixel_size;
    AbVec2 shake;
    float zoom;
} AbCameraViewportInput;

typedef struct {
    AbVec2 top_left;
    AbVec2 bottom_right;
    AbVec2 world_size;
    AbCoordinateCamera rendering;
} AbCameraViewport;

/* CameraSystem screen extents and native rendering setup. Multiply pixel
 * dimensions by binary32 one-half before dividing by zoom to form each radius.
 * Add/subtract that radius from center independently; world_size divides the
 * original pixel dimensions by zoom. Rendering subtracts camera shake from
 * logical top-left and uses the same zoom on both axes. Do not derive the
 * far edge from the near edge plus world_size or fold the half/divide order.
 * Used inputs/intermediates/results must be finite and zoom nonzero. Signed
 * values are preserved; failure leaves output unchanged. Camera movement,
 * bounds enforcement, shake generation and device aspect policy are separate.
 */
bool ab_camera_viewport(const AbCameraViewportInput *input, AbCameraViewport *output);

#endif
