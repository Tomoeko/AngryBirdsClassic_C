#ifndef AB_OBJECT_FORCES_H
#define AB_OBJECT_FORCES_H

#include "ab/base.h"
#include "ab/physics_step.h"

typedef struct {
    AbVec2 gravity;
    AbVec2 force;
    float object_mass;
    float relative_force;
    float target_velocity;
    bool gravity_enabled;
    bool disabled;
    bool force_x_present, force_y_present;
    bool target_present;
    bool relative_present;
    bool impulse;
} AbObjectForces;

/* Resolved GameLua force-adder application. impulse selects the interval pulse
 * application; otherwise forces accumulate continuously. Timing and resources
 * remain separate from this numeric operation.
 * A present nonzero relative_force uses the body's angle with binary64 trig
 * and multiplication before float rounding; it ignores mass and X/Y settings.
 * The supported finite angle arc is [-1024,1024].
 * Settings contain resolved object values; only literal Boolean true disables
 * the configured force. Gravity cancellation
 * still runs while disabled and uses the engine body's mass. Configured axes
 * use the supplied object-table mass, with absent axes set to positive zero.
 * A target suppresses each axis only at/above its absolute velocity limit while
 * that force accelerates it; a force admitted before this gate still wakes the body
 * if both resulting axes are zero. Accumulation follows gravity then configured
 * force, both at center. Static/kinematic bodies preserve all state. Used
 * numbers/results must be finite; failure preserves the body. No allocation,
 * Lua, timer, sprite, audio or random work executes here. */
bool ab_object_forces_apply(AbBodyMotion *body, const AbObjectForces *settings,
                            AbError *error);

/* Direct GameLua applyForce numeric boundary: accumulate force and torque about
 * the body's world-space center, waking a dynamic body even for zero force.
 * Static/kinematic bodies ignore all numbers. Used inputs/results must be finite;
 * failure preserves body state. Body lookup and world locking are caller-owned. */
bool ab_object_force_at(AbBodyMotion *body, AbVec2 force, AbVec2 point, AbError *error);

typedef struct {
    float interval, timer;
    AbSpan sprite, applied_sprite, original_sprite;
    bool timer_present, original_present;
} AbObjectForceInterval;

typedef struct {
    AbObjectForceInterval next;
    bool pulse;
    bool sprite_requested;
} AbObjectForceIntervalPlan;

/* Numeric applyForceInterval route, separate from applying gravity/impulses.
 * Subtract seconds from the present timer or initial interval. At/below zero,
 * add one interval (no catch-up loop), save the current sprite and request the
 * applied sprite. Otherwise restore a present original sprite below binary64
 * interval * 0.8, including repeated requests without clearing that reference.
 * Disabled plans preserve all state and ignore numeric/resource values.
 * Borrowed sprite bytes remain owned by the caller. Required numbers/results
 * must be finite; invalid input preserves output. Actual sprite/audio/random
 * effects and their ordering are caller work. No allocation or original code. */
bool ab_object_force_interval_plan(const AbObjectForceInterval *state, float seconds,
                                   bool disabled, AbObjectForceIntervalPlan *output,
                                   AbError *error);

#endif
