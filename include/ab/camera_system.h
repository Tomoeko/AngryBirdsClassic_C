#ifndef AB_CAMERA_SYSTEM_H
#define AB_CAMERA_SYSTEM_H

#include "ab/camera_mover.h"
#include "ab/camera_limits.h"
#include "ab/camera_sequence.h"
#include "ab/camera_follow.h"

typedef struct AbCameraSystem AbCameraSystem;

typedef struct {
    AbCameraValues values;
    float movement_duration;
    float zoom_duration;
    /* When any numeric target or selector is supplied, absent movement selects
     * cubic in/out and absent zoom inherits the resolved movement selection. */
    AbCameraTween movement_tween;
    AbCameraTween zoom_tween;
    bool movement_tween_present;
    bool zoom_tween_present;
} AbCameraTargets;

typedef struct {
    AbCameraTargets targets;
    AbVec2 shake;
    bool targets_present;
    bool user_input;
    bool moving_objects;
    bool specialty_available;
} AbCameraSystemInput;

typedef struct {
    const AbCameraMover *mover;
    const AbCameraLimits *limits;
    const AbCameraSequenceManager *sequence;
    /* NULL until the first successful viewport publication. */
    const AbCameraViewport *viewport;
    AbVec2 pixel_size;
    AbVec2 saved_toggle_position;
    bool saved_toggle_position_present;
    bool automatic_present;
    bool input_enabled;
    AbVec2 screen_position;
    const AbCameraFollow *follow;
    const AbCameraFollowTargets *follow_curves;
} AbCameraSystemView;

typedef struct {
    float duration;
    AbCameraTween movement_tween;
    AbCameraTween zoom_tween;
    bool duration_present;
    bool movement_tween_present;
    bool zoom_tween_present;
} AbCameraMove;

typedef bool (*AbCameraSystemDone)(void *context, AbCameraSystem *system,
                                   AbError *error);
typedef bool (*AbCameraSystemFollowResolver)(void *context, AbDataValue target,
                                             const AbCameraFollowBird **bird,
                                             AbCameraFollowHost **environment,
                                             AbError *error);

/* Own stable camera/controller state; snaps remain borrowed until close.
 * Initialize the native zero center/unit zoom mover and enabled default limits.
 * allocation_limit bounds this owner. Pixel dimensions and limit configuration
 * are explicit caller selections. Output starts NULL, with full failure cleanup.
 * This does not load data, select an input handler, generate shake or call a GPU. */
bool ab_camera_system_open(const AbCameraSnap *snaps, size_t count, AbVec2 pixel_size,
                           float absolute_bottom, float horizontal_leeway,
                           size_t allocation_limit, AbCameraSystem **output,
                           AbError *error);
void ab_camera_system_close(AbCameraSystem *system);
AbCameraSystemView ab_camera_system_view(const AbCameraSystem *system);
/* Read coordinate extents without advancing movement or sequences. Before the
 * first native publication, derive them from the source constructor's screen,
 * mover zoom, pixel size and zero shake. Later reads retain the published view.
 * This supports input queries before the first controller camera update. */
bool ab_camera_system_query_viewport(const AbCameraSystem *system,
                                     AbCameraViewport *output);
/* Application drawable-size boundary. Recompute the default enforcer from its
 * current borrowed snaps and new dimensions, preserving movement, sequences,
 * following and the lock flag. Invalidate the old viewport until the next real
 * update publishes one; constructor-style coordinate queries remain available.
 * Call between frames, after refreshing any resolution-dependent snap data.
 * This is geometry rebinding, not an original resize-event sequence. */
bool ab_camera_system_resize(AbCameraSystem *system, AbVec2 pixel_size, AbError *error);

/* Queue one automatic move without changing the active sequence. Snap indices
 * are zero based; use_snap_zoom selects the snap's zoom rather than a literal
 * value. Absent duration is zero for snaps and distance/1000 for positions.
 * Partial position values retain current axes/zoom. Snap validation corrects
 * position only. Later queued moves replace earlier unconsumed moves. */
bool ab_camera_system_goto_snap(AbCameraSystem *system, size_t index,
                                bool use_snap_zoom, bool validate,
                                const AbCameraMove *move, AbError *error);
bool ab_camera_system_goto_position(AbCameraSystem *system,
                                    const AbCameraValues *values,
                                    const AbCameraMove *move, AbError *error);
/* Toggle relative zoom at the strict half threshold, saving current position
 * when going far and consuming it when going close. A supplied position takes
 * priority over the saved position. Optional input constraints precede hard
 * position validation. Queue a half-second cubic-out position/zoom move. */
bool ab_camera_system_toggle_zoom(AbCameraSystem *system, const AbVec2 *position,
                                  bool constrain_input, AbError *error);
bool ab_camera_system_input_enabled(AbCameraSystem *system, bool enabled);
/* CameraSystem.setMovementTweenFunction forwards to CameraMover's two builtin
 * selections. Preserve timers, positions, targets and automatic behaviors. */
bool ab_camera_system_movement_tweens(AbCameraSystem *system, AbCameraTween movement,
                                      AbCameraTween zoom);
/* Original stopMovement: drop automaticBehavior, then reset mover targets.
 * Preserve current pose, timers, viewport and the active sequence. */
bool ab_camera_system_stop_movement(AbCameraSystem *system);

/* Own one follow behavior over a borrowed target and environment. Null target
 * clears the current automatic behavior. Absent allow_cancel selects true;
 * numeric compensation uses the follow initializer's default when absent.
 * The environment supplies PREDICT, TRANSFORM and SEQUENCE; geometry/zoom/snap
 * queries bind the owned camera. Supplied snaps use initialized one-based IDs
 * in their array order. The environment remains alive until the behavior ends.
 * Query callbacks must not replace the follow behavior or free/reenter this
 * owner during an update. Target removal clears only the automatic behavior;
 * already captured mover curves retain their original independent lifetime. */
bool ab_camera_system_follow_bird(AbCameraSystem *system,
                                  const AbCameraFollowBird *target,
                                  const bool *allow_cancel, const float *compensation,
                                  AbCameraFollowHost *environment, AbError *error);
const AbCameraFollowBird *ab_camera_system_target_bird(const AbCameraSystem *system);
bool ab_camera_system_remove_bird(AbCameraSystem *system,
                                  const AbCameraFollowBird *bird);
/* Resolve a sequence followTarget identifier/value against actual game state.
 * Null resolver clears the binding. The resolver borrows target and returns
 * a borrowed bird/environment (null bird clears following). Context remains
 * alive until unbound or close. It must not free/reenter this owner. Ordinary
 * game-state changes are allowed; compensation is read after resolution. */
bool ab_camera_system_bind_follow(AbCameraSystem *system, void *context,
                                  AbCameraSystemFollowResolver resolver);

/* Start an already prepared borrowed sequence. Builtin conditions and ordinary
 * wait/lock/unlock/input-enable/position/snap actions with default cubic tweens
 * are supported. Numeric nonzero velocity derives duration from the previous
 * published screen position, captures the strict timer condition and replaces
 * stage duration. Velocity position moves require both coordinates. Named
 * custom tweens, object movement and custom conditions reject explicitly.
 * followTarget uses the registered resolver, interrupt flag and optional
 * numeric velocityCompensation. Done runs before manager reset;
 * Snap zoom is a truthiness selector; snap validation defaults true. Position
 * moves retain missing axes/current zoom, default absent duration to distance
 * divided by 1000, and do not pass validation into their automatic behavior.
 * callbacks must not free the owner or reenter its update. Source/stages and
 * callback context stay stable until the sequence ends or this owner closes. */
bool ab_camera_system_start(AbCameraSystem *system, AbCameraSequence *sequence,
                            AbCameraSystemDone done, void *context, AbError *error);
/* Original cancelCameraSequence calls endSequence, including the current done
 * callback before reset. Automatic behavior and mover curves are independent. */
bool ab_camera_system_cancel_sequence(AbCameraSystem *system, AbError *error);
/* Input handling is an explicit resolved boundary. Apply input limits, update
 * the sequence, update the active automatic behavior (whose result replaces
 * input targets even when absent), set mover targets, update movement, then
 * publish shake-adjusted
 * viewport state. The original closest-values wrapper drops its extra force-
 * zoom argument: automatic validation checks position without clamping zoom.
 * Partial X/Y targets move only when both fields are present after limiting.
 * input_enabled reports the requested handler state; supplied resolved input
 * must already reflect that handler's policy. No allocation, resource loading,
 * physics, audio or original code executes unless an explicit follow environment
 * resolves those game services.
 * Later failures retain preceding changes; discard or stop that controller. */
bool ab_camera_system_update(AbCameraSystem *system, float seconds,
                             const AbCameraSystemInput *input, AbError *error);

#endif
