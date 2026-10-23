#ifndef AB_CLASSIC_BLOCK_SCRIPTS_H
#define AB_CLASSIC_BLOCK_SCRIPTS_H

#include "ab/block_script.h"
#include "ab/classic_ambient.h"
#include "ab/classic_flames.h"
#include "ab/classic_level.h"
#include "ab/classic_visuals.h"
#include "ab/physics_script.h"

typedef struct AbClassicBlockScripts AbClassicBlockScripts;

typedef struct {
    AbClassicLevel *level;
    AbClassicVisuals *visuals;
    AbRandom48 *random;
    AbClassicParticles *particles;
    AbClassicRenderScene *scene;
    AbExtraDraws *extra_draws;
    float physics_scale;
    const bool *physics_enabled;
    size_t storage_bytes;
    size_t object_capacity;
    void *context;
    bool (*level_step)(void *context, float seconds, AbError *error);
} AbClassicBlockScriptConfig;

typedef struct {
    AbBlockScriptManager *manager;
    AbBlockEventObject *objects;
    size_t count;
    AbBlockScriptUpdate *update;
    AbPhysicsScriptStep *physics;
    AbClassicAmbient *ambient;
    AbClassicAmbient *ambient_particles;
    AbClassicFlames *flames;
    AbBlockEventHost host;
} AbClassicBlockScriptView;

/* Actual default BlockScriptManager registration order and canApply type gates
 * for FlameEffect, AmbientAnimations and AmbientParticles. Build one canonical
 * attachment row per initial object and apply all three classes in creation
 * order. Their unapplied private maps are the original empty maps; update reads
 * the actual physics flag and traverses those maps through the real manager.
 *
 * AmbientAnimations owns its actual per-object identity map and mutable variant
 * copies, drives the shared RNG and changes actual visual/physical sprites.
 * AmbientParticles has its own identity map and copied effect timers, emits at
 * the current body angle/script position and uses the shared particle engine.
 * FlameEffect owns its source state, object update and z draw registrations.
 * No VM, invented empty script registration, frame allocation or physics mutation.
 * Borrow the actual level, flag and optional resolved level physics hook.
 * Close only after controller/contact users release these stable views. */
bool ab_classic_block_scripts_open(AbClassicBlockScriptConfig config,
                                   AbClassicBlockScripts **output, AbError *error);
void ab_classic_block_scripts_close(AbClassicBlockScripts *scripts);
AbClassicBlockScriptView ab_classic_block_scripts_view(AbClassicBlockScripts *scripts);
/* Direct-circle construction does not run the ordinary factory's component
 * application. Publish its actual empty script row in reserved stable storage. */
bool ab_classic_block_scripts_register_circle(AbClassicBlockScripts *scripts,
                                              size_t object, AbError *error);
/* Ordinary box factory: register the row, then apply the actual default classes. */
bool ab_classic_block_scripts_register_box(AbClassicBlockScripts *scripts,
                                           size_t object, AbError *error);

#endif
