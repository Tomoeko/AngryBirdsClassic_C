#ifndef AB_CAMERA_SHAKE_H
#define AB_CAMERA_SHAKE_H

#include "ab/physics_geometry.h"
#include "ab/random.h"

enum {
    AB_CAMERA_SHAKE_DURATION = 1,
    AB_CAMERA_SHAKE_AMOUNT = 2,
    AB_CAMERA_SHAKE_AMPLITUDE = 4,
};

typedef struct {
    float duration;
    float amount;
    float amplitude;
    AbVec2 offset;
    /* Numeric fields versus absent/nil fields; offsets always exist. */
    uint32_t present;
} AbCameraShake;

/* CameraSystem's numeric shake fields and globals, isolated as caller-owned
 * state. Init has no explosion duration/amount and zero amplitude/offset.
 * Add replaces duration/amount; remove writes both zero. Neither changes the
 * current amplitude/offset until update. No allocation or implicit RNG seed. */
bool ab_camera_shake_init(AbCameraShake *state);
bool ab_camera_shake_add(AbCameraShake *state, float duration, float amount);
bool ab_camera_shake_remove(AbCameraShake *state);

/* Positive explosion duration subtracts seconds without clamping, then fills
 * absent/nonpositive amplitude from amount. A duration already nonpositive
 * clears itself and writes zero amplitude. Each active nonzero amplitude draws
 * X then Y through original math.random integer intervals and floors results;
 * fractional amounts below one still consume both draws. Existing positive
 * amplitude persists. Random is required only for a draw and remains shared
 * explicitly with other consumers. Numeric fields must be finite; active
 * absolute amplitude must be below 2^30. Used seconds must be finite. Invalid
 * input preserves state/RNG; original type/error side effects are not modeled.
 * Caller schedules this separately from camera movement/publication. */
bool ab_camera_shake_update(AbCameraShake *state, float seconds, AbRandom48 *random);

#endif
