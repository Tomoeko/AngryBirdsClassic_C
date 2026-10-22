#ifndef AB_CLASSIC_GAME_LEVEL_PARTICLES_H
#define AB_CLASSIC_GAME_LEVEL_PARTICLES_H

#include "ab/particles.h"

typedef struct {
    AbParticles *particles;
    AbDataValue *level_particles;
    const AbDataValue *enabled;
    const bool *physics_enabled;
    float *timer;
    void *context;
    /* Resolved g_extra_game_update function; NULL is explicitly absent. */
    bool (*extra_update)(void *context, float seconds, float time, AbError *error);
} AbClassicGameLevelParticleConfig;

typedef struct {
    AbClassicGameLevelParticleConfig config;
    bool initialized, updating, failed;
} AbClassicGameLevelParticles;

/* Concrete GameUpdate110400 instructions381..503. Borrow the actual mutable
 * objects.levelParticles, its settingsFrame, canonical flags/timer and loaded
 * engine at stable addresses. The selected resource/bootstrap must supply them;
 * initialize chooses no defaults or replacement metadata. Plain string fields,
 * finite numeric values and a string particle definition only. Shared source
 * tables may not be rebound/released during a call. No effects at initialize.
 */
bool ab_classic_game_level_particles_initialize(AbClassicGameLevelParticleConfig config,
                                                AbClassicGameLevelParticles *output,
                                                AbError *error);
/* First-frame burst uses amount*15, y/3 and height0 for ground-start, otherwise
 * the original y as height. Publish firstFrame=false only after emission.
 * Later frames advance the timer and use a strict interval gate plus the actual
 * physics flag, resetting timer before emission. Height nil/false selects0.
 * Then run the resolved extra update, including when particles are disabled.
 * This emits into the real independent level list/RNG; native engine update and
 * rendering stay at their separate root boundaries. No frame allocation.
 * Reentry rejects; late failure retains preceding effects and stops this owner.
 */
bool ab_classic_game_level_particles_update(AbClassicGameLevelParticles *particles,
                                            float seconds, float time, AbError *error);

#endif
