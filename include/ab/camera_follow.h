#ifndef AB_CAMERA_FOLLOW_H
#define AB_CAMERA_FOLLOW_H

#include "ab/bird_impact_prediction.h"
#include "ab/camera_limits.h"
#include "ab/camera_mover.h"

typedef struct {
    AbVec2 position, launch_velocity;
    bool launch_present;
} AbCameraFollowBird;

typedef struct {
    float approach_duration, start_zoom, approach_change;
    float hold_duration, peak_zoom, finish_zoom;
} AbCameraFollowZoom;

typedef struct {
    AbCameraValues values;
    float movement_duration, zoom_duration;
    AbCameraFollowZoom zoom_curve;
    bool zoom_duration_present, custom_zoom, custom_y;
} AbCameraFollowTargets;

typedef enum {
    AB_CAMERA_FOLLOW_PREDICT = 1,
    AB_CAMERA_FOLLOW_TRANSFORM,
    AB_CAMERA_FOLLOW_PROJECT,
    AB_CAMERA_FOLLOW_ZOOM,
    AB_CAMERA_FOLLOW_SNAP,
    AB_CAMERA_FOLLOW_VALIDATE,
    AB_CAMERA_FOLLOW_EXTENTS,
    AB_CAMERA_FOLLOW_ZOOM_FIT,
    AB_CAMERA_FOLLOW_SEQUENCE,
    AB_CAMERA_FOLLOW_SCREEN_X,
} AbCameraFollowOperation;

typedef enum {
    AB_CAMERA_FOLLOW_INITIAL,
    AB_CAMERA_FOLLOW_PEAK,
    AB_CAMERA_FOLLOW_PEAK_EXTENTS,
    AB_CAMERA_FOLLOW_ZOOM_START,
    AB_CAMERA_FOLLOW_ZOOM_RETURN,
    AB_CAMERA_FOLLOW_FINAL,
    AB_CAMERA_FOLLOW_LIVE_Y,
    AB_CAMERA_FOLLOW_WAIT,
} AbCameraFollowSite;

typedef struct {
    AbCameraFollowOperation operation;
    AbCameraFollowSite site;
    const AbCameraFollowBird *bird;
    AbCameraFollowTargets targets;
    AbVec2 point;
    float zoom;
    /* Output fields: prediction; vector for transform/project/snap; values
     * for validation; number for zoom/extents/screen-X; snap_id for snap. */
    AbBirdImpactPrediction prediction;
    AbVec2 vector;
    AbCameraValues values;
    float number, snap_id;
} AbCameraFollowCall;

typedef struct {
    void *context;
    float gravity;
    bool (*query)(void *context, AbCameraFollowCall *call, AbError *error);
} AbCameraFollowHost;

typedef struct {
    const AbCameraFollowBird *target;
    AbVec2 hit_position;
    float hit_time, original_zoom, time_to_peak, scroll_timer;
    float timer;
    float movement_duration_scaler, velocity_compensation, minimum_movement_duration;
    bool hit_present, hit_time_present, original_zoom_present, time_to_peak_present;
    bool scroll_present, peak_is_hit_position, peak_flag_present, allow_cancel;
} AbCameraFollow;

typedef struct {
    AbCameraFollowTargets targets;
    bool targets_present, done;
} AbCameraFollowResult;

/* FollowBirdCameraMovementBehavior numeric state. Target and host remain
 * borrowed and may change at query boundaries. An absent compensation selects
 * binary32 0.7; numeric zero remains zero. These init constants are retained
 * even where the original update does not read them. No allocation or VM. */
bool ab_camera_follow_init(AbCameraFollow *state, const AbCameraFollowBird *target,
                           const float *compensation, AbError *error);

/* Original update and captured-tween semantics over typed numeric properties
 * and ordinary original helper bindings. Services execute in original order;
 * prediction names are unused. VALIDATE always represents the original true
 * force-zoom argument; an owned CameraSystem binding must preserve its wrapper's
 * separately recovered argument-dropping policy. SEQUENCE denotes the literal
 * FollowShotBirdEndCameraSequence. Missing launch data fails when reached.
 * Earlier state/host effects survive failure; result commits only on success.
 * Host/context/storage stay alive, without reentry, until the call returns.
 * This does not own a camera, physics world, script classes or bird lifetime. */
bool ab_camera_follow_update(AbCameraFollow *state, float seconds, bool user_input,
                             AbCameraFollowHost *host, AbCameraFollowResult *output,
                             AbError *error);

/* The original zoom closure ignores supplied start/change and captures six
 * numbers. The outer duration affects only its final return segment. Existing
 * finite cosine/cubic tween contracts apply. Output commits on success. */
bool ab_camera_follow_zoom_tween(const AbCameraFollowZoom *curve, float time,
                                 float duration, float *output, AbError *error);
/* The original Y closure ignores every tween argument, querying live screen X,
 * projected Y and closest valid values instead. Prior host effects survive a
 * later failure; output commits only on success. */
bool ab_camera_follow_y_tween(AbCameraFollowHost *host, float *output, AbError *error);

#endif
