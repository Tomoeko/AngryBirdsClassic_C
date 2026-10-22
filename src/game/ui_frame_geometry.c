#include "ab/ui_frame_geometry.h"

#include <math.h>

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

bool ab_ui_frame_screen_position(const AbUiFrameGeometry *frame, size_t maximum_depth,
                                 AbVec2 *output) {
    if (!frame || !output || !maximum_depth || !finite_point(frame->position)) {
        return false;
    }
    AbVec2 position = frame->position;
    size_t depth = 1;
    for (const AbUiFrameGeometry *parent = frame->parent; parent;
         parent = parent->parent) {
        if (depth++ >= maximum_depth || !finite_point(parent->position) ||
            (parent->scroll_present & ~3u)) {
            return false;
        }
        position.x = position.x + parent->position.x;
        position.y = position.y + parent->position.y;
        if (parent->scroll_present == 3) {
            if (!finite_point(parent->scroll)) {
                return false;
            }
            position.x = position.x - parent->scroll.x;
            position.y = position.y - parent->scroll.y;
        }
        if (!finite_point(position)) {
            return false;
        }
    }
    *output = position;
    return true;
}

bool ab_ui_frame_cumulative_scale(const AbUiFrameGeometry *frame, size_t maximum_depth,
                                  AbVec2 *output) {
    if (!output) {
        return false;
    }
    AbVec2 scale = {1, 1};
    size_t depth = 0;
    for (const AbUiFrameGeometry *item = frame; item; item = item->parent) {
        if (depth++ >= maximum_depth || !finite_point(item->scale)) {
            return false;
        }
        scale.x = scale.x * item->scale.x;
        scale.y = scale.y * item->scale.y;
        if (!finite_point(scale)) {
            return false;
        }
    }
    *output = scale;
    return true;
}

bool ab_ui_invisible_hit(const AbUiInvisibleGeometry *geometry, AbVec2 point,
                         bool *output) {
    if (!geometry || !output || (geometry->scale_present & ~3u) ||
        !finite_point(geometry->position) || !finite_point(geometry->size) ||
        !finite_point(point)) {
        return false;
    }
    float scale_x = geometry->scale_present & 1 ? geometry->scale.x : 1;
    float scale_y = geometry->scale_present & 2 ? geometry->scale.y : 1;
    if (!isfinite(scale_x) || !isfinite(scale_y)) {
        return false;
    }
    float half_width = geometry->size.x * 0.5f;
    float half_height = geometry->size.y * 0.5f;
    float left = geometry->position.x - half_width * scale_x;
    float right = geometry->position.x + (geometry->size.x - half_width) * scale_x;
    float top = geometry->position.y - half_height * scale_y;
    float bottom = geometry->position.y + (geometry->size.y - half_height) * scale_y;
    if (!isfinite(left) || !isfinite(right) || !isfinite(top) || !isfinite(bottom)) {
        return false;
    }
    *output =
        left <= point.x && point.x <= right && top <= point.y && point.y <= bottom;
    return true;
}
