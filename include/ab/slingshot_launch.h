#ifndef AB_SLINGSHOT_LAUNCH_H
#define AB_SLINGSHOT_LAUNCH_H

#include "ab/bird_shot.h"

typedef struct {
    AbShotBird *dragged;
    float default_force, physics_scale;
    /* Resolved distinct Lua predicates: equality to Boolean true vs truthiness. */
    bool eagle_bait_is_true, special_mode_truthy;
} AbSlingLaunchState;

typedef enum {
    AB_SLING_LAUNCH_DIRECTION = 1,
    AB_SLING_LAUNCH_DISTANCE,
    AB_SLING_LAUNCH_BACKWARDS,
    AB_SLING_LAUNCH_SHOT_COUNT,
    AB_SLING_LAUNCH_STRETCH,
    AB_SLING_LAUNCH_FORCE,
    AB_SLING_LAUNCH_POWER,
    AB_SLING_LAUNCH_POSITION,
    AB_SLING_LAUNCH_WITH_IMPULSE,
} AbSlingLaunchOperation;

typedef struct {
    AbSlingLaunchOperation operation;
    AbVec2 *direction, *position, *impulse;
    const char *name;
    float number;
    bool force_present;
} AbSlingLaunchCall;

typedef bool (*AbSlingLaunchHost)(void *context, AbSlingLaunchCall *call,
                                  AbError *error);

/* SlingshotSystem.launchBird's ordered query/statistic/impulse route. An explicit
 * direction is borrowed and its length is measured before callbacks; NULL asks
 * the host for direction and distance separately. Direction components stay live
 * through the power query. Always increment total shots after the optional
 * backwards increment. Read definition force for the current dragged name;
 * absent/false selects the then-current default. Numeric zero is present.
 * Capture scale/mass before querying power, then calculate impulse before asking
 * for the launch-position vector. WITH_IMPULSE receives that borrowed mutable
 * position and a temporary mutable impulse, suitable for ab_bird_shot.
 *
 * Hosts may change live state/vectors/bird metadata; borrowed identities must
 * remain valid until last use. Required numeric replies/used intermediates are
 * finite, used mass positive and used distance nonzero. Generic property lookup,
 * function rebinding and Lua coercion are separate. No allocation. A late error
 * retains preceding host effects; no later callback runs. Do not reenter.
 */
bool ab_slingshot_launch(AbSlingLaunchState *state, AbVec2 *direction,
                         AbSlingLaunchHost host, void *context, AbError *error);

typedef enum {
    AB_SLING_RELEASE_CHECK = 1,
    AB_SLING_RELEASE_LAUNCH,
    AB_SLING_RELEASE_CANCEL,
} AbSlingReleaseOperation;

typedef struct {
    AbSlingReleaseOperation operation;
    bool stretched;
} AbSlingReleaseCall;

typedef bool (*AbSlingReleaseHost)(void *context, AbSlingReleaseCall *call,
                                   AbError *error);

/* Query resolved stretch truthiness, then invoke launch or cancel in order.
 * The host binds the actual launch/cancel operations and their state effects.
 * No allocation or numeric work. A failed query/operation stops immediately. */
bool ab_slingshot_release(AbSlingReleaseHost host, void *context, AbError *error);

#endif
