#ifndef AB_PARTICLES_H
#define AB_PARTICLES_H

#include "ab/lua_data.h"
#include "ab/physics_geometry.h"
#include "ab/random.h"

typedef struct AbParticles AbParticles;

typedef struct {
    AbSpan name;
    const AbDataTable *table;
} AbParticleSource;

typedef struct {
    AbSpan definition, sprite;
    AbVec2 position, velocity;
    int32_t layer;
    float angle, angular_velocity, begin_scale, end_scale, scale;
    uintptr_t sprite_handle;
    float time, lifetime;
    uint32_t frame;
    size_t system;
    bool animated;
} AbParticle;

typedef struct {
    int32_t soft_limit, hard_limit;
    float soft_multiplier;
} AbParticleAdmission;

typedef struct {
    const AbParticleSource *sources;
    size_t source_count, capacity, storage_bytes;
    AbEngineRandom *random;
    AbParticleAdmission admission;
    void *context;
    /* Resolve a preloaded ordinary sprite. Zero is the actual missing-resource
     * result and still permits particle storage. Nonzero handles remain borrowed
     * and stable until close. No allocation or resource mutation in this host. */
    bool (*sprite)(void *context, AbSpan name, uintptr_t *handle, AbError *error);
    size_t level_capacity;
} AbParticleConfig;

typedef struct {
    AbSpan definition;
    float amount;
    AbVec2 position, size;
    float angle;
    bool ignore_limits, alternate;
} AbParticleEmission;

/* Own bounded normal/alternate particle lists and lazily prepared definition
 * caches. Borrow immutable source names/tables, engine RNG and sprite host until
 * close. Source tables are the actual packed particle resource globals. No
 * allocation during emit/update. Open has no RNG/resource/game effects. List
 * capacity and dense sprite counts are bounded to 2^24.
 *
 * Initial supported native path: ordinary sprites and box/circle emitters,
 * optional randomized birth position, gravity/scale/angle/lifetime, soft/hard
 * admission, lifetime sprite animation and both list selections. Target emitters,
 * composite sprites and full root draw/alpha policy remain explicit gaps
 * and reject when their definition is reached. This owner does not draw or decode
 * resource files. Limits cover all arrays; resource handles stay host-owned.
 * Stop after a failed emission/update; prior RNG/particle/host effects remain.
 * Serialize calls; the sprite host may not reenter or release borrowed storage. */
bool ab_particles_open(AbParticleConfig config, AbParticles **output, AbError *error);
void ab_particles_close(AbParticles *particles);
size_t ab_particles_bytes(const AbParticles *particles);
AbParticleAdmission *ab_particles_admission(AbParticles *particles);
size_t ab_particles_count(const AbParticles *particles, bool alternate);
/* View is valid until the next list mutation or close. */
const AbParticle *ab_particles_at(const AbParticles *particles, bool alternate,
                                  size_t index);
bool ab_particles_emit(AbParticles *particles, const AbParticleEmission *emission,
                       AbError *error);
/* Original update chooses this same normal/alternate list. Normal particles
 * also expire when their pre-update world y is positive; alternate particles
 * skip that gate. Time advances before strict lifetime removal, followed by
 * semi-implicit velocity/position, scale interpolation and angular motion. */
bool ab_particles_update(AbParticles *particles, float seconds, bool alternate,
                         AbError *error);

/* Original addLevelParticles/updateLevelParticles own a third separate list.
 * Each emission draws one engine double first and selects layer1 if >.5,
 * otherwise layer2, including zero/rejected amounts. Then use the shared normal
 * emitter/admission rules. alternate in the request is unused here. Reserve
 * level_capacity explicitly at open; normal/menu capacity remains independent.
 * Updating uses normal positive-y/lifetime removal. No frame allocation or
 * implicit drawing is supplied. Earlier RNG/effects survive a
 * failure; stop the caller. Used arithmetic must be finite as above.
 */
bool ab_particles_emit_level(AbParticles *particles, const AbParticleEmission *emission,
                             AbError *error);
bool ab_particles_update_level(AbParticles *particles, float seconds, AbError *error);
size_t ab_particles_level_count(const AbParticles *particles);
const AbParticle *ab_particles_level_at(const AbParticles *particles, size_t index);

#endif
