#ifndef AB_CLASSIC_PHYSICS_PHASES_H
#define AB_CLASSIC_PHYSICS_PHASES_H

#include "ab/classic_simulation.h"
#include "ab/classic_contacts.h"
#include "ab/classic_removals.h"
#include "ab/physics_script.h"
#include "ab/water_effect.h"

typedef struct {
    AbClassicLevel *level;
    AbClassicContacts *contacts;
    AbClassicRemovals *removals;
    AbClassicObjectRemoval *physical;
    AbPhysicsScriptStep *scripts;
    void *context;
    bool (*water)(void *context, const AbWaterEffect *effect, float seconds,
                  AbError *error);
} AbClassicPhysicsPhaseConfig;

typedef struct {
    AbClassicPhysicsPhaseConfig config;
    AbWaterEffect water;
    bool calling;
} AbClassicPhysicsPhases;

/* Resolve WaterEffect from actual loaded level globals. Its disabled native
 * gate needs no buoyancy host; enabled water always requires the real service.
 * Borrow stable phase/listener/script/core owners. Listener storage may be bound
 * after initialize, before the first advance. Initialization has no effects.
 * The script step is the actual optional level hook plus live block dispatcher;
 * the caller must supply those resolved registries, not an empty substitute.
 *
 * AFTER_STEP runs removeBlocks first, then pending velocities for surviving
 * entries in the native creation-order bird vector. Physical removal prunes that
 * same vector. Do not flush removed objects or reorder by Lua bird indices.
 * Bind call as the simulation phase host. No allocation, skipped required phase,
 * root/controller scheduling or water implementation is supplied here. Earlier
 * world/removal/proposal effects survive failure; discard the failed frame.
 */
bool ab_classic_physics_phases_initialize(AbClassicPhysicsPhaseConfig config,
                                          AbClassicPhysicsPhases *output,
                                          AbError *error);
bool ab_classic_physics_phases_call(void *context, const AbClassicSimulationCall *call,
                                    AbError *error);

#endif
