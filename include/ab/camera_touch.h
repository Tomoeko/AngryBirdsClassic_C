#ifndef AB_CAMERA_TOUCH_H
#define AB_CAMERA_TOUCH_H

#include "ab/camera_drag.h"
#include "ab/camera_system.h"

typedef struct {
    int32_t id;
    AbVec2 position;
} AbCameraTouch;

typedef struct {
    /* Script-root shared state, explicitly shared by all associated handlers. */
    float tap_count;
} AbCameraTouchShared;

typedef enum {
    AB_CAMERA_TOUCH_IDLE,
    AB_CAMERA_TOUCH_DRAG,
    AB_CAMERA_TOUCH_ZOOM,
} AbCameraTouchState;

typedef struct {
    AbCameraTouchState state;
    AbCameraDrag drag;
    int32_t first_id;
    int32_t second_id;
    float timer;
    AbVec2 latest_tap_position;
    bool latest_tap_position_present;
    bool detect_tap;
    float zoom_velocity;
    float start_zoom;
    float start_distance;
    float previous_distance;
    float previous_diff;
    bool initialized;
} AbCameraTouchHandler;

typedef struct {
    /* Borrow a unique-ID snapshot in the original filtered table's traversal
     * order. Platform mapping, menu filtering and native table order are caller
     * boundaries. Numeric IDs are restricted to exactly represented integers. */
    const AbCameraTouch *touches;
    size_t count;
    bool bird_dragged;
    /* Original gate requires specialty_available OR released_after_specialty,
     * in addition to enabled input and no dragged bird. */
    bool specialty_available;
    bool released_after_specialty;
    bool ingame;
    bool suppress_zoom_snap;
} AbCameraTouchInput;

typedef struct {
    AbCameraTargets targets;
    bool targets_present;
    /* Original FSM packing drops this flag when the target table is absent. */
    bool user_input;
} AbCameraTouchOutput;

/* Caller-owned, allocation-free TouchCameraInputHandler numeric state. Init
 * creates an untapped idle state and resets the explicitly shared tap counter. */
bool ab_camera_touch_init(AbCameraTouchHandler *handler, AbCameraTouchShared *shared);
/* Update the current state, then enter its replacement. Constructor actions may
 * queue automatic camera moves before replacement entry. Returned targets are
 * pre-limit resolved input: pass them and user_input to camera_system_update in
 * the same frame, before any other camera update. The supplied camera owns all
 * automatic/sequence/mover state; its published viewport supplies drag distance
 * conversion. No physics, input polling, allocation, resources or Lua executes.
 * Finite seconds/touches and finite used geometry are required. Pinch zoom may
 * be nonfinite and is deliberately left for camera input limits to clamp.
 * Failure can retain preceding state/actions; stop or discard the controller. */
bool ab_camera_touch_update(AbCameraTouchHandler *handler, AbCameraTouchShared *shared,
                            AbCameraSystem *camera, float seconds,
                            const AbCameraTouchInput *input,
                            AbCameraTouchOutput *output, AbError *error);

#endif
