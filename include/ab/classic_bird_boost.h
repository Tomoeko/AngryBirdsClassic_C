#ifndef AB_CLASSIC_BIRD_BOOST_H
#define AB_CLASSIC_BIRD_BOOST_H

#include "ab/classic_particles.h"
#include "ab/classic_trajectories.h"
#include "ab/audio_bank.h"
#include "ab/settings_statistics.h"

typedef struct {
    AbClassicLevel *level;
    AbClassicVisuals *visuals;
    AbClassicTrajectories *trajectories;
    AbClassicParticles *particles;
    AbAudioBank *audio;
    AbSettingsStatistics *settings;
    float force, physics_scale;
} AbClassicBirdBoost;

/* triggerSpecialty125163 instructions43..101, before its common hook/event/
 * behavior tail. Borrow the canonical level, effects and cached definition for
 * this call. Bootstrap supplies actual boostForce/physicsScale. Read cached
 * physical velocity/mass, normalize with utils10029, then apply the native
 * three-argument impulse at the body transform origin. Emit actual boost
 * particles, resolve fresh definition audio, publish spriteSpecial and create
 * the attached trajectory puff in source order. No allocation or RNG resets;
 * earlier tutorial/physics/effect changes survive a later reached failure.
 * Ordinary finite numeric fields and immutable definition constructors only. */
bool ab_classic_bird_boost(const AbClassicBirdBoost *boost, size_t object,
                           const AbDataTable *definition, AbError *error);

#endif
