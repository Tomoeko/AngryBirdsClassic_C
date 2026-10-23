#ifndef AB_CLASSIC_WORLD_RUNTIME_H
#define AB_CLASSIC_WORLD_RUNTIME_H

#include "ab/classic_bird_collisions.h"
#include "ab/classic_block_scripts.h"
#include "ab/classic_dynamics.h"
#include "ab/classic_museum_collisions.h"
#include "ab/classic_playfield.h"

typedef struct AbClassicWorldRuntime AbClassicWorldRuntime;

typedef struct {
    AbClassicPlayfield *playfield;
    AbClassicParticles *particles;
    AbAudioBank *audio;
    AbSettingsStatistics *settings;
    AbEventManager *events;
    AbClassicGameGlobals *globals;
    AbRandom48 *random;
    const bool *physics_enabled;
    /* These retain their actual service contexts, raw flags and numeric
     * settings. Shared owners, attachment rows, maps and score bindings are
     * selected below. Leave those selected fields NULL/zero. */
    AbClassicBlockCollisionConfig blocks;
    AbClassicBirdCollisionConfig birds;
    AbClassicDynamicsConfig dynamics;
    AbClassicMuseumCollisionConfig museum;
    void *script_context;
    bool (*level_physics)(void *context, float seconds, AbError *error);
    size_t map_nodes, map_bytes, score_entries, script_bytes, storage_bytes;
    AbFloatingScoreLimits floating;
    size_t object_capacity;
} AbClassicWorldRuntimeConfig;

typedef struct {
    AbClassicDynamics *dynamics;
    AbClassicBlockScripts *scripts;
    AbClassicBlockCollisions *blocks;
    AbClassicBirdCollisions *birds;
    AbClassicScores *scores;
    AbFloatingScores *floating;
    AbStringMap *dead_blocks, *hats, *teleporters;
    AbBirdCollisionGrenades *grenades;
    AbBlockCollisionScores *block_scores;
    AbBlockRemovalScores *destruction_scores;
    float *bird_bonus;
    const bool *physics_enabled;
    AbDataValue *flying;
    /* Stable physical-table values used by the shared dead-block map. */
    const AbDataValue *physical_values;
    size_t physical_count;
    size_t object_capacity;
} AbClassicWorldRuntimeView;

/* Fresh ordinary Museum world services. Construct the actual fresh death/hat/
 * teleporter registries, grenade list and score records, apply the registered
 * default block classes, and bind both collision routes to the same dynamics,
 * removal, goal and score owners. Reserve later direct-circle identities with
 * object_capacity and publish them through register_circle. Borrow the
 * prepared playfield, particles, shared state and resolved service callbacks.
 * This is construction, not a frame tick or an input/GUI substitute.
 *
 * The default block-class active domains and each component's reached errors
 * remain explicit. Custom sound/level/definition/achievement/particle methods
 * must be supplied where required. No missing service returns success. The
 * selected base dynamics effect must match the caller's actual class selection.
 * Component limits, map storage and owned fixed arrays fit storage_bytes.
 * Source callbacks and observers can retain state until close; drain them first.
 * Close restores the world listener before releasing its borrowed contact rows.
 * Failure after source application requires discarding the borrowing session. */
bool ab_classic_world_runtime_open(AbClassicWorldRuntimeConfig config,
                                   AbClassicWorldRuntime **output, AbError *error);
bool ab_classic_world_runtime_close(AbClassicWorldRuntime **runtime, AbError *error);
AbClassicWorldRuntimeView ab_classic_world_runtime_view(AbClassicWorldRuntime *runtime);
/* Publish the next source direct circle after its real playfield/native-vector
 * construction. Reserve object_capacity at open. No gameplay effects or RNG. */
bool ab_classic_world_runtime_register_circle(AbClassicWorldRuntime *runtime,
                                              size_t object, AbError *error);

#endif
