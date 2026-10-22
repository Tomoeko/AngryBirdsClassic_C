#ifndef AB_CHARACTER_ANIMATION_H
#define AB_CHARACTER_ANIMATION_H

#include "ab/bird_animation.h"
#include "ab/bird_jump.h"
#include "ab/goal_animation.h"

typedef struct {
    AbGoalAnimation goal;
    AbBirdAnimation bird;
    AbBirdJumpState jump;
    bool override_drawing;
    bool frozen_solid;
    bool shot;
    bool animation_enabled;
    bool allow_roll;
    bool has_hat;
} AbCharacterAnimationObject;

typedef struct {
    AbGoalAnimationControls goal;
    AbBirdAnimationControls bird;
    AbBirdJumpRandom jump;
} AbCharacterAnimationBindings;

typedef struct {
    AbCharacterAnimationObject *objects;
    const AbCharacterAnimationBindings *bindings;
    size_t count;
    const size_t *goal_order;
    size_t goal_count;
    const size_t *bird_order;
    size_t bird_count;
    size_t current_bird;
    bool level_completed;
    void *context;
    bool (*apply_jump)(void *context, size_t object, const AbBirdJumpFrame *frame,
                       AbError *error);
    bool (*update_hat)(void *context, size_t object, AbError *error);
    /* Optional borrowed object-array mask. Removed rows leave both goal passes
     * and bird/jump/hat work before reading bindings, timers or RNG. */
    const bool *removed_objects;
} AbCharacterAnimations;

/* Visit native table orders: all goal expressions, all goal oinks, then each
 * eligible bird's sprite prefix and non-held jump/hat work. Orders are distinct
 * borrowed lists of object indices with unique entries within each list; an
 * object can belong to both lists. Storage and membership remain stable during
 * the call. Gates are resolved booleans: shot and allow_roll mean literal true;
 * drawing/frozen/timer-enabled mean script truthiness. Recheck goal eligibility
 * for its second pass, but retain a bird's enclosing gate through its jump.
 * Callbacks may mutate ordinary animation state and gate/current-bird flags.
 * Bindings retain their individual module contracts. The numeric jump helper
 * commits its result before apply_jump; its random callbacks must not mutate
 * the jump state, and intermediate jump-state snapshots are not reproduced.
 * apply_jump executes the ordered native position/rotation boundaries, including
 * coordinate conversions/history updates. Hats run afterward if that jump
 * entered its motion branch, including its final restoration frame.
 * No allocation, automatic membership changes or physics stepping occurs.
 * Invalid storage/orders preserve state; a later failure may retain partial
 * state, draws and effects. Full controller scheduling remains separate. */
bool ab_character_animations_update(AbCharacterAnimations *animations, float seconds,
                                    AbError *error);

#endif
