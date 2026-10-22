#ifndef AB_BIRD_JUMP_H
#define AB_BIRD_JUMP_H

#include "ab/physics_geometry.h"

typedef struct {
    AbVec2 position;
    float rotation;
    float timer;
    float height;
    float speed;
    float phase;
    float roll;
    float saved_y;
    float saved_angle;
    bool jumping;
    bool rebound;
} AbBirdJumpState;

typedef enum {
    AB_BIRD_JUMP_POSITION = 1,
    AB_BIRD_JUMP_ROTATION,
} AbBirdJumpEventKind;

typedef struct {
    AbBirdJumpEventKind kind;
    AbVec2 value;
} AbBirdJumpEvent;

typedef struct {
    AbBirdJumpState next;
    AbBirdJumpEvent events[4];
    size_t event_count;
} AbBirdJumpFrame;

typedef struct {
    void *context;
    bool (*integer)(void *context, int minimum, int maximum, int *output);
} AbBirdJumpRandom;

/* Idle, unshot bird branch of gamelogic updateCharacterAnimations. The enclosing
 * controller excludes the current held bird and disabled drawing/animation.
 * level_completed resolves Lua truthiness; allow_roll and stored jumping/
 * rebound flags resolve literal-true tests. Keep target float
 * arithmetic, random draw bounds/order, one rebound and optional roll. The
 * caller supplies random integers, including the native float wrapper's rare
 * upper-bound-plus-one result, and executes ordered position/rotation
 * events, including final restoration. Event positions are numeric boundaries;
 * native coordinate round trips and hats belong to the enclosing controller.
 * Decrement the timer only when not jumping. Draw when the resulting timer is
 * strictly negative, including a negative timer already stored while jumping.
 * Finite required values/results and |advanced phase| <= 1024 are supported.
 * Failure preserves output; a supplied random callback may already have run.
 */
bool ab_bird_jump_frame(const AbBirdJumpState *state, float seconds,
                        bool level_completed, bool allow_roll,
                        const AbBirdJumpRandom *random, AbBirdJumpFrame *output);

#endif
