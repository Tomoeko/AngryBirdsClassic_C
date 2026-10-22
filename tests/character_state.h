#ifndef AB_TEST_CHARACTER_STATE_H
#define AB_TEST_CHARACTER_STATE_H

#include "ab/character_animation.h"

#include <string.h>

static uint32_t character_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void character_state_words(const AbCharacterAnimationObject *input,
                                  uint32_t output[20]) {
    const AbGoalAnimation *goal = &input->goal;
    const AbBirdJumpState *jump = &input->jump;
    uint32_t words[] = {character_float_bits(goal->strength),
                        character_float_bits(goal->special_timer),
                        character_float_bits(goal->blink_timer),
                        character_float_bits(goal->oink_timer),
                        goal->special_expression,
                        goal->blink_on,
                        character_float_bits(input->bird.timer),
                        input->bird.active,
                        character_float_bits(jump->position.x),
                        character_float_bits(jump->position.y),
                        character_float_bits(jump->rotation),
                        character_float_bits(jump->timer),
                        character_float_bits(jump->height),
                        character_float_bits(jump->speed),
                        character_float_bits(jump->phase),
                        character_float_bits(jump->roll),
                        character_float_bits(jump->saved_y),
                        character_float_bits(jump->saved_angle),
                        jump->jumping,
                        jump->rebound};
    memcpy(output, words, sizeof(words));
}

#endif
