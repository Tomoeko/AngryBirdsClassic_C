#ifndef AB_CLASSIC_POWERUP_PARTICLES_H
#define AB_CLASSIC_POWERUP_PARTICLES_H

#include "ab/classic_birds.h"

/* Owned named ParticleSpawner records shared by King Sling and Super Seeds.
 * Borrow canonical objects/birds, shared RNG, physics gate and particle engine.
 * Preserve source hash iteration, removal gates, random bubble intervals,
 * immediate trails and one emission per update. Reserve all storage at open.
 * Potion launches own deep-copied particle descriptors until close; they never
 * mutate the shared catalog. Close before the particle engine and level. */
typedef struct AbClassicPowerupParticles AbClassicPowerupParticles;
typedef enum {
    AB_POWERUP_PARTICLES_KING,
    AB_POWERUP_PARTICLES_POTION,
    AB_POWERUP_PARTICLES_BUBBLES,
} AbPowerupParticlesKind;

bool ab_classic_powerup_particles_open(AbClassicLevel *level, AbClassicBirds *birds,
                                       AbClassicParticles *particles,
                                       AbRandom48 *random, const bool *physics_enabled,
                                       size_t bird_capacity, size_t storage_bytes,
                                       AbClassicPowerupParticles **output,
                                       AbError *error);
void ab_classic_powerup_particles_close(AbClassicPowerupParticles *owner);
bool ab_classic_powerup_particles_add(AbClassicPowerupParticles *owner, size_t object,
                                      AbPowerupParticlesKind kind, AbError *error);
bool ab_classic_powerup_particles_update(AbClassicPowerupParticles *owner,
                                         float seconds, AbError *error);

#endif
