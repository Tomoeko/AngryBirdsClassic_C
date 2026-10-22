#ifndef AB_BIRD_ANIMATION_H
#define AB_BIRD_ANIMATION_H

#include "ab/base.h"

typedef struct {
    float timer;
    bool active;
} AbBirdAnimation;

typedef enum {
    AB_BIRD_FACE_BASE = 1,
    AB_BIRD_FACE_YELL,
    AB_BIRD_FACE_BLINK,
} AbBirdFace;

typedef struct {
    void *context;
    bool (*random_integer)(void *context, int minimum, int maximum, int *output);
    bool (*camera_distance)(void *context, float *output, AbError *error);
    bool (*play_yell)(void *context, float volume, AbError *error);
    bool (*set_sprite)(void *context, AbBirdFace face, AbError *error);
    bool yell_sprite_present;
    bool blink_sprite_present;
    bool eagle_bait_launched;
} AbBirdAnimationControls;

/* Ordinary unshot-bird sprite prefix of updateCharacterAnimations. eligible
 * resolves the enclosing shot/overrideDrawing/animTimer gates; active resolves
 * literal true. Sprite presence means non-nil, rather than truthiness. Random
 * callbacks retain the native float wrapper's possible maximum+1 result.
 * camera_distance supplies the original sling-start-to-camera distance boundary;
 * play_yell performs sound selection and playback before any sprite change.
 * Missing optional face sprites still start the active interval. Jump/hat work
 * follows this phase separately. Native timer writes and callbacks occur in
 * order; callbacks may observe/mutate state before the final active-flag write.
 * No allocation occurs. Finite used timing/distance is required. Initial numeric
 * validation preserves state; later failure can retain partial state/RNG/effects.
 * Numeric results require nearest rounding and gradual underflow.
 * An ineligible bird retains state without using seconds or controls. */
bool ab_bird_animation_update(AbBirdAnimation *state, float seconds, bool eligible,
                              const AbBirdAnimationControls *controls, AbError *error);

#endif
