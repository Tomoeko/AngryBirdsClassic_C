#ifndef AB_BIRD_SHOCKWAVE_H
#define AB_BIRD_SHOCKWAVE_H

#include "ab/lua_data.h"
#include "ab/physics_geometry.h"

typedef struct {
    float duration, delay, radius, initial_radius;
    float force, minimum_force, force_dropoff;
    float damage, minimum_damage;
    bool impulse, damage_from_definition;
} AbBirdShockwaveDefinition;

typedef struct {
    AbBirdShockwaveDefinition definition;
    float time, delay_timer, progress, current_radius, force;
    bool delay_present;
} AbBirdShockwave;

typedef struct {
    bool delaying, finished, speed_written, shake_present;
    float physics_speed, overlay_alpha, camera_shake, time_ratio;
} AbBirdShockwavePhase;

typedef struct {
    AbVec2 relative;
    float strength, definition_strength, definition_defence;
    bool bird, shot, same_wave, live, definition_strength_present;
} AbBirdShockwaveObject;

typedef struct {
    bool inside, break_joints, mark_damaged, inflict, impulse, force;
    float damage, score;
    AbVec2 vector;
} AbBirdShockwaveImpact;

/* Numeric policy recovered from BlackBirdAbility.lua trigger4246/update7598
 * and apply5721 in the pinned APK. Copy finite actual shockwaveAbility fields;
 * retain numeric-zero truthiness and the source defaults for optional fields.
 * The supported domain has positive duration and a dropoff below one. */
bool ab_bird_shockwave_begin(const AbDataTable *definition, AbBirdShockwave *output,
                             AbError *error);
/* The delay-crossing call returns without advancing active time. During the
 * active phase, compute radius/force from pre-decrement time, then decrement and
 * report completion. The enclosing owner must run ElectricShockwave.update
 * before this boundary, apply impacts before clearing, and own all registries,
 * particles, audio, overlay, camera and callbacks. No effects or allocation. */
bool ab_bird_shockwave_advance(AbBirdShockwave *wave, float seconds,
                               AbBirdShockwavePhase *output, AbError *error);
/* One live dynamic-list candidate. Exclude unshot birds before geometry reads;
 * use a strict radius test, fixed break-force request, one damage/impulse per
 * wave name for nonbirds, and continuous unnormalized force for shot birds too.
 * Caller supplies source base-definition fields, canonical identity tests and
 * current physical strength. Vectors remain in script units. No mutation. */
bool ab_bird_shockwave_impact(const AbBirdShockwave *wave,
                              const AbBirdShockwaveObject *object,
                              AbBirdShockwaveImpact *output, AbError *error);

#endif
