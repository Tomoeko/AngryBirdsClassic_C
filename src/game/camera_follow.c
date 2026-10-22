#include "ab/camera_follow.h"
#include "script_float_internal.h"

#include <math.h>

static float add(float first, float second) {
    return script_float_binary(first, second, AB_SCRIPT_FLOAT_ADD);
}

static float subtract(float first, float second) {
    return script_float_binary(first, second, AB_SCRIPT_FLOAT_SUBTRACT);
}

static float multiply(float first, float second) {
    return script_float_binary(first, second, AB_SCRIPT_FLOAT_MULTIPLY);
}

static float divide(float first, float second) {
    return script_float_binary(first, second, AB_SCRIPT_FLOAT_DIVIDE);
}

static bool query(AbCameraFollowHost *host, AbCameraFollowCall *call, AbError *error) {
    return (host && host->query) ? host->query(host->context, call, error)
                                 : ab_fail(error, "Missing follow-camera service");
}

static bool vector_query(AbCameraFollowHost *host, AbCameraFollowOperation operation,
                         AbCameraFollowSite site, AbVec2 point, AbVec2 *output,
                         AbError *error) {
    AbCameraFollowCall call = {.operation = operation, .site = site, .point = point};
    if (!query(host, &call, error)) {
        return false;
    }
    *output = call.vector;
    return true;
}

static bool number_query(AbCameraFollowHost *host, AbCameraFollowOperation operation,
                         AbCameraFollowSite site, AbVec2 point, float *output,
                         AbError *error) {
    AbCameraFollowCall call = {.operation = operation, .site = site, .point = point};
    if (!query(host, &call, error)) {
        return false;
    }
    *output = call.number;
    return true;
}

static bool project(AbCameraFollowHost *host, AbCameraFollowSite site, float x,
                    float *y, AbError *error) {
    AbVec2 result;
    if (!vector_query(host, AB_CAMERA_FOLLOW_PROJECT, site, (AbVec2){x, 0}, &result,
                      error)) {
        return false;
    }
    *y = result.y;
    return true;
}

static bool validate(AbCameraFollowHost *host, AbCameraFollowSite site,
                     AbCameraFollowTargets targets, AbCameraValues *output,
                     AbError *error) {
    AbCameraFollowCall call = {
        .operation = AB_CAMERA_FOLLOW_VALIDATE, .site = site, .targets = targets};
    if (!query(host, &call, error)) {
        return false;
    }
    *output = call.values;
    return true;
}

bool ab_camera_follow_init(AbCameraFollow *state, const AbCameraFollowBird *target,
                           const float *compensation, AbError *error) {
    if (!state || !target) {
        return ab_fail(error, "Missing follow-camera state or target");
    }
    *state = (AbCameraFollow){.target = target,
                              .movement_duration_scaler = 0x1.0624dep-10f,
                              .velocity_compensation =
                                  compensation ? *compensation : 0x1.666666p-1f,
                              .minimum_movement_duration = 0x1.333334p-2f,
                              .allow_cancel = true};
    return true;
}

static bool launch(const AbCameraFollow *state, AbError *error) {
    return (state->target && state->target->launch_present) ||
           ab_fail(error, "Missing follow-camera launch velocity");
}

static bool prepare_zoom(AbCameraFollow *state, AbCameraFollowHost *host, AbVec2 peak,
                         float peak_clock, AbCameraFollowTargets *targets,
                         AbError *error) {
    if (!number_query(host, AB_CAMERA_FOLLOW_ZOOM, AB_CAMERA_FOLLOW_ZOOM_START,
                      (AbVec2){0}, &state->original_zoom, error)) {
        return false;
    }
    state->original_zoom_present = true;
    state->time_to_peak = peak_clock;
    state->time_to_peak_present = true;
    float first_fit;
    if (!number_query(host, AB_CAMERA_FOLLOW_ZOOM_FIT, AB_CAMERA_FOLLOW_PEAK, peak,
                      &first_fit, error)) {
        return false;
    }
    targets->values.zoom = state->original_zoom;
    targets->zoom_duration = add(state->hit_time, 1);
    targets->zoom_duration_present = true;
    float approach = state->peak_is_hit_position ? targets->zoom_duration
                                                 : multiply(peak_clock, 0x1.666666p-1f);
    float hold =
        state->peak_is_hit_position
            ? 0
            : multiply(subtract(targets->zoom_duration, approach), 0x1.99999ap-4f);
    AbCameraFollowZoom curve = {.approach_duration = approach,
                                .start_zoom = state->original_zoom,
                                .approach_change =
                                    subtract(first_fit, state->original_zoom),
                                .hold_duration = hold,
                                .peak_zoom = first_fit,
                                .finish_zoom = state->original_zoom};
    AbVec2 hit_offset = {add(state->hit_position.x, 0), add(state->hit_position.y, 50)};
    float return_fit;
    if (!number_query(host, AB_CAMERA_FOLLOW_ZOOM_FIT, AB_CAMERA_FOLLOW_ZOOM_RETURN,
                      hit_offset, &return_fit, error)) {
        return false;
    }
    if (return_fit < curve.finish_zoom) {
        curve.finish_zoom = return_fit;
    }
    targets->values.zoom = curve.finish_zoom;
    targets->zoom_curve = curve;
    targets->custom_zoom = targets->custom_y = true;
    return true;
}

static bool prepare_peak(AbCameraFollow *state, AbCameraFollowHost *host,
                         AbCameraFollowTargets *targets, AbError *error) {
    float gravity = host->gravity;
    const AbCameraFollowBird *bird = state->target;
    float apex_time = divide(-bird->launch_velocity.y, gravity);
    /* The original uses target.y as the base of both apex coordinates. */
    float apex_x = add(bird->position.y, multiply(bird->launch_velocity.x, apex_time));
    float apex_y = add(bird->position.y, multiply(bird->launch_velocity.y, apex_time));
    apex_y =
        add(apex_y, multiply(multiply(0.5f, gravity), multiply(apex_time, apex_time)));
    AbVec2 peak;
    float peak_clock = apex_time;
    if (apex_time < state->hit_time) {
        if (!vector_query(host, AB_CAMERA_FOLLOW_TRANSFORM, AB_CAMERA_FOLLOW_PEAK,
                          (AbVec2){apex_x, apex_y}, &peak, error)) {
            return false;
        }
    } else {
        state->peak_is_hit_position = true;
        state->peak_flag_present = true;
        peak = state->hit_position;
        peak_clock = state->hit_time;
    }
    peak.x = add(peak.x, 0);
    peak.y = add(peak.y, 50);
    /* Only the hit-position branch replaces the apex time with hitTime. */
    float projected;
    if (!project(host, AB_CAMERA_FOLLOW_PEAK, peak.x, &projected, error)) {
        return false;
    }
    AbCameraFollowTargets partial = {
        .values = {.position = {0, projected}, .present = AB_CAMERA_VALUE_Y}};
    AbCameraValues valid;
    if (!validate(host, AB_CAMERA_FOLLOW_PEAK, partial, &valid, error)) {
        return false;
    }
    float zoom;
    if (!number_query(host, AB_CAMERA_FOLLOW_ZOOM, AB_CAMERA_FOLLOW_PEAK_EXTENTS,
                      (AbVec2){0}, &zoom, error)) {
        return false;
    }
    AbCameraFollowCall extents = {.operation = AB_CAMERA_FOLLOW_EXTENTS,
                                  .site = AB_CAMERA_FOLLOW_PEAK_EXTENTS,
                                  .point = {peak.x, valid.position.y},
                                  .zoom = zoom};
    if (!query(host, &extents, error)) {
        return false;
    }
    return !(peak.y < extents.number) ||
           prepare_zoom(state, host, peak, peak_clock, targets, error);
}

static bool initial_targets(AbCameraFollow *state, AbCameraFollowHost *host,
                            AbCameraFollowTargets *targets, AbError *error) {
    AbVec2 converted;
    if (!vector_query(host, AB_CAMERA_FOLLOW_TRANSFORM, AB_CAMERA_FOLLOW_INITIAL,
                      state->hit_position, &converted, error)) {
        return false;
    }
    state->hit_position = converted;
    float y;
    if (!project(host, AB_CAMERA_FOLLOW_INITIAL, state->hit_position.x, &y, error)) {
        return false;
    }
    *targets = (AbCameraFollowTargets){
        .values = {.position = {state->hit_position.x, y}, .present = 7},
        .movement_duration = multiply(state->hit_time, 0x1.ccccccp-1f)};
    if (!number_query(host, AB_CAMERA_FOLLOW_ZOOM, AB_CAMERA_FOLLOW_INITIAL,
                      (AbVec2){0}, &targets->values.zoom, error)) {
        return false;
    }
    AbCameraFollowCall snap = {.operation = AB_CAMERA_FOLLOW_SNAP,
                               .site = AB_CAMERA_FOLLOW_INITIAL,
                               .targets = *targets};
    if (!query(host, &snap, error)) {
        return false;
    }
    float difference = subtract(snap.vector.x, targets->values.position.x);
    if (fabsf(difference) < 1000 && snap.snap_id != 1) {
        targets->values.position = snap.vector;
    }
    if (!launch(state, error)) {
        return false;
    }
    if (state->target->launch_velocity.y < 0 &&
        !prepare_peak(state, host, targets, error)) {
        return false;
    }
    AbCameraFollowTargets copy = *targets;
    if (!number_query(host, AB_CAMERA_FOLLOW_ZOOM, AB_CAMERA_FOLLOW_FINAL, (AbVec2){0},
                      &copy.values.zoom, error)) {
        return false;
    }
    AbCameraValues valid;
    if (!validate(host, AB_CAMERA_FOLLOW_FINAL, copy, &valid, error)) {
        return false;
    }
    targets->values.position = valid.position;
    return true;
}

bool ab_camera_follow_update(AbCameraFollow *state, float seconds, bool user_input,
                             AbCameraFollowHost *host, AbCameraFollowResult *output,
                             AbError *error) {
    if (!state || !output) {
        return ab_fail(error, "Missing follow-camera state or result");
    }
    AbCameraFollowResult result = {0};
    if (user_input) {
        result.done = true;
        *output = result;
        return true;
    }
    if (!state->hit_present) {
        if (!state->target) {
            return ab_fail(error, "Missing follow-camera target");
        }
        if (state->target->launch_present) {
            AbCameraFollowCall prediction = {.operation = AB_CAMERA_FOLLOW_PREDICT,
                                             .site = AB_CAMERA_FOLLOW_INITIAL,
                                             .bird = state->target};
            if (!query(host, &prediction, error)) {
                return false;
            }
            state->hit_time = prediction.prediction.time;
            state->hit_time_present = true;
            state->hit_present = prediction.prediction.point_present;
            if (!state->hit_present) {
                result.done = true;
                *output = result;
                return true;
            }
            state->hit_position = prediction.prediction.point;
            if (!initial_targets(state, host, &result.targets, error)) {
                return false;
            }
            result.targets_present = true;
            *output = result;
            return true;
        }
    }
    if (state->hit_present) {
        if (!launch(state, error)) {
            return false;
        }
        if (state->target->launch_velocity.x > 0) {
            if (!state->scroll_present && !state->hit_time_present) {
                return ab_fail(error, "Missing follow-camera hit time");
            }
            state->scroll_timer = state->scroll_present
                                      ? subtract(state->scroll_timer, seconds)
                                      : state->hit_time;
            state->scroll_present = true;
            if (state->scroll_timer <= 0) {
                AbCameraFollowCall sequence = {.operation = AB_CAMERA_FOLLOW_SEQUENCE,
                                               .site = AB_CAMERA_FOLLOW_WAIT};
                if (!query(host, &sequence, error)) {
                    return false;
                }
            }
        }
    }
    *output = result;
    return true;
}

bool ab_camera_follow_zoom_tween(const AbCameraFollowZoom *curve, float time,
                                 float duration, float *output, AbError *error) {
    if (!curve || !output) {
        return ab_fail(error, "Missing captured follow-camera zoom curve");
    }
    float value;
    bool okay;
    if (time <= curve->approach_duration) {
        okay =
            ab_camera_tween(AB_CAMERA_TWEEN_COSINE_IN_OUT, time, curve->start_zoom,
                            curve->approach_change, curve->approach_duration, &value);
    } else if (curve->approach_duration <= time &&
               time <= add(curve->approach_duration, curve->hold_duration)) {
        value = curve->peak_zoom;
        okay = true;
    } else {
        okay = ab_camera_tween(
            AB_CAMERA_TWEEN_CUBIC_IN_OUT,
            subtract(subtract(time, curve->approach_duration), curve->hold_duration),
            curve->peak_zoom, subtract(curve->finish_zoom, curve->peak_zoom),
            subtract(subtract(duration, curve->approach_duration),
                     curve->hold_duration),
            &value);
    }
    if (!okay) {
        return ab_fail(error, "Unsupported captured follow-camera zoom evaluation");
    }
    *output = value;
    return true;
}

bool ab_camera_follow_y_tween(AbCameraFollowHost *host, float *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing follow-camera Y curve result");
    }
    float x, y;
    if (!number_query(host, AB_CAMERA_FOLLOW_SCREEN_X, AB_CAMERA_FOLLOW_LIVE_Y,
                      (AbVec2){0}, &x, error) ||
        !project(host, AB_CAMERA_FOLLOW_LIVE_Y, x, &y, error)) {
        return false;
    }
    AbCameraFollowTargets partial = {
        .values = {.position = {0, y}, .present = AB_CAMERA_VALUE_Y}};
    AbCameraValues valid;
    if (!validate(host, AB_CAMERA_FOLLOW_LIVE_Y, partial, &valid, error)) {
        return false;
    }
    *output = valid.position.y;
    return true;
}
