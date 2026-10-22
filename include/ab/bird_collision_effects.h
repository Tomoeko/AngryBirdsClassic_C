#ifndef AB_BIRD_COLLISION_EFFECTS_H
#define AB_BIRD_COLLISION_EFFECTS_H

#include "ab/bird_collision_notify.h"

typedef struct {
    AbDataValue material_name, collision_sound, damage_sound;
} AbBirdCollisionEffectDefinition;

typedef struct {
    void *context;
    bool (*get_particles)(void *context, AbDataValue *output, AbError *error);
} AbBirdCollisionParticleQuery;

typedef struct {
    AbDataValue x_velocity, y_velocity, mass;
    AbBirdCollisionParticleQuery *script;
} AbBirdCollisionEffectBird;

typedef struct {
    AbSpan first, second;
    bool first_present;
    float force, damage;
    AbBirdCollisionObject *second_object;
    AbBirdCollisionEffectDefinition *first_definition, *second_definition;
} AbBirdCollisionEffectInput;

typedef struct {
    void *context;
    /* Pure lookup of birds[first], with NULL for no bird metadata. */
    bool (*bird)(void *context, AbSpan name, AbBirdCollisionEffectBird **output,
                 AbError *error);
    float threshold;
    bool (*audio_name)(void *context, AbDataValue sound, AbDataValue *output,
                       AbError *error);
    bool (*audio)(void *context, AbAudioPlay request, AbError *error);
    /* addParticlesNew(first, descriptor, false, false), without a receiver. */
    bool (*particles)(void *context, AbSpan first, AbDataValue descriptor,
                      AbError *error);
} AbBirdCollisionEffectHost;

/* Bird callback instructions 827..946. A truthy second sensor or locally absent
 * first name skips. Cache birds[first] and derive momentum only for immovable /
 * staticGround targets with both velocity fields non-nil. Contact force or that
 * momentum strictly above threshold enables target sound, bird particle lookup,
 * then bird sound in this order. Sounds use non-nil gates, including false.
 * Damage > 10 selects a truthy damageSound only when collisionSound is non-nil.
 * Target audio uses volume .7; bird volume uses current mass and damage > 1,
 * clamped by original first-operand min/max semantics. Both use false loop and
 * channel two. Cache each audio binding before its audio-name callback.
 *
 * Plain stable metadata/definitions and original numeric utility semantics only.
 * Hosts may change later fields/bindings, retaining the cached bird identity.
 * Missing/non-table particle query results reject after the query. Finite used
 * velocity/mass/contact inputs; calculated overflow, infinity and default NaN
 * retain original ARM binary32 behavior. No allocation, Lua, actual playback,
 * rendering or particle simulation. Failure retains prior host effects; the
 * separate final damage refresh follows this phase. Do not reenter/free storage.
 */
bool ab_bird_collision_effects(const AbBirdCollisionEffectInput *input,
                               const AbBirdCollisionEffectHost *host, AbError *error);

#endif
