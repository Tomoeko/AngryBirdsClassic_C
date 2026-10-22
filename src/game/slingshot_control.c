#include "ab/slingshot_control.h"

#include <math.h>

bool ab_slingshot_delta_update(AbVec2 *delta, bool enabled, AbVec2 screen_displacement,
                               const AbCoordinateCamera *camera, float sensitivity,
                               float shoot_range, AbError *error) {
    if (!delta) {
        return ab_fail(error, "Missing sling delta storage");
    }
    if (!enabled) {
        return true;
    }
    AbVec2 converted;
    if (!isfinite(delta->x) || !isfinite(delta->y) || !isfinite(sensitivity) ||
        !isfinite(shoot_range) || shoot_range < 0 ||
        !ab_coordinate_screen_scale(camera, screen_displacement.x, &converted.x) ||
        !ab_coordinate_screen_scale(camera, screen_displacement.y, &converted.y)) {
        return ab_fail(error, "Invalid sling delta input");
    }
    converted.x *= sensitivity;
    converted.y *= sensitivity;
    AbVec2 result = {delta->x - converted.x, delta->y - converted.y};
    float limit = shoot_range + 3.2f;
    float squared = result.x * result.x + result.y * result.y;
    if (!isfinite(converted.x) || !isfinite(converted.y) || !isfinite(limit) ||
        !isfinite(squared)) {
        return ab_fail(error, "Sling delta exceeds finite range");
    }
    float length = sqrtf(squared);
    if (limit < length) {
        float factor = limit / length;
        result.x *= factor;
        result.y *= factor;
    }
    if (!isfinite(result.x) || !isfinite(result.y)) {
        return ab_fail(error, "Sling delta result exceeds finite range");
    }
    *delta = result;
    return true;
}

bool ab_slingshot_delta_reset(AbVec2 *delta, bool enabled) {
    if (!delta) {
        return false;
    }
    if (enabled) {
        *delta = (AbVec2){0};
    }
    return true;
}

static bool valid_band(const AbSlingBandState *band) {
    return band && isfinite(band->rest.x) && isfinite(band->rest.y) &&
           isfinite(band->rubber_band.x) && isfinite(band->rubber_band.y) &&
           isfinite(band->angle) && isfinite(band->distance) &&
           isfinite(band->wobble) && isfinite(band->previous_distance);
}

bool ab_slingshot_reset_band(AbSlingBandState *band, AbSlingControlHost host,
                             void *context, AbError *error) {
    if (!valid_band(band) || !host) {
        return ab_fail(error, "Invalid slingshot band reset");
    }
    band->rubber_band = band->rest;
    band->angle = 0;
    band->distance = 0;
    band->wobble = 0;
    band->previous_distance = 0;
    AbSlingControlCall call = {.operation = AB_SLING_CONTROL_QUERY_LASER};
    if (!host(context, &call, error)) {
        return false;
    }
    if (!call.audio_playing) {
        band->scope_sound_eligible = true;
    }
    return true;
}

bool ab_slingshot_cancel_drag(AbSlingBandState *band, AbShotState *shot,
                              AbSlingControlHost host, void *context, AbError *error) {
    if (!valid_band(band) || !shot || !host ||
        (shot->dragged && !shot->dragged->name)) {
        return ab_fail(error, "Invalid slingshot drag cancellation");
    }
    if (shot->dragged) {
        AbSlingControlCall call = {.operation = AB_SLING_CONTROL_SET_POSITION,
                                   .bird = shot->dragged,
                                   .position = band->rest};
        if (!host(context, &call, error)) {
            return false;
        }
        shot->dragged = NULL;
        shot->selection_played = false;
    }
    return ab_slingshot_reset_band(band, host, context, error);
}
