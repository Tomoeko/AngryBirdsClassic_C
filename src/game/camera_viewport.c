#include "ab/camera_viewport.h"

#include <math.h>

static bool finite_vector(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

bool ab_camera_viewport(const AbCameraViewportInput *input, AbCameraViewport *output) {
    if (!input || !output || !finite_vector(input->center) ||
        !finite_vector(input->pixel_size) || !finite_vector(input->shake) ||
        !isfinite(input->zoom) || input->zoom == 0) {
        return false;
    }
    AbVec2 half = {input->pixel_size.x * 0.5f, input->pixel_size.y * 0.5f};
    AbVec2 radius = {half.x / input->zoom, half.y / input->zoom};
    AbCameraViewport result = {
        .top_left = {input->center.x - radius.x, input->center.y - radius.y},
        .bottom_right = {input->center.x + radius.x, input->center.y + radius.y},
        .world_size = {input->pixel_size.x / input->zoom,
                       input->pixel_size.y / input->zoom}};
    result.rendering =
        (AbCoordinateCamera){.top_left = {result.top_left.x - input->shake.x,
                                          result.top_left.y - input->shake.y},
                             .zoom = {input->zoom, input->zoom}};
    if (!finite_vector(half) || !finite_vector(radius) ||
        !finite_vector(result.top_left) || !finite_vector(result.bottom_right) ||
        !finite_vector(result.world_size) ||
        !finite_vector(result.rendering.top_left)) {
        return false;
    }
    *output = result;
    return true;
}
