#include "ab/camera_zoom_fit.h"
#include "script_float_internal.h"

#include <math.h>

bool ab_camera_zoom_fit(float projected_y, float point_y, float close_zoom,
                        float screen_height, float bottom_border, float *output) {
    if (!output || !isfinite(projected_y) || !isfinite(point_y) ||
        !isfinite(close_zoom) || !isfinite(screen_height) || !isfinite(bottom_border)) {
        return false;
    }
    if (point_y == projected_y) {
        *output = close_zoom;
        return true;
    }
    float half = script_float_binary(screen_height, 2, AB_SCRIPT_FLOAT_DIVIDE);
    float difference =
        script_float_binary(projected_y, point_y, AB_SCRIPT_FLOAT_SUBTRACT);
    float zoom = script_float_binary(half, fabsf(difference), AB_SCRIPT_FLOAT_DIVIDE);
    float radius = script_float_binary(screen_height, 0.5f, AB_SCRIPT_FLOAT_MULTIPLY);
    radius = script_float_binary(radius, zoom, AB_SCRIPT_FLOAT_DIVIDE);
    float bottom = script_float_binary(projected_y, radius, AB_SCRIPT_FLOAT_ADD);
    if (bottom <= bottom_border) {
        *output = zoom;
        return true;
    }
    difference = script_float_binary(bottom_border, point_y, AB_SCRIPT_FLOAT_SUBTRACT);
    *output =
        script_float_binary(screen_height, fabsf(difference), AB_SCRIPT_FLOAT_DIVIDE);
    return true;
}
