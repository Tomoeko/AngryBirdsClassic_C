#ifndef AB_CLASSIC_DYNAMICS_H
#define AB_CLASSIC_DYNAMICS_H

#include "ab/classic_physics_phases.h"
#include "ab/classic_destruction_statistics.h"
#include "ab/classic_game_globals.h"
#include "ab/classic_goals.h"

typedef struct AbClassicDynamics AbClassicDynamics;

typedef enum {
    AB_CLASSIC_DYNAMICS_BASE_EFFECT = 1,
    AB_CLASSIC_DYNAMICS_CUSTOM_EFFECT,
} AbClassicDynamicsEffect;

typedef struct {
    AbClassicLevel *level;
    AbClassicVisuals *visuals;
    AbClassicRenderScene *scene;
    AbClassicBehaviors *behaviors;
    AbAudioBank *audio;
    AbSettingsStatistics *settings;
    AbEventManager *events;
    AbClassicGameGlobals *globals;
    AbRandom48 *random;
    /* Component capacities, raw fields, registries and optional services remain
     * explicit. Common owner references are selected from the fields above.
     * Simulation phase/rolling, removal statistics/goal callbacks, contact
     * routes/glue and the physical rolling map belong to this assembly. Leave
     * those selected fields NULL. Remaining callbacks retain their own original
     * config.context; no required source callback is replaced by success. */
    AbClassicSimulationConfig simulation;
    AbClassicObjectRemovalConfig physical;
    AbClassicGoalConfig goals;
    AbClassicDestructionStatisticsConfig statistics;
    AbClassicRemovalConfig removals;
    AbClassicContactsConfig contacts;
    AbClassicContactObject *contact_objects;
    size_t contact_count, storage_bytes;
    AbPhysicsScriptStep *scripts;
    AbClassicDynamicsEffect effect;
    void *water_context;
    bool (*water)(void *context, const AbWaterEffect *effect, float seconds,
                  AbError *error);
} AbClassicDynamicsConfig;

typedef struct {
    AbClassicSimulation *simulation;
    AbClassicObjectRemoval *physical;
    AbClassicGoals *goals;
    AbClassicDestructionStatistics *statistics;
    AbClassicRemovals *removals;
    AbClassicContacts *contacts;
    AbClassicPhysicsPhases *phases;
} AbClassicDynamicsView;

/* Production assembly of simulation, physical removal, goal membership,
 * destruction statistics, removeBlocks and the actual contact/physics-phase
 * bindings. Borrow the already loaded level/playfield resources, canonical
 * fields, actual block-script step, contact metadata and registries until close.
 * Source state and class selection are caller inputs; no seed, empty scripts,
 * teleporter map, notification policy, gameplay defaults or events are invented.
 * Resolve BASE_EFFECT explicitly to bind the real base destroyed-sound method;
 * CUSTOM_EFFECT requires its supplied method only when reached.
 *
 * Each component retains its supported domain and late-failure policy. Contact
 * collision/end/special services remain actual required caller work. This does
 * not supply full root/GameUpdate/input/menu/GPU scheduling. Open has no physics
 * step or gameplay callback and chooses no clock state. All owned component byte
 * budgets plus this owner must fit storage_bytes; bytes() reports that reserved
 * bound. No frame allocation beyond the world's existing bounded growth.
 *
 * Fixed initial world identities only. Calls are serialized. Do not close from
 * callbacks or while any child owner/view is active. Drain external event/map
 * observers before close. Close restores the previous world listener before
 * freeing owned state; active/replaced listener errors preserve the handle so
 * the caller can retry. Source level/resources outlive this owner. Failure
 * returns NULL and releases partial storage; discard the borrowing session if
 * a component has published state. */
bool ab_classic_dynamics_open(AbClassicDynamicsConfig config,
                              AbClassicDynamics **output, AbError *error);
bool ab_classic_dynamics_close(AbClassicDynamics **owner, AbError *error);
size_t ab_classic_dynamics_bytes(const AbClassicDynamics *owner);
AbClassicDynamicsView ab_classic_dynamics_view(AbClassicDynamics *owner);

#endif
