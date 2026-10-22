#ifndef AB_GOAL_ANIMATION_H
#define AB_GOAL_ANIMATION_H

#include "ab/base.h"

typedef struct {
    float strength;
    float special_timer;
    float blink_timer;
    float oink_timer;
    bool special_expression;
    bool blink_on;
} AbGoalAnimation;

typedef enum {
    AB_GOAL_FACE_SPECIAL = 1,
    AB_GOAL_FACE_SMILE,
    AB_GOAL_FACE_DAMAGE,
    AB_GOAL_FACE_BLINK,
} AbGoalFace;

typedef struct {
    void *context;
    bool (*random_integer)(void *context, int minimum, int maximum, int *output);
    bool (*lose_condition)(void *context, bool *output, AbError *error);
    bool (*set_sprite)(void *context, AbGoalFace face, AbError *error);
    bool (*camera_distance)(void *context, float *output, AbError *error);
    bool (*play_oink)(void *context, float volume, AbError *error);
    float base_strength;
    float damage1_min;
    bool disable_smiling;
} AbGoalAnimationControls;

/* Separate level-goal loops in updateCharacterAnimations: visit all eligible
 * expressions first, then all eligible oinks, then birds. eligible resolves
 * overrideDrawing/frozenSolid truthiness. Expression presence and blink_on are
 * truthy tests; clearing an expired expression means nil in the script state.
 * Sprite selection is unconditional, including absent resource values. Audio
 * callbacks supply the native object-to-camera distance, then select/play its
 * sound when 1-distance/1000 is positive. Coordinate conversion and sound
 * selection remain caller boundaries. Callbacks may observe/mutate state;
 * the blink flag is written after its sprite callback. No allocation occurs.
 * Finite used input and timer results are required; division by zero in the
 * strength ratio retains IEEE comparison behavior. Random callbacks can return
 * maximum+1 as the target's float wrapper can. Failure after a callback may
 * retain partial state, draws and effects. An ineligible object uses no inputs
 * beyond its required state pointer. */
bool ab_goal_expression_update(AbGoalAnimation *state, float seconds, bool eligible,
                               const AbGoalAnimationControls *controls, AbError *error);
bool ab_goal_oink_update(AbGoalAnimation *state, float seconds, bool eligible,
                         const AbGoalAnimationControls *controls, AbError *error);

#endif
