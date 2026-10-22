#ifndef AB_AMBIENT_ANIMATION_H
#define AB_AMBIENT_ANIMATION_H

#include "ab/base.h"
#include "ab/random.h"

typedef struct {
    const AbSpan *sprites;
    size_t count;
    float speed, delay, min_delay, max_delay;
    bool delay_present;
    float timer;
    bool timer_present;
} AbAmbientAnimationVariant;

typedef struct {
    size_t current;
    float delay_timer, delay_duration;
    AbSpan original_sprite;
    bool current_present, original_sprite_present;
} AbAmbientAnimationState;

typedef enum {
    AB_AMBIENT_ANIMATION_WAITING,
    AB_AMBIENT_ANIMATION_PLAYING,
    AB_AMBIENT_ANIMATION_FINISHED,
} AbAmbientAnimationResult;

typedef struct {
    void *context;
    bool (*set_sprite)(void *context, AbSpan sprite, bool present, AbError *error);
} AbAmbientAnimationHost;

/* Start one resolved AmbientAnimations sequence: select a variant using the
 * original two-argument Lua integer random boundary, then choose its explicit
 * numeric delay (including zero) or consume one unit draw for min/max delay.
 * Reset that variant's timer and capture the current object's sprite. Other
 * variants retain their state. Dense variants and nil/string sprites only;
 * immutable sprite arrays/string bytes remain borrowed. No allocation or Lua.
 * Invalid used data preserves state/variants; already consumed RNG is retained.
 */
bool ab_ambient_animation_start(AbAmbientAnimationState *state,
                                AbAmbientAnimationVariant *variants, size_t count,
                                AbSpan object_sprite, bool sprite_present,
                                AbRandom48 *random, AbError *error);

/* Advance the selected variant. A delay-crossing call only advances the delay;
 * it does not spend the remainder on animation. Otherwise select ceil(timer *
 * speed), restore the captured sprite on an absent frame, or set a changed
 * frame. A restoration is emitted even when already showing that sprite.
 * State changes precede sprite callbacks. FINISHED leaves current_present set:
 * the enclosing object-map update clears it after this helper returns true.
 * WAITING corresponds to the original nil return; PLAYING to false. Finite used
 * arithmetic is required. Late errors retain preceding state/host changes.
 */
bool ab_ambient_animation_advance(AbAmbientAnimationState *state,
                                  AbAmbientAnimationVariant *variants, size_t count,
                                  float seconds, AbSpan object_sprite,
                                  bool sprite_present, AbAmbientAnimationHost host,
                                  AbAmbientAnimationResult *result, AbError *error);

#endif
