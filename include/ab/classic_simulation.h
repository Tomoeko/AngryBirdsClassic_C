#ifndef AB_CLASSIC_SIMULATION_H
#define AB_CLASSIC_SIMULATION_H

#include "ab/classic_forces.h"
#include "ab/classic_post_physics.h"

typedef struct AbClassicSimulation AbClassicSimulation;

typedef enum {
    AB_CLASSIC_SIMULATION_WATER,
    AB_CLASSIC_SIMULATION_SCRIPT_STEP,
    AB_CLASSIC_SIMULATION_AFTER_STEP,
} AbClassicSimulationStage;

typedef struct {
    AbClassicSimulationStage stage;
    float seconds, time;
} AbClassicSimulationCall;

/* WATER receives the scaled frame duration. SCRIPT_STEP and AFTER_STEP receive
 * the fixed timestep. AFTER_STEP runs after force clearing, before history:
 * removeBlocks and deferred velocity application belong here. Actual effects
 * remain required host work. Queued glue is consumed immediately after each
 * world step, before explicit force clearing and AFTER_STEP. time is the planned
 * new unscaled frame time; the underlying frame clock commits after its
 * physics/interpolation phase. */
typedef bool (*AbClassicSimulationHost)(void *context,
                                        const AbClassicSimulationCall *call,
                                        AbError *error);

typedef struct {
    size_t storage_bytes, queued_forces, force_sound_choices;
    size_t glue_pairs;
    size_t object_capacity;
} AbClassicSimulationLimits;

typedef struct {
    int32_t minimum_x, maximum_x;
    size_t velocity_iterations, position_iterations;
    uint32_t maximum_steps;
    AbRandom48 *random;
    AbClassicSimulationHost phase;
    AbClassicForceHost force;
    AbRollingAudioHost rolling;
    void *context;
    AbClassicSimulationLimits limits;
    AbClassicActivityBindings activity;
} AbClassicSimulationConfig;

typedef struct {
    AbClassicLevel *level;
    AbClassicForceView forces;
    AbClassicPostView post;
    AbForceQueue *queue;
    AbClassicGlueQueue *glue;
} AbClassicSimulationView;

/* Own force/friction state, bounded force/glue queues and the ordinary post-physics
 * controller for one borrowed level. The single byte limit covers all owned
 * allocations. Names, catalog, bodies and optional RNG remain borrowed; close
 * before the level and preserve fixed object/body slots. Successful physical
 * removal must call ab_classic_level_forget_object before later passes; removed
 * rows skip force/history/publication even when an orphan body stays live.
 * Direct circles can register between frames in explicitly reserved rows.
 * General insertion/reinsertion remains unsupported. No initial RNG,
 * effects or world changes. Unsupported bindings reject and output starts NULL.
 * This is the simulation portion of GameLua.update, before its root controller;
 * app input/publication, teleporters, root update and particles are separate
 * work. It does not provide a playable session or pause policy. */
bool ab_classic_simulation_open(AbClassicLevel *level, AbClassicSimulationConfig config,
                                AbClassicSimulation **output, AbError *error);
void ab_classic_simulation_close(AbClassicSimulation *simulation);
size_t ab_classic_simulation_bytes(const AbClassicSimulation *simulation);
AbClassicSimulationView ab_classic_simulation_view(AbClassicSimulation *simulation);

/* Register the next source-created direct circle with both actual force and
 * post-physics owners, before its first frame. No source fields are defaulted
 * from a later copied catalog definition. Refresh views after registration.
 * Does not supply contacts, physical removal, bird queues or script adapters.
 * No allocation or callbacks. Earlier registration effects survive failure;
 * discard on error. Calls during a simulation pass reject. */
bool ab_classic_simulation_register_circle(AbClassicSimulation *simulation,
                                           size_t object, AbError *error);

/* Plan timing, repeat queued forces/friction/force-adder/water/script/physics/
 * glue/after-step/history in recovered order, clear the force queue once after all
 * enabled steps, interpolate, then publish properties/activity/rolling requests. Queue
 * requests survive every fixed step until that final clear, including enabled
 * zero-step frames. The glue queue is consumed only after actual world steps;
 * disabled and zero-step frames preserve it. Pair production is explicit through
 * the borrowed glue view, and engine/registry capacity belongs to the level.
 * Disabled physics preserves queue/post state while advancing unscaled time.
 * A phase host is required when a fixed step reaches it.
 * No allocation beyond bounded world broadphase growth. No implicit delta cap,
 * seed or dropped steps. Output commits on complete success; a late failure can
 * retain world, history, clock, queue, RNG or host effects. Stop/discard on error.
 * Do not reenter from a host callback or mutate storage/membership mid-pass. */
bool ab_classic_simulation_advance(AbClassicSimulation *simulation,
                                   const AbFrameClockInput *timing,
                                   AbFrameClockPlan *output, AbError *error);

#endif
