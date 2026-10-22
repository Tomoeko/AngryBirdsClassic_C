#ifndef AB_CAMERA_ZOOM_FIT_H
#define AB_CAMERA_ZOOM_FIT_H

#include "ab/base.h"

/* CameraLimitEnforcer.getZoomScaleToFitOnScreen after its projection boundary.
 * Equal requested/projected Y returns close_zoom. Otherwise fit around the
 * projected Y first, then refit against bottom_border if the resulting bottom
 * exceeds that border. Preserve binary32 operation order and strict native
 * comparisons, including nonfinite intermediate/result words from finite
 * inputs. Projection and caller-selected screen height are explicit inputs.
 * No allocation or clamping. Invalid/nonfinite inputs preserve output. */
bool ab_camera_zoom_fit(float projected_y, float point_y, float close_zoom,
                        float screen_height, float bottom_border, float *output);

#endif
