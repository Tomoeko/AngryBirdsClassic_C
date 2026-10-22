#ifndef AB_GAME_PHYSICS_PREPARE_H
#define AB_GAME_PHYSICS_PREPARE_H

#include "ab/object_force_step.h"
#include "ab/object_friction.h"
#include "ab/physics_world.h"

typedef struct {
    size_t body;
    /* Native GameObject preparation gate, separate from body awake/active. */
    bool prepare_forces;
    AbObjectFriction friction;
    AbObjectForces forces;
    const bool *gravity_enabled;
    AbObjectForceInterval *interval;
    const AbObjectForceSound *sound;
    AbRandom48 *random;
    AbObjectForceHost resource_host;
    void *resource_context;
    /* Optional borrowed physical-map lifetime flag; skip before any body or
     * force-field access when true, even if an orphan engine body remains. */
    const bool *removed;
} AbGamePhysicsObject;

typedef enum {
    AB_GAME_PHYSICS_APPLY_QUEUED_FORCES = 1,
    AB_GAME_PHYSICS_WATER,
    AB_GAME_PHYSICS_SCRIPT_STEP,
    AB_GAME_PHYSICS_CLEAR_FORCE_FUNCTIONS,
} AbGamePhysicsStage;

typedef bool (*AbGamePhysicsHook)(void *context, AbGamePhysicsStage stage,
                                  AbPhysicsWorld *world, float seconds, AbError *error);

typedef struct {
    /* Borrowed original native object-map order, stable during the object pass.
     * The early queued-force hook may replace this view before traversal. */
    AbGamePhysicsObject *objects;
    size_t count;
    float water_seconds;
    AbGamePhysicsHook hook;
    void *context;
} AbGamePhysicsPrepare;

/* Direct AbGameFrameControls.before_step callback; context must point to an
 * AbGamePhysicsPrepare. Apply the queued-force hook, then each force-gated or
 * gravity-disabled object's
 * friction and force-adder in order, then water and Lua physics-step
 * hooks. Each force-adder reads current world gravity. water_seconds is the
 * frame's scaled physics duration, not the fixed step passed to SCRIPT_STEP;
 * callers set it before advancing that frame. APPLY/CLEAR have no numeric
 * argument and receive zero. NULL hook means those external stages are empty.
 * Object fields are resolved runtime state; interval resources use each row's
 * resource host. Dynamic traversal changes, table resolution and actual water/script
 * bodies remain external. World must be healthy/unlocked; active rows require live
 * bodies. Invalid top-level input rejects before work. Later failure retains preceding
 * hook/object changes; stop/discard the frame. No allocation or original code. */
bool ab_game_physics_before_step(void *context, AbPhysicsWorld *world, float seconds,
                                 AbError *error);

/* Direct after_steps callback: invoke force-function cleanup after the frame
 * driver has cleared forces, including an enabled zero-step update. */
bool ab_game_physics_after_steps(void *context, AbPhysicsWorld *world, AbError *error);

#endif
