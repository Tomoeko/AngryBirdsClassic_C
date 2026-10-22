#ifndef AB_GAME_FRAME_H
#define AB_GAME_FRAME_H

#include "ab/body_pose.h"
#include "ab/frame_clock.h"
#include "ab/physics_world.h"

typedef struct {
    size_t body;
    uint32_t collider;
    AbBodyPose history[2];
    AbBodyPose rendered;
} AbGameFrameObject;

typedef struct {
    AbFrameClock clock;
    AbGameFrameObject *objects;
    size_t count;
    uint32_t history_index;
    /* Optional borrowed lifetime mask in object-array order. Removed rows keep
     * their cached poses and never read a body, including locked-world orphans. */
    const bool *removed_objects;
} AbGameFrame;

typedef struct {
    size_t velocity_iterations;
    size_t position_iterations;
    uint32_t maximum_steps;
    void *context;
    bool (*after_step)(void *context, AbPhysicsWorld *world, AbError *error);
    bool (*before_step)(void *context, AbPhysicsWorld *world, float seconds,
                        AbError *error);
    bool (*after_steps)(void *context, AbPhysicsWorld *world, AbError *error);
    bool (*after_world_step)(void *context, AbPhysicsWorld *world, AbError *error);
} AbGameFrameControls;

/* Reset both history samples and rendered pose after an explicit body transform.
 * Caller supplies its collider. Invalid input preserves output. */
bool ab_game_frame_object_reset(const AbPhysicsWorld *world, size_t body,
                                uint32_t collider, AbGameFrameObject *output,
                                AbError *error);

/* Compose the recovered clock, 1/30 physics steps, gated history passes and
 * awake-body interpolation. before_step runs before each world step. Forces
 * after_world_step runs immediately after each world step, where the native
 * game consumes glue pairs. Forces then clear before after_step and history
 * capture. World auto-clear may already have run according to its own setting.
 * Enabled physics clears forces once more after the loop (including zero steps), then
 * runs after_steps before interpolation. Disabled physics runs none of these phases.
 * Missing bodies retain their old poses; callers remove/reset
 * bindings when object lifetimes change or body slots are reused. Callers run
 * surrounding controllers, animation, effects and events in their recovered order.
 * Storage is caller-owned and disjoint from world/output. This phase allocates
 * no storage beyond the world's configured broadphase growth. Invalid clock or
 * binding storage rejects before stepping; later failure may leave partial
 * world/history/callback changes, so the caller must stop or discard that frame.
 * Clock and plan commit on success. */
bool ab_game_frame_advance(AbGameFrame *frame, AbPhysicsWorld *world,
                           const AbFrameClockInput *timing,
                           const AbGameFrameControls *controls,
                           AbFrameClockPlan *output, AbError *error);

#endif
