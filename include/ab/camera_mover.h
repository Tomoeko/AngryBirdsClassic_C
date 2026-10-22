#ifndef AB_CAMERA_MOVER_H
#define AB_CAMERA_MOVER_H

#include "ab/base.h"
#include "ab/physics_geometry.h"

typedef enum {
    AB_CAMERA_TWEEN_CUBIC_IN_OUT,
    AB_CAMERA_TWEEN_CUBIC_OUT,
    AB_CAMERA_TWEEN_END,
    AB_CAMERA_TWEEN_COSINE_IN_OUT,
    AB_CAMERA_TWEEN_SMOOTHSTEP,
} AbCameraTween;

typedef struct {
    AbVec2 current_position;
    AbVec2 target_position;
    AbVec2 start_position;
    float current_zoom;
    float target_zoom;
    float zoom_velocity;
    float start_zoom_scale;
    /* The immediate zoom setter writes startZoom, a different script field. */
    float start_zoom;
    float movement_duration;
    float movement_timer;
    float zoom_duration;
    float zoom_timer;
    bool target_position_present;
    bool target_zoom_present;
    bool start_position_present;
    bool start_zoom_scale_present;
    bool start_zoom_present;
    AbCameraTween movement_tween;
    AbCameraTween zoom_tween;
} AbCameraMover;

/* Default CameraMover numeric state. Init starts with absent optional start
 * fields; reset preserves those existing fields while resetting current/target
 * position, current/target zoom, velocity, durations and timers. */
bool ab_camera_mover_init(AbCameraMover *mover);
bool ab_camera_mover_reset(AbCameraMover *mover);
bool ab_camera_mover_clear_targets(AbCameraMover *mover);
/* Already resolved builtin tween selections. Reset preserves these fields;
 * init selects cubic in/out. Arbitrary Lua functions remain unsupported. */
bool ab_camera_mover_tweens(AbCameraMover *mover, AbCameraTween movement,
                            AbCameraTween zoom);

/* Finite numeric target/duration inputs; the caller resolves nil/false duration
 * to zero. Positive duration starts from current state with timer zero. Other
 * durations snap immediately and store positive-zero timer/duration. Clearing
 * targets preserves all other fields. No vector/table identity is modeled. */
bool ab_camera_mover_position(AbCameraMover *mover, AbVec2 target, float duration);
bool ab_camera_mover_zoom(AbCameraMover *mover, float target, float duration);

/* Recovered default cubic in/out tween with separate binary32 operations.
 * No progress clamp is added. Used intermediates must remain finite and the
 * halved duration must be nonzero. Failure preserves output. */
bool ab_camera_tween_cubic(float time, float start, float change, float duration,
                           float *output);
/* Utils.smoothstep, including math.max(0, math.min(1, ratio)) selection order.
 * All binary32 numeric inputs are supported. Inverted/equal bounds and NaN
 * ratios retain the original clamp behavior; no floating-point flag contract.
 * No allocation. A null output rejects without writing. */
bool ab_camera_smoothstep(float lower, float upper, float value, float *output);
/* Original cubic in/out, cubic out, cosine in/out, smoothstep, or touch zoom's
 * b+c closure. Smoothstep accepts any numeric time/duration (including zero
 * duration); its clamped curve must produce a finite final start/change sum.
 * Start/change and the final result must be finite. End uses only start/change;
 * cubic out divides by duration directly. Other curves require finite used
 * intermediates and nonzero divisors. Cosine uses float
 * pi*time/duration followed by the script double-cosine boundary; its phase
 * must lie within [-1024,1024]. */
bool ab_camera_tween(AbCameraTween tween, float time, float start, float change,
                     float duration, float *output);

/* Update position before zoom with the selected builtin tweens.
 * A phase runs only with positive duration and a present target. Its timer adds
 * seconds then clamps only above duration, retaining negative time. Completed
 * phases stay enabled and reevaluate on later updates. Custom tween functions,
 * automatic behaviors, input, camera limits and callbacks are separate. No
 * allocation; used values/results must be finite. Failure preserves the mover.
 * Unused seconds may be nonfinite when neither phase is active. */
bool ab_camera_mover_update(AbCameraMover *mover, float seconds);

typedef enum {
    AB_CAMERA_AXIS_X,
    AB_CAMERA_AXIS_Y,
    AB_CAMERA_AXIS_ZOOM,
} AbCameraAxis;

typedef struct {
    void *context;
    uint32_t axes;
    bool (*evaluate)(void *context, AbCameraAxis axis, float time, float start,
                     float change, float duration, float *output, AbError *error);
} AbCameraMoverCurves;

/* Resolved first-party curves override the selected axes (1 << axis). Preserve
 * X, Y, then zoom evaluation order and the ordinary mover's unpublished-copy
 * failure behavior. Callback effects survive later failure. Callbacks borrow
 * storage and must not reenter, free or modify the mover. Other axes use their
 * builtin selection. The owner publishes screen coordinates afterward. */
bool ab_camera_mover_update_curves(AbCameraMover *mover, float seconds,
                                   const AbCameraMoverCurves *curves, AbError *error);

#endif
