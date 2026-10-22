#include "ab/bird_animation.h"

#include <math.h>

static bool draw_integer(const AbBirdAnimationControls *controls, int minimum,
                         int maximum, float *output, AbError *error) {
    int value;
    if (!controls || !controls->random_integer ||
        !controls->random_integer(controls->context, minimum, maximum, &value) ||
        value < minimum || (int64_t)value > (int64_t)maximum + 1) {
        ab_fail(error, "Invalid bird animation random boundary");
        return false;
    }
    *output = (float)value;
    return true;
}

static bool set_face(const AbBirdAnimationControls *controls, AbBirdFace face,
                     AbError *error) {
    if (!controls->set_sprite) {
        return ab_fail(error, "Missing bird animation sprite boundary");
    }
    return controls->set_sprite(controls->context, face, error);
}

static bool yell_audio(const AbBirdAnimationControls *controls, AbError *error) {
    float distance;
    if (!controls->camera_distance) {
        return ab_fail(error, "Missing bird animation camera-distance boundary");
    }
    if (!controls->camera_distance(controls->context, &distance, error)) {
        return false;
    }
    if (!isfinite(distance)) {
        return ab_fail(error, "Nonfinite bird animation camera distance");
    }
    float volume = 1.0f - distance / 1000.0f;
    if (volume > 0 && !controls->eagle_bait_launched) {
        if (!controls->play_yell) {
            return ab_fail(error, "Missing bird animation audio boundary");
        }
        return controls->play_yell(controls->context, volume, error);
    }
    return true;
}

bool ab_bird_animation_update(AbBirdAnimation *state, float seconds, bool eligible,
                              const AbBirdAnimationControls *controls, AbError *error) {
    if (!state) {
        return ab_fail(error, "Missing bird animation state");
    }
    if (!eligible) {
        return true;
    }
    if (!isfinite(state->timer) || !isfinite(seconds)) {
        return ab_fail(error, "Nonfinite bird animation timer");
    }
    float timer = state->timer - seconds;
    if (!isfinite(timer)) {
        return ab_fail(error, "Bird animation timer overflow");
    }
    state->timer = timer;
    if (!(timer < 0)) {
        return true;
    }
    float draw;
    if (state->active) {
        if (!draw_integer(controls, 10, 150, &draw, error)) {
            return false;
        }
        state->timer = draw / 100.0f;
        if (!set_face(controls, AB_BIRD_FACE_BASE, error)) {
            return false;
        }
        state->active = false;
        return true;
    }
    if (!draw_integer(controls, 10, 20, &draw, error)) {
        return false;
    }
    state->timer = draw / 100.0f;
    if (!draw_integer(controls, 1, 4, &draw, error)) {
        return false;
    }
    AbBirdFace face = draw == 1 ? AB_BIRD_FACE_YELL : AB_BIRD_FACE_BLINK;
    bool present = face == AB_BIRD_FACE_YELL ? controls->yell_sprite_present
                                             : controls->blink_sprite_present;
    if (face == AB_BIRD_FACE_YELL && !yell_audio(controls, error)) {
        return false;
    }
    if (present && !set_face(controls, face, error)) {
        return false;
    }
    state->active = true;
    return true;
}
