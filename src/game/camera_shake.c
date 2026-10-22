#include "ab/camera_shake.h"

#include <math.h>

static bool valid(const AbCameraShake *state) {
    return state && !(state->present & ~UINT32_C(7)) &&
           (!(state->present & AB_CAMERA_SHAKE_DURATION) ||
            isfinite(state->duration)) &&
           (!(state->present & AB_CAMERA_SHAKE_AMOUNT) || isfinite(state->amount)) &&
           (!(state->present & AB_CAMERA_SHAKE_AMPLITUDE) ||
            isfinite(state->amplitude));
}

bool ab_camera_shake_init(AbCameraShake *state) {
    if (!state) {
        return false;
    }
    *state = (AbCameraShake){.present = AB_CAMERA_SHAKE_AMPLITUDE};
    return true;
}

bool ab_camera_shake_add(AbCameraShake *state, float duration, float amount) {
    if (!state || !isfinite(duration) || !isfinite(amount)) {
        return false;
    }
    state->duration = duration;
    state->amount = amount;
    state->present |= AB_CAMERA_SHAKE_DURATION | AB_CAMERA_SHAKE_AMOUNT;
    return true;
}

bool ab_camera_shake_remove(AbCameraShake *state) {
    return ab_camera_shake_add(state, 0, 0);
}

bool ab_camera_shake_update(AbCameraShake *state, float seconds, AbRandom48 *random) {
    if (!valid(state)) {
        return false;
    }
    AbCameraShake next = *state;
    if (next.present & AB_CAMERA_SHAKE_DURATION) {
        if (next.duration > 0) {
            next.duration = next.duration - seconds;
            if (!isfinite(seconds) || !isfinite(next.duration)) {
                return false;
            }
            if (!(next.present & AB_CAMERA_SHAKE_AMPLITUDE) || next.amplitude <= 0) {
                next.amplitude = next.amount;
                next.present =
                    (next.present & ~(uint32_t)AB_CAMERA_SHAKE_AMPLITUDE) |
                    ((next.present & AB_CAMERA_SHAKE_AMOUNT) ? AB_CAMERA_SHAKE_AMPLITUDE
                                                             : 0);
            }
        } else {
            next.present &= ~(uint32_t)AB_CAMERA_SHAKE_DURATION;
            next.amplitude = 0;
            next.present |= AB_CAMERA_SHAKE_AMPLITUDE;
        }
    }
    next.offset = (AbVec2){0};
    if (!(next.present & AB_CAMERA_SHAKE_AMPLITUDE) || next.amplitude == 0) {
        *state = next;
        return true;
    }
    float magnitude = fabsf(next.amplitude);
    if (magnitude >= 0x1p30f || !random) {
        return false;
    }
    int32_t bound = (int32_t)magnitude;
    AbRandom48 drawn = *random;
    float x, y;
    if (ab_script_random(&drawn, 2, -bound, bound, &x) != AB_SCRIPT_RANDOM_OK ||
        ab_script_random(&drawn, 2, -bound, bound, &y) != AB_SCRIPT_RANDOM_OK) {
        return false;
    }
    next.offset = (AbVec2){floorf(x), floorf(y)};
    *state = next;
    *random = drawn;
    return true;
}
