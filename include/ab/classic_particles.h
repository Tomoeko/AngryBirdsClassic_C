#ifndef AB_CLASSIC_PARTICLES_H
#define AB_CLASSIC_PARTICLES_H

#include "ab/classic_level.h"
#include "ab/ambient_particle.h"
#include "ab/particles.h"

typedef struct AbClassicParticles AbClassicParticles;

typedef struct {
    AbSpan *names;
    size_t count, capacity, work_limit;
} AbClassicParticleNames;

/* Append unique particle resource names from the initial canonical objects'
 * resolved particles properties and raw damageStateChangedParticles.name.
 * Support descriptor trees and the legacy string/dense-string-array shape.
 * Borrow returned bytes from the live immutable catalog/physical fields. The
 * caller may first add theme or active ability dependencies to this same list.
 * This is preload metadata collection, not emission, RNG or frame scheduling;
 * it neither guesses names nor promises to cover custom callback emissions.
 * Bounded work/depth and caller storage; partial appends survive failure. */
bool ab_classic_particle_names(AbClassicLevel *level, AbClassicParticleNames *names,
                               AbError *error);

typedef struct {
    AbClassicLevel *level;
    const AbSpan *definitions;
    size_t definition_count, resource_bytes, storage_bytes, capacity;
    AbEngineRandom *engine_random;
    AbRandom48 *script_random;
    AbParticleAdmission admission;
    AbStringOrderWorkspace *workspace;
    void *context;
    bool (*sprite)(void *context, AbSpan name, uintptr_t *handle, AbError *error);
    size_t level_capacity;
    const AbClassicLevelSource *source;
} AbClassicParticleConfig;

/* Preload selected packed particle definitions through the first-party static
 * data reader, then own the particle engine. No Lua VM, original executable
 * code, key files or frame-time resource loading. resource_bytes bounds retained
 * decoded data and each decode stage; storage_bytes covers this owner, file/
 * source rows and engine arrays. Archive is borrowed only during open. Definition
 * name bytes, level, RNGs, workspace and sprite context remain borrowed until
 * close. No startup RNG/emission occurs. Unsupported static constructors reject.
 * Source names select exact assets/data/scripts/particles/<name>.lua resources.
 * Output starts NULL; every partial allocation is released on failure. */
bool ab_classic_particles_open(const AbZip *archive, AbClassicParticleConfig config,
                               AbClassicParticles **output, AbError *error);
void ab_classic_particles_close(AbClassicParticles *particles);
size_t ab_classic_particles_bytes(const AbClassicParticles *particles);
size_t ab_classic_particles_resource_bytes(const AbClassicParticles *particles);
AbParticles *ab_classic_particles_engine(AbClassicParticles *particles);
AbClassicLevel *ab_classic_particles_level(const AbClassicParticles *particles);
/* Borrow the preloaded particle definition; missing names return NULL. No I/O,
 * decoding or mutation. The returned table lives until this owner closes. */
const AbDataTable *ab_classic_particles_definition(const AbClassicParticles *particles,
                                                   AbSpan name);

/* Borrow metadata for runtime deep-copied descriptors. Their entries retain
 * source pairs insertion order and grow from an empty table; hash_capacity is
 * zero to distinguish that construction from the immutable catalog. Bind once
 * between calls, unbind before releasing the owning copied tables. */
bool ab_classic_particles_bind_copies(AbClassicParticles *particles,
                                      const AbLuaData *copies, AbError *error);

/* Actual addParticlesNew(object.name, descriptor, false, false), suitable for
 * the owned bird-contact service. Resolve live Lua-world membership/pose/size,
 * traverse the immutable catalog descriptor in retained constructor hash order,
 * choose explicit amount or the shared Lua random interval, then read the live
 * world's published angle and emit into the native-engine RNG/list owner. Coordinates
 * use the measured x20 conversion. Unknown/rebound descriptors or world identities
 * reject when reached. Earlier random/emission effects survive later failure.
 * No allocation, implicit descriptors, skipped required emissions or drawing. */
bool ab_classic_particles_contact(void *context, size_t object, AbDataValue descriptor,
                                  AbError *error);
/* Named removeBlocks bindings, with the same live-world lookup and geometry.
 * Legacy addParticles accepts a single name or a dense numeric name array;
 * truthy cached definition amounts replace the shared amount for subsequent
 * entries. Missing world members skip before inspecting unused arguments.
 * Definition arrays and names stay borrowed during the call. Both paths retain
 * earlier emissions on failure. Unknown emitters reject rather than omit. */
bool ab_classic_particles_removal(void *context, AbSpan name, AbDataValue descriptor,
                                  bool ignore_limits, bool alternate, AbError *error);
bool ab_classic_particles_legacy(void *context, AbSpan name, AbDataValue definitions,
                                 AbDataValue amount, bool ignore_limits, bool alternate,
                                 AbError *error);
/* Original ambient/damage-state newParticles boundary: the supplied world
 * position/amount followed by three numeric zeros and two false flags. */
bool ab_classic_particles_ambient(void *context,
                                  const AbAmbientParticleEmission *emission,
                                  AbError *error);

#endif
