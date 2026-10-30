#ifndef AB_BIRD_BUBBLES_H
#define AB_BIRD_BUBBLES_H

#include "ab/base.h"
#include "ab/physics_geometry.h"

typedef struct {
    AbVec2 relative;
    float radius;
    bool excluded_material, controllable, shot, puffer, flying, same;
} AbBirdBubbleCandidate;

/* Pinned scripts_common/bubbles.lua275: exclude the named material classes,
 * unshot controllable objects, the flying bird and PUFFER, then use the strict
 * distance test. The caller owns definitions, world iteration and RNG order. */
bool ab_bird_bubble_candidate(AbBirdBubbleCandidate input, bool *inside,
                              AbError *error);

/* updateBubble3505 tests the timer before decrementing. A crossing call still
 * applies force; cleanup occurs on the following call. Return a copied timer
 * and a cleanup request without mutating callbacks, particles or membership. */
bool ab_bird_bubble_timer(float timer, float seconds, float speed, float *next,
                          bool *cleanup, AbError *error);

/* Private function3017, default_conf.lua globals: 10 velocity limit and 1.8
 * antigravity. Normal input tests vertical velocity; held A tests total speed.
 * Keep raw script force units for the shared deferred force queue. */
bool ab_bird_bubble_force(AbVec2 velocity, bool held_a, float lateral, float mass,
                          float gravity, AbVec2 *force, bool *apply, AbError *error);

#endif
