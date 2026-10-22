#include "ab/slingshot.h"
#include "ab/rotation.h"

#include <math.h>

bool ab_slingshot_measure_stretch(AbVec2 rest, AbVec2 band, float maximum_length,
                                  AbSlingStretch *output) {
    if (!output || !isfinite(rest.x) || !isfinite(rest.y) || !isfinite(band.x) ||
        !isfinite(band.y) || !isfinite(maximum_length) || maximum_length <= 0) {
        return false;
    }
    AbVec2 actual = {band.x - rest.x, band.y - rest.y};
    float squared = actual.x * actual.x + actual.y * actual.y;
    if (!isfinite(squared)) {
        return false;
    }
    AbSlingStretch result = {.distance = sqrtf(squared)};
    float stretch = result.distance / maximum_length;
    if (!isfinite(stretch)) {
        return false;
    }
    result.factor = stretch < 0 ? 0 : stretch > 1 ? 1 : stretch;
    *output = result;
    return true;
}

static float vertical_stretchability(float angle) {
    float factor = 1;
    if (angle >= -1.9f && angle < -1.75f) {
        factor = -(angle + 1.75f) / 0.15f;
    } else if (angle >= -1.75f && angle < -1.5f) {
        factor = 0.25f;
    } else if (angle >= -1.5f && angle < -1.35f) {
        factor = (1.5f + angle) / 0.15f;
    }
    return factor < 0.25f ? 0.25f : factor;
}

bool ab_slingshot_drag(const AbSlingDragInput *input, AbSlingDragFrame *output) {
    if (!input || !output || !isfinite(input->rest.x) || !isfinite(input->rest.y) ||
        !isfinite(input->displacement.x) || !isfinite(input->displacement.y) ||
        !isfinite(input->maximum_length) || input->maximum_length <= 0 ||
        !isfinite(input->previous_distance) || !isfinite(input->wobble)) {
        return false;
    }
    AbVec2 delta = input->displacement;
    float squared = delta.x * delta.x + delta.y * delta.y;
    if (!isfinite(squared)) {
        return false;
    }
    float length = sqrtf(squared);
    AbSlingDragFrame result = {.wobble = input->wobble, .selection_played = true};
    if (length > 0 &&
        !ab_script_angle_from_components(delta.y, delta.x, &result.angle)) {
        return false;
    }
    bool inside = length < input->maximum_length;
    float clamped_length = inside ? length : input->maximum_length;
    if (inside) {
        result.rubber_band = (AbVec2){input->rest.x - delta.x, input->rest.y - delta.y};
        float increase = length - input->previous_distance;
        if (!isfinite(increase)) {
            return false;
        }
        if (increase > 0.1f) {
            result.events[result.event_count++] = AB_SLING_DRAG_QUERY_STRETCH;
            if (!input->stretch_audio_playing) {
                result.events[result.event_count++] = AB_SLING_DRAG_PLAY_STRETCH;
            }
        }
    } else {
        result.rubber_band =
            (AbVec2){input->rest.x - (delta.x * input->maximum_length) / length,
                     input->rest.y - (delta.y * input->maximum_length) / length};
        result.wobble = 0;
        if (!input->selection_played) {
            result.events[result.event_count++] = AB_SLING_DRAG_PLAY_STRETCH;
        }
    }
    if (!input->selection_played) {
        result.events[result.event_count++] = AB_SLING_DRAG_PLAY_SELECTION;
    }
    float factor = vertical_stretchability(result.angle);
    float vertical_limit = factor * input->maximum_length;
    if (!isfinite(vertical_limit)) {
        return false;
    }
    if (vertical_limit < clamped_length) {
        float offset = (input->maximum_length * factor) * delta.y;
        result.rubber_band.y = input->rest.y - offset / length;
    }
    AbSlingStretch measured;
    if (!ab_slingshot_measure_stretch(input->rest, result.rubber_band,
                                      input->maximum_length, &measured)) {
        return false;
    }
    result.distance = measured.distance;
    result.stretch_factor = measured.factor;
    *output = result;
    return true;
}
