#ifndef AB_CLASSIC_BIRD_COLLISIONS_H
#define AB_CLASSIC_BIRD_COLLISIONS_H

#include "ab/classic_scores.h"

#include "ab/classic_block_collisions.h"
#include "ab/bird_collision.h"
#include "ab/floating_scores.h"

typedef struct AbClassicBirdCollisions AbClassicBirdCollisions;

typedef struct {
    AbClassicLevel *level;
    AbClassicVisuals *visuals;
    AbClassicBehaviors *behaviors;
    AbAudioBank *audio;
    AbFloatingScores *floating_scores;
    AbBlockEventObject *script_objects;
    size_t script_object_count;
    /* These are the actual shared game registries/state, not snapshots. Hat map
     * handles point to stable AbBirdCollisionHat records. Bird scores use fixed
     * initial object indices; block and bird callbacks share the append list. */
    AbStringMap *hats;
    AbBirdCollisionGrenades *grenades;
    AbBirdCollisionScoreEntry *bird_scores;
    size_t bird_score_count;
    AbBlockCollisionScores *scores;
    AbDataValue *flying_object, *specialty_available;
    const AbDataValue *delta_based_slingshot, *achievement_processor;
    const float *floating_score_scaling, *sound_force_threshold;
    AbStringOrderWorkspace *sprite_workspace;
    void *context;
    bool (*level_hook)(void *context, size_t first, size_t second,
                       const AbBlockCollisionNotice *notice, AbError *error);
    bool (*named)(void *context, AbDataValue name, size_t first, size_t second,
                  const AbBlockCollisionNotice *notice, AbError *error);
    bool (*achievement)(void *context, size_t first, size_t second, AbError *error);
    bool (*cooldown)(void *context, AbError *error);
    bool (*mighty)(void *context, size_t object, AbSpan first, AbSpan second,
                   AbError *error);
    AbBirdCollisionPrint print;
    bool (*binding)(void *context, AbDataValue key, AbUiPointerValue *output,
                    AbError *error);
    bool (*specialty)(void *context, AbUiPointerValue function,
                      const size_t *flying_object,
                      const AbBirdCollisionNamedInput *contact, AbError *error);
    bool (*release_hat)(void *context, AbSpan name, AbError *error);
    bool (*contact_particles)(void *context, size_t object, AbDataValue descriptor,
                              AbError *error);
    AbAmbientParticleHost damage_particles;
    size_t storage_bytes;
    AbClassicScores *root_scores;
} AbClassicBirdCollisionConfig;

/* Bind every recovered bird-callback phase to the live initial level registry,
 * attached scripts, owned behavior dispatch, visuals, audio and floating scores.
 * Own bounded stable adapter/definition rows only. Borrow all config storage and
 * source strings until close. Open has no game effects. Calls allocate nothing.
 *
 * Publish changed physical fields and shared specialty/score state before each
 * service/lookup, and refresh after successful services. Retain entry objects and
 * immutable catalog definitions through later rebinding. Support initial fixed
 * world identities and immutable catalog tables; dynamic objects, renames and
 * custom gameObjectScript receivers reject when reached. Missing required
 * services fail at their actual boundary, preserving preceding game effects.
 * Achievement processor and delta-sling presence are explicit live values.
 * No empty hats/grenades or absent achievement state is invented here.
 *
 * The callback does not compute contact damage, remove bodies, emit particles
 * without their host, step physics or schedule frames. The Museum contact owner
 * supplies damage and calls this direct-compatible service. Serialize calls,
 * stop the session after failure and close before all borrowed owners. External
 * services may mutate canonical fields and shared registries, but must retain
 * storage and must not reenter this owner. */
bool ab_classic_bird_collisions_open(AbClassicBirdCollisionConfig config,
                                     AbClassicBirdCollisions **output, AbError *error);
void ab_classic_bird_collisions_close(AbClassicBirdCollisions *collisions);
size_t ab_classic_bird_collisions_bytes(const AbClassicBirdCollisions *collisions);
/* Stable identity for binding the actual flyingGrenades list. Do not edit this
 * projection; canonical fields refresh at call/service boundaries. */
AbBirdCollisionObject *
ab_classic_bird_collisions_object(AbClassicBirdCollisions *collisions, size_t object);
bool ab_classic_bird_collisions_call(void *context,
                                     const AbBirdCollisionNotifyInput *input,
                                     AbError *error);

#endif
