#include "ab/camera_mover.h"
#include "ab/script_math.h"

#include <math.h>

bool ab_camera_mover_reset(AbCameraMover *mover) {
    if (!mover) {
        return false;
    }
    mover->current_position = (AbVec2){0, 0};
    mover->target_position = (AbVec2){0, 0};
    mover->current_zoom = 1;
    mover->target_zoom = 1;
    mover->zoom_velocity = 0;
    mover->movement_duration = 0;
    mover->movement_timer = 0;
    mover->zoom_duration = 0;
    mover->zoom_timer = 0;
    mover->target_position_present = true;
    mover->target_zoom_present = true;
    return true;
}

bool ab_camera_mover_init(AbCameraMover *mover) {
    if (!mover) {
        return false;
    }
    *mover = (AbCameraMover){0};
    return ab_camera_mover_reset(mover);
}

bool ab_camera_mover_clear_targets(AbCameraMover *mover) {
    if (!mover) {
        return false;
    }
    mover->target_position_present = false;
    mover->target_zoom_present = false;
    return true;
}

bool ab_camera_mover_tweens(AbCameraMover *mover, AbCameraTween movement,
                            AbCameraTween zoom) {
    if (!mover || (unsigned)movement > AB_CAMERA_TWEEN_SMOOTHSTEP ||
        (unsigned)zoom > AB_CAMERA_TWEEN_SMOOTHSTEP) {
        return false;
    }
    mover->movement_tween = movement;
    mover->zoom_tween = zoom;
    return true;
}

bool ab_camera_mover_position(AbCameraMover *mover, AbVec2 target, float duration) {
    if (!mover || !isfinite(target.x) || !isfinite(target.y) || !isfinite(duration) ||
        (duration > 0 && (!isfinite(mover->current_position.x) ||
                          !isfinite(mover->current_position.y)))) {
        return false;
    }
    mover->target_position = target;
    mover->target_position_present = true;
    mover->movement_timer = 0;
    mover->movement_duration = duration > 0 ? duration : 0;
    if (duration <= 0) {
        mover->current_position = target;
    }
    mover->start_position = mover->current_position;
    mover->start_position_present = true;
    return true;
}

bool ab_camera_mover_zoom(AbCameraMover *mover, float target, float duration) {
    if (!mover || !isfinite(target) || !isfinite(duration) ||
        (duration > 0 && !isfinite(mover->current_zoom))) {
        return false;
    }
    mover->target_zoom = target;
    mover->target_zoom_present = true;
    mover->zoom_timer = 0;
    mover->zoom_duration = duration > 0 ? duration : 0;
    if (duration > 0) {
        mover->start_zoom_scale = mover->current_zoom;
        mover->start_zoom_scale_present = true;
    } else {
        mover->current_zoom = target;
        mover->start_zoom = target;
        mover->start_zoom_present = true;
    }
    return true;
}

bool ab_camera_tween_cubic(float time, float start, float change, float duration,
                           float *output) {
    if (!output || !isfinite(time) || !isfinite(start) || !isfinite(change) ||
        !isfinite(duration)) {
        return false;
    }
    float half_duration = duration / 2.0f;
    if (half_duration == 0) {
        return false;
    }
    float progress = time / half_duration;
    bool first_half = progress < 1.0f;
    if (!first_half) {
        progress = progress - 2.0f;
    }
    float half_change = change / 2.0f;
    float curve = progress * progress;
    curve = curve * progress;
    if (!first_half) {
        curve = curve + 2.0f;
    }
    float result = half_change * curve;
    result = result + start;
    if (!isfinite(progress) || !isfinite(curve) || !isfinite(result)) {
        return false;
    }
    *output = result;
    return true;
}

static bool advance_timer(float timer, float seconds, float duration, float *output) {
    if (!isfinite(timer) || !isfinite(seconds) || !isfinite(duration)) {
        return false;
    }
    float next = timer + seconds;
    if (!isfinite(next)) {
        return false;
    }
    *output = duration < next ? duration : next;
    return true;
}

bool ab_camera_tween(AbCameraTween tween, float time, float start, float change,
                     float duration, float *output) {
    if (tween == AB_CAMERA_TWEEN_CUBIC_IN_OUT) {
        return ab_camera_tween_cubic(time, start, change, duration, output);
    }
    if (!output || !isfinite(start) || !isfinite(change) ||
        (unsigned)tween > AB_CAMERA_TWEEN_SMOOTHSTEP) {
        return false;
    }
    float result;
    if (tween == AB_CAMERA_TWEEN_CUBIC_OUT) {
        if (!isfinite(time) || !isfinite(duration) || duration == 0) {
            return false;
        }
        float progress = time / duration;
        progress = progress - 1.0f;
        float curve = progress * progress;
        curve = curve * progress;
        curve = curve + 1.0f;
        result = change * curve;
        result = result + start;
        if (!isfinite(progress) || !isfinite(curve)) {
            return false;
        }
    } else if (tween == AB_CAMERA_TWEEN_COSINE_IN_OUT) {
        if (!isfinite(time) || !isfinite(duration) || duration == 0) {
            return false;
        }
        float factor = -change / 2.0f;
        float phase = 0x1.921fb6p+1f * time;
        phase = phase / duration;
        float cosine;
        if (!ab_script_cosine(phase, &cosine)) {
            return false;
        }
        float curve = cosine - 1.0f;
        result = factor * curve;
        result = result + start;
    } else if (tween == AB_CAMERA_TWEEN_SMOOTHSTEP) {
        float progress = time / duration;
        float curve;
        if (!ab_camera_smoothstep(0, 1, progress, &curve)) {
            return false;
        }
        result = change * curve;
        result = result + start;
    } else {
        result = start + change;
    }
    if (!isfinite(result)) {
        return false;
    }
    *output = result;
    return true;
}

static bool evaluate_curve(const AbCameraMoverCurves *curves, AbCameraAxis axis,
                           AbCameraTween builtin, float time, float start, float change,
                           float duration, float *output, AbError *error) {
    if (curves && (curves->axes & (1u << (unsigned)axis))) {
        if (!curves->evaluate || !curves->evaluate(curves->context, axis, time, start,
                                                   change, duration, output, error)) {
            return false;
        }
        return isfinite(*output) || ab_fail(error, "Nonfinite custom camera curve");
    }
    return ab_camera_tween(builtin, time, start, change, duration, output);
}

bool ab_camera_mover_update_curves(AbCameraMover *mover, float seconds,
                                   const AbCameraMoverCurves *curves, AbError *error) {
    if (!mover || (curves && (curves->axes & ~UINT32_C(7)))) {
        return false;
    }
    AbCameraMover next = *mover;
    if (next.movement_duration > 0 && next.target_position_present) {
        if (!next.start_position_present ||
            !advance_timer(next.movement_timer, seconds, next.movement_duration,
                           &next.movement_timer) ||
            !evaluate_curve(curves, AB_CAMERA_AXIS_X, next.movement_tween,
                            next.movement_timer, next.start_position.x,
                            next.target_position.x - next.start_position.x,
                            next.movement_duration, &next.current_position.x, error) ||
            !evaluate_curve(curves, AB_CAMERA_AXIS_Y, next.movement_tween,
                            next.movement_timer, next.start_position.y,
                            next.target_position.y - next.start_position.y,
                            next.movement_duration, &next.current_position.y, error)) {
            return false;
        }
    }
    if (next.zoom_duration > 0 && next.target_zoom_present) {
        if (!next.start_zoom_scale_present ||
            !advance_timer(next.zoom_timer, seconds, next.zoom_duration,
                           &next.zoom_timer) ||
            !evaluate_curve(curves, AB_CAMERA_AXIS_ZOOM, next.zoom_tween,
                            next.zoom_timer, next.start_zoom_scale,
                            next.target_zoom - next.start_zoom_scale,
                            next.zoom_duration, &next.current_zoom, error)) {
            return false;
        }
    }
    *mover = next;
    return true;
}

bool ab_camera_mover_update(AbCameraMover *mover, float seconds) {
    return ab_camera_mover_update_curves(mover, seconds, NULL, NULL);
}
