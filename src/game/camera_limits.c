#include "ab/camera_limits.h"

#include <math.h>

enum { LEFT = 1, RIGHT = 2, TOP = 4, BOTTOM = 8 };

static bool finite_point(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static bool extents(AbVec2 position, float zoom, AbVec2 pixels,
                    AbCameraViewport *output) {
    AbCameraViewportInput input = {position, pixels, {0, 0}, zoom};
    return ab_camera_viewport(&input, output);
}

static float minimum(float first, float second) {
    return second < first ? second : first;
}

static float maximum(float first, float second) {
    return first < second ? second : first;
}

bool ab_camera_limits_initialize(const AbCameraSnap *snaps, size_t count,
                                 AbVec2 pixel_size, float absolute_bottom,
                                 float horizontal_leeway, AbCameraLimits *output) {
    if (!snaps || !count || count > SIZE_MAX / sizeof(*snaps) || !output ||
        !isfinite(absolute_bottom) || !isfinite(horizontal_leeway)) {
        return false;
    }
    const AbCameraSnap *first = snaps, *last = snaps + count - 1;
    AbCameraViewport near, far;
    if (!extents(first->position, first->zoom, pixel_size, &near) ||
        !extents(last->position, last->zoom, pixel_size, &far)) {
        return false;
    }
    AbCameraLimits result = {.snaps = snaps,
                             .snap_count = count,
                             .left = minimum(near.top_left.x, far.top_left.x) - 1,
                             .right =
                                 maximum(near.bottom_right.x, far.bottom_right.x) + 1,
                             .close_zoom = maximum(first->zoom, last->zoom),
                             .absolute_bottom = absolute_bottom,
                             .horizontal_leeway = horizontal_leeway,
                             .enabled = true};
    result.far_zoom = pixel_size.x / (result.right - result.left);
    AbVec2 center = {(first->position.x + last->position.x) / 2.0f,
                     (first->position.y + last->position.y) / 2.0f};
    AbCameraViewport combined;
    if (!extents(center, result.far_zoom, pixel_size, &combined)) {
        return false;
    }
    result.bottom = minimum(absolute_bottom, combined.bottom_right.y);
    result.top = pixel_size.y / result.far_zoom - result.bottom;
    if (!isfinite(result.left) || !isfinite(result.right) ||
        !isfinite(result.close_zoom) || !isfinite(result.top)) {
        return false;
    }
    *output = result;
    return true;
}

bool ab_camera_limits_project(const AbCameraLimits *limits, AbVec2 wanted,
                              AbVec2 *output) {
    if (!limits || !limits->snaps || !limits->snap_count || !output ||
        !finite_point(wanted)) {
        return false;
    }
    const AbCameraSnap *left = NULL, *right = NULL;
    for (size_t index = 0; index < limits->snap_count; ++index) {
        const AbCameraSnap *snap = limits->snaps + index;
        if (!finite_point(snap->position)) {
            return false;
        }
        if (snap->position.x < wanted.x) {
            if (!left || left->position.x < snap->position.x) {
                left = snap;
            }
        } else if (wanted.x < snap->position.x) {
            if (!right || snap->position.x < right->position.x) {
                right = snap;
            }
        } else {
            *output = snap->position;
            return true;
        }
    }
    if (!left || !right) {
        *output = (left ? left : right)->position;
        return true;
    }
    AbVec2 direction = {right->position.x - left->position.x,
                        right->position.y - left->position.y};
    float squared = direction.x * direction.x + direction.y * direction.y;
    if (!isfinite(squared)) {
        return false;
    }
    if (squared == 0) {
        *output = left->position;
        return true;
    }
    AbVec2 offset = {wanted.x - left->position.x, wanted.y - left->position.y};
    float length = sqrtf(squared);
    float dot = offset.x * direction.x + offset.y * direction.y;
    float factor = dot / (length * length);
    AbVec2 result = {factor * direction.x + left->position.x,
                     factor * direction.y + left->position.y};
    if (!finite_point(offset) || !isfinite(dot) || !isfinite(factor) ||
        !finite_point(result)) {
        return false;
    }
    *output = result;
    return true;
}

static bool values_valid(const AbCameraValues *values) {
    return values && !(values->present & ~UINT32_C(7)) &&
           (!(values->present & AB_CAMERA_VALUE_X) || isfinite(values->position.x)) &&
           (!(values->present & AB_CAMERA_VALUE_Y) || isfinite(values->position.y));
}

bool ab_camera_limits_zoom(const AbCameraLimits *limits, const AbCameraValues *values,
                           AbCameraValues *output) {
    if (!limits || !output || !values_valid(values)) {
        return false;
    }
    AbCameraValues result = *values;
    if (values->present & AB_CAMERA_VALUE_ZOOM) {
        if (!isfinite(limits->far_zoom) || !isfinite(limits->close_zoom)) {
            return false;
        }
        result.zoom =
            maximum(limits->far_zoom, minimum(limits->close_zoom, values->zoom));
    }
    *output = result;
    return true;
}

static bool resolved(const AbCameraLimitContext *context, const AbCameraValues *values,
                     AbVec2 *position, float *zoom) {
    if (!context || !values_valid(values)) {
        return false;
    }
    *position =
        (AbVec2){values->present & AB_CAMERA_VALUE_X ? values->position.x
                                                     : context->current_position.x,
                 values->present & AB_CAMERA_VALUE_Y ? values->position.y
                                                     : context->current_position.y};
    *zoom =
        values->present & AB_CAMERA_VALUE_ZOOM ? values->zoom : context->current_zoom;
    return finite_point(*position) && isfinite(*zoom) && *zoom != 0;
}

static bool valid_borders(const AbCameraLimits *limits, bool check_top) {
    return limits && isfinite(limits->left) && isfinite(limits->right) &&
           isfinite(limits->bottom) && (!check_top || isfinite(limits->top));
}

static uint32_t broken(const AbCameraLimits *limits, const AbCameraViewport *view,
                       bool check_top) {
    return (view->top_left.x < limits->left ? LEFT : 0u) |
           (limits->right < view->bottom_right.x ? RIGHT : 0u) |
           (check_top && view->top_left.y < limits->top ? TOP : 0u) |
           (limits->bottom < view->bottom_right.y ? BOTTOM : 0u);
}

bool ab_camera_limits_position(const AbCameraLimits *limits,
                               const AbCameraLimitContext *context,
                               const AbCameraValues *values, bool check_top,
                               AbCameraValues *output) {
    AbVec2 position;
    float zoom;
    AbCameraViewport first, second;
    if (!output || !valid_borders(limits, check_top) ||
        !resolved(context, values, &position, &zoom) ||
        context->original_present & ~UINT32_C(3) ||
        !extents(position, zoom, context->pixel_size, &first)) {
        return false;
    }
    uint32_t before = broken(limits, &first, check_top);
    if (before & LEFT) {
        position.x = (position.x + limits->left) - first.top_left.x;
    } else if (before & RIGHT) {
        position.x = (position.x + limits->right) - first.bottom_right.x;
    }
    if (before & TOP) {
        position.y = (position.y + limits->top) - first.top_left.y;
    } else if (before & BOTTOM) {
        position.y = (position.y + limits->bottom) - first.bottom_right.y;
    }
    if (!extents(position, zoom, context->pixel_size, &second)) {
        return false;
    }
    uint32_t after = broken(limits, &second, check_top);
    if (((before & LEFT) && (after & RIGHT)) || ((before & RIGHT) && (after & LEFT))) {
        position.x = (limits->left + limits->right) * 0.5f;
    }
    if (((before & TOP) && (after & BOTTOM)) || ((before & BOTTOM) && (after & TOP))) {
        position.y = (limits->top + limits->bottom) * 0.5f;
    }
    if (!finite_point(position)) {
        return false;
    }
    AbCameraValues result = *values;
    if (!(context->original_present & AB_CAMERA_VALUE_X) ||
        position.x != context->original_globals.x) {
        result.position.x = position.x;
        result.present |= AB_CAMERA_VALUE_X;
    }
    if (!(context->original_present & AB_CAMERA_VALUE_Y) ||
        position.y != context->original_globals.y) {
        result.position.y = position.y;
        result.present |= AB_CAMERA_VALUE_Y;
    }
    *output = result;
    return true;
}

static float horizontal_resistance(float distance) {
    float square = distance * distance;
    float denominator = square + 200.0f * distance;
    denominator = denominator + 1.0f;
    return square / denominator;
}

bool ab_camera_limits_input(const AbCameraLimits *limits,
                            const AbCameraLimitContext *context,
                            const AbCameraValues *values, AbCameraValues *output) {
    if (!limits || !output || !values_valid(values)) {
        return false;
    }
    if (!limits->enabled) {
        *output = *values;
        return true;
    }
    AbCameraValues clamped;
    if (!ab_camera_limits_zoom(limits, values, &clamped)) {
        return false;
    }
    values = &clamped;
    AbVec2 position;
    float zoom;
    if (!valid_borders(limits, false) || !isfinite(limits->horizontal_leeway) ||
        !resolved(context, values, &position, &zoom)) {
        return false;
    }
    AbVec2 original = position, projected;
    if (!ab_camera_limits_project(limits, position, &projected)) {
        return false;
    }
    position.y = projected.y;
    AbCameraViewport view;
    if (!extents(position, zoom, context->pixel_size, &view)) {
        return false;
    }
    uint32_t constraints = broken(limits, &view, false);
    if (constraints & BOTTOM) {
        position.y = (position.y + limits->bottom) - view.bottom_right.y;
    }
    if (constraints & LEFT) {
        float distance = fabsf(limits->left - view.top_left.x);
        float resistance = horizontal_resistance(distance);
        float border = limits->left - resistance * limits->horizontal_leeway;
        float offset = position.x - view.top_left.x;
        position.x = border + offset;
    } else if (constraints & RIGHT) {
        float distance = fabsf(limits->right - view.bottom_right.x);
        float resistance = horizontal_resistance(distance);
        float border = limits->right + resistance * limits->horizontal_leeway;
        float offset = position.x - view.bottom_right.x;
        position.x = border + offset;
    }
    if (!finite_point(position)) {
        return false;
    }
    AbCameraValues result = *values;
    if (position.x != original.x) {
        result.position.x = position.x;
        result.present |= AB_CAMERA_VALUE_X;
    }
    if (position.y != original.y) {
        result.position.y = position.y;
        result.present |= AB_CAMERA_VALUE_Y;
    }
    *output = result;
    return true;
}
