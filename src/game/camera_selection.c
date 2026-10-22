#include "ab/camera_selection.h"

#include <math.h>
#include <string.h>

static bool valid_snaps(const AbCameraSnap *snaps, size_t count, AbVec2 position,
                        const size_t *output) {
    if (!snaps || !count || count >= UINT32_C(0x1000000) || !output ||
        !isfinite(position.x) || !isfinite(position.y)) {
        return false;
    }
    for (size_t index = 0; index < count; ++index) {
        if (!isfinite(snaps[index].position.x) || !isfinite(snaps[index].position.y)) {
            return false;
        }
    }
    return true;
}

static float distance(AbVec2 first, AbVec2 second) {
    float x = first.x - second.x;
    float y = first.y - second.y;
    float x_squared = x * x;
    float y_squared = y * y;
    return sqrtf(x_squared + y_squared);
}

bool ab_camera_snap_closest(const AbCameraSnap *snaps, size_t count, AbVec2 position,
                            size_t *output) {
    if (!valid_snaps(snaps, count, position, output)) {
        return false;
    }
    size_t selected = 0;
    float nearest = distance(snaps[0].position, position);
    for (size_t index = 1; index < count; ++index) {
        float candidate = distance(snaps[index].position, position);
        if (candidate < nearest) {
            selected = index;
            nearest = candidate;
        }
    }
    *output = selected;
    return true;
}

bool ab_camera_snap_direction(const AbCameraSnap *snaps, size_t count, AbVec2 position,
                              float velocity_x, size_t *output) {
    if (!valid_snaps(snaps, count, position, output)) {
        return false;
    }
    float sign = velocity_x <= 0 ? -1.0f : 1.0f;
    size_t selected = SIZE_MAX;
    float nearest = 0;
    for (size_t index = 0; index < count; ++index) {
        float x = snaps[index].position.x - position.x;
        float y = snaps[index].position.y - position.y;
        float horizontal = x * sign;
        float vertical = y * 0.0f;
        float dot = horizontal + vertical;
        if (!(dot > 0)) {
            continue;
        }
        float candidate = distance(position, snaps[index].position);
        if (selected == SIZE_MAX || candidate < nearest) {
            selected = index;
            nearest = candidate;
        }
    }
    if (selected == SIZE_MAX) {
        return ab_camera_snap_closest(snaps, count, position, output);
    }
    *output = selected;
    return true;
}

bool ab_camera_relative_zoom(float current, float close, float far, float *output) {
    if (!output || !isfinite(current) || !isfinite(close) || !isfinite(far)) {
        return false;
    }
    float numerator = current - far;
    float denominator = close - far;
    float quotient = numerator / denominator;
    float result = 1.0f - quotient;
    if (isnan(result)) {
        uint32_t word = UINT32_C(0x7fc00000);
        memcpy(&result, &word, sizeof(result));
    }
    *output = result;
    return true;
}

bool ab_camera_snap_zoom(float current, float close, float far, bool suppress,
                         float velocity, float *output) {
    if (!output || !isfinite(current) || !isfinite(close) || !isfinite(far)) {
        return false;
    }
    float result = current;
    if (!suppress) {
        if (velocity > 0) {
            result = close;
        } else if (velocity < 0) {
            result = far;
        } else {
            float relative;
            if (!ab_camera_relative_zoom(current, close, far, &relative)) {
                return false;
            }
            result = relative < 0.5f ? close : far;
        }
    }
    *output = result;
    return true;
}
