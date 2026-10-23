#ifndef AB_CLASSIC_PLAYFIELD_H
#define AB_CLASSIC_PLAYFIELD_H

#include "ab/classic_launch_host.h"
#include "ab/classic_trajectories.h"

typedef struct AbClassicPlayfield AbClassicPlayfield;

typedef struct {
    size_t visuals, scene, trajectories, camera, behaviors, shots, services, sling;
    size_t storage_bytes, name_capacity, extra_renderers, tracks, points, notifications;
    size_t behavior_objects;
    size_t shot_objects;
} AbClassicPlayfieldLimits;

typedef struct {
    AbSlingshotStartPose pose;
    float physics_scale, default_force, current_bird_index;
    bool fill_in_next_bird, allow_reset_to_bird_camera, disabled_for_current_level;
} AbClassicPlayfieldStart;

typedef struct {
    AbClassicLevel *level;
    const AbSpriteProvider *sprites;
    AbAudioBank *audio;
    AbSettingsStatistics *statistics;
    AbEventManager *events;
    const AbPowerupUses *powerups;
    AbClassicGameGlobals *globals;
    AbShotState *shot_state;
    AbDataValue *flying_object;
    const AbDataValue *other_birds;
    const AbDataValue *powerups_enabled, *red_special_available, *next_bird_sound;
    uint32_t *renderer_counter;
    AbBlend object_blend;
    const AbMaskTexture *terrain;
    /* Source resources and callbacks remain borrowed. level/storage_bytes are
     * selected by this constructor from level/limits above. */
    AbClassicCameraConfig camera;
    AbClassicPlayfieldStart start;
    AbClassicPlayfieldLimits limits;
    void *context;
    void (*missing_visual)(void *context, size_t object, AbSpan name);
    AbShotHost level_hook, definition_hook;
    AbClassicShotTrails shot_trails;
} AbClassicPlayfieldConfig;

typedef struct {
    AbClassicLevel *level;
    AbClassicVisuals *visuals;
    AbClassicRenderScene *scene;
    AbClassicTrajectories *trajectories;
    AbClassicCamera *camera;
    AbClassicBehaviors *behaviors;
    AbClassicShots *shots;
    AbShotServices *services;
    AbClassicSling *sling;
} AbClassicPlayfieldView;

/* Production ownership assembly for an already loaded ordinary Museum level.
 * Own eight real components at stable addresses; share actual camera/trajectory
 * behavior services and the launch host, without fixture callbacks. The level,
 * sprites, camera resource sources, globals, RNG/audio, events/statistics, raw
 * flags and counter remain borrowed until close. Initial behavior tables must
 * be unbound. Open constructs visuals/scene, trajectories, camera, behaviors,
 * generic launch services, shots and sling in that dependency order.
 *
 * This chooses no game-global defaults, seed, menu, listener registration or
 * source level-loading policy. Prepare the actual source values, including the
 * shot controller state obtained from the view, before gameplay. Missing named
 * level/catalog hooks fail when reached. Other component domain limits apply.
 * Museum legacy and Blues trajectory classes share the real trajectory owner.
 * Blues reads live flying_object and optional other_birds at their source
 * boundaries; a reached child lookup requires that actual table, without an
 * empty substitute. Split-body creation remains separate, as do full
 * simulation/root/input/menu/GPU scheduling.
 *
 * Sum all component byte budgets and this owner under storage_bytes. Temporary
 * workspaces retain their component contracts; bytes() returns that reserved
 * bound, not resident-memory measurement. Failure returns NULL and frees partial
 * owned storage. Published level fields, consumed renderer IDs and any reached
 * source init effects can survive failure: discard the borrowing level. Drain
 * event/process observers before close; retained tables expire at close. Close
 * before the level/resources, and do not continue using its published views.
 */
bool ab_classic_playfield_open(AbClassicPlayfieldConfig config,
                               AbClassicPlayfield **output, AbError *error);
void ab_classic_playfield_close(AbClassicPlayfield *playfield);
size_t ab_classic_playfield_bytes(const AbClassicPlayfield *playfield);
AbClassicPlayfieldView ab_classic_playfield_view(const AbClassicPlayfield *playfield);

/* Bind the actual canonical otherBirds slot after its owner is constructed.
 * Its address stays borrowed through behavior dispatches; replace the value
 * through that owner, without rebinding this pointer. NULL unbinds and retains
 * the existing explicit error on a reached missing child lookup. Call only
 * between dispatches. No source effects, state defaults or allocation. */
bool ab_classic_playfield_bind_other_birds(AbClassicPlayfield *playfield,
                                           const AbDataValue *field, AbError *error);

#endif
