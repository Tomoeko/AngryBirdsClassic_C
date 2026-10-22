#ifndef AB_OBJECT_FORCE_STEP_H
#define AB_OBJECT_FORCE_STEP_H

#include "ab/object_forces.h"
#include "ab/object_force_sound.h"

typedef enum {
    AB_FORCE_EFFECT_SPRITE = 1,
    AB_FORCE_EFFECT_AUDIO,
} AbObjectForceEffect;

typedef struct {
    AbObjectForceEffect effect;
    AbSpan name;
    float gain;
} AbObjectForceCall;

typedef bool (*AbObjectForceHost)(void *context, const AbObjectForceCall *call,
                                  AbError *error);

typedef struct {
    const AbObjectForces *forces;
    /* Current world gravity; forces->gravity is ignored by this controller. */
    AbVec2 gravity;
    /* Optional canonical GameObject gravity flag; otherwise use forces' value. */
    const bool *gravity_enabled;
    AbObjectForceInterval *interval;
    const AbObjectForceSound *sound;
    AbRandom48 *random;
    AbObjectForceHost host;
    void *context;
} AbObjectForceStep;

/* Compose GameLua's recovered per-object force-adder phases over resolved state.
 * Read disable once, apply gravity cancellation, then an optional numeric
 * interval. Sprite/original-sprite changes precede SPRITE; pulse sound selection
 * follows that callback. Publish the timer after resource requests, then read
 * configured force/target values and apply its center impulse. Without an
 * interval, apply continuous forces. NULL sound represents an absent field.
 * Required effects need a host; an explicit no-op host may represent an absent
 * renderable. AUDIO uses the force-sound plan's fixed playback parameters.
 *
 * State/pointer lifetimes remain stable. Host changes to existing force/sound
 * values are read at their later phase; the initial disable decision and local
 * timer arithmetic remain captured. The caller owns body/world locking and
 * actual sprite/audio resource binding. A later failure retains earlier body,
 * metadata, host and RNG changes; stop/discard that update. No allocation or
 * original code executes. */
bool ab_object_force_step(AbBodyMotion *body, const AbObjectForceStep *step,
                          float seconds, AbError *error);

#endif
