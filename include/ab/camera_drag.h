#ifndef AB_CAMERA_DRAG_H
#define AB_CAMERA_DRAG_H

#include "ab/coordinates.h"

enum { AB_CAMERA_DRAG_SAMPLES = 5 };

typedef struct {
    AbVec2 start;
    AbVec2 anchor;
    AbVec2 current_screen;
    AbVec2 old_screen;
    AbVec2 velocity[AB_CAMERA_DRAG_SAMPLES];
    size_t count;
    bool initialized;
    bool dragging;
} AbCameraDrag;

typedef struct {
    AbVec2 screen_position;
    AbVec2 normalized_velocity;
} AbCameraDragRelease;

/* Numeric DragHandler state. Empty construction has no numeric fields/buffer;
 * start writes start/current/old touch positions, the world anchor and an empty
 * five-sample buffer. All storage is caller-owned; no allocation or table/vector
 * identity. Start inputs must be finite. */
bool ab_camera_drag_init(AbCameraDrag *state);
bool ab_camera_drag_start(AbCameraDrag *state, AbVec2 touch, AbVec2 anchor);
bool ab_camera_drag_is_dragging(const AbCameraDrag *state);
/* Append then drop the oldest sample when the fixed buffer exceeds five.
 * Initial start is required. IEEE velocity components, including NaNs and
 * infinities, retain their stored representation. */
bool ab_camera_drag_sample(AbCameraDrag *state, AbVec2 velocity);
/* Sum oldest to newest, negate, divide by count, then divide X/Y by the logical
 * screen dimensions separately. An empty initialized buffer yields target quiet
 * NaN on both axes. Buffer add/divide uses recovered ARM NaN selection and
 * quieting, preserving sign/payload. Dimensions must be finite and nonzero;
 * normalized results may be nonfinite. Floating status flags are outside this
 * contract. Failure preserves output. */
bool ab_camera_drag_velocity(const AbCameraDrag *state, AbVec2 pixel_size,
                             AbVec2 *output);
/* Save old/current screen positions and append per-second screen velocity.
 * Convert displacement from the original touch using the recovered distance
 * transform, including translation-dependent rounding, then subtract that world
 * displacement from the original world anchor. Finite seconds, an active drag,
 * finite touch positions and finite coordinate/target arithmetic are required.
 * Zero/negative time and nonfinite velocity propagation are retained. Original
 * type-error and callback mutations require separate recovery.
 * Failure preserves state/output. */
bool ab_camera_drag_update(AbCameraDrag *state, float seconds, AbVec2 touch,
                           const AbCoordinateCamera *camera, AbVec2 *target);
/* Return current screen position and average velocity, clear start/anchor field
 * presence, preserve current/old positions and the sample buffer. No camera snap
 * selection, tween, input filtering or surrounding touch state machine runs. */
bool ab_camera_drag_release(AbCameraDrag *state, AbVec2 pixel_size,
                            AbCameraDragRelease *output);

#endif
