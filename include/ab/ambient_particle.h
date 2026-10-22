#ifndef AB_AMBIENT_PARTICLE_H
#define AB_AMBIENT_PARTICLE_H

#include "ab/base.h"
#include "ab/physics_geometry.h"
#include "ab/random.h"

typedef struct {
    AbSpan particles;
    AbVec2 offset;
    float amount;
    float timer, delay, min_delay, max_delay;
    bool timer_present, delay_present;
} AbAmbientParticle;

typedef struct {
    AbSpan particles;
    AbVec2 position;
    float amount;
} AbAmbientParticleEmission;

typedef struct {
    void *context;
    /* NULL uses the original fixed physics-to-world point transform (x20). */
    bool (*physics_to_world)(void *context, AbVec2 physics_position,
                             AbVec2 *world_position, AbError *error);
    /* Original trailing arguments are three numeric zeros and two false flags. */
    bool (*emit)(void *context, const AbAmbientParticleEmission *emission,
                 AbError *error);
} AbAmbientParticleHost;

/* Resolved single-object AmbientParticles loop, with physics enabled and world
 * membership already checked. Visit the dense copied descriptors in ipairs
 * order. Absent/false timers choose explicit numeric delay (zero is present) or
 * one shared unit draw; subtract seconds once and emit at most once per call.
 * Clear the timer before offset rotation, world conversion and emission. Rotate
 * using Lua's rounded double trig results; read object position for each effect.
 * Convert through the recovered coordinate helper unless a host override is supplied.
 * The caller supplies the original body-angle query's resolved value. Used trig
 * is bounded to |angle| <= 1024; all used math must be finite. Callbacks may edit
 * later descriptors/position, whose storage remains stable. No allocation, Lua,
 * particle simulation, membership or map ordering occurs here. Late failures
 * retain preceding timer/random/host effects.
 */
bool ab_ambient_particles_update(AbAmbientParticle *particles, size_t count,
                                 float seconds, float body_angle,
                                 const AbVec2 *object_position, AbRandom48 *random,
                                 AbAmbientParticleHost host, AbError *error);

#endif
