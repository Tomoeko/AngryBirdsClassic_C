#include "ab/character_animation.h"

static bool valid_order(const size_t *order, size_t count, size_t objects) {
    if (count > objects || (count && !order)) {
        return false;
    }
    for (size_t cursor = 0; cursor < count; ++cursor) {
        if (order[cursor] >= objects) {
            return false;
        }
        for (size_t previous = 0; previous < cursor; ++previous) {
            if (order[previous] == order[cursor]) {
                return false;
            }
        }
    }
    return true;
}

static bool goal_eligible(const AbCharacterAnimationObject *object) {
    return !object->override_drawing && !object->frozen_solid;
}

static bool update_birds(AbCharacterAnimations *animations, float seconds,
                         AbError *error) {
    for (size_t cursor = 0; cursor < animations->bird_count; ++cursor) {
        size_t index = animations->bird_order[cursor];
        if (animations->removed_objects && animations->removed_objects[index]) {
            continue;
        }
        AbCharacterAnimationObject *object = animations->objects + index;
        const AbCharacterAnimationBindings *binding = animations->bindings + index;
        if (object->shot || object->override_drawing || !object->animation_enabled) {
            continue;
        }
        if (!ab_bird_animation_update(&object->bird, seconds, true, &binding->bird,
                                      error)) {
            return false;
        }
        if (index == animations->current_bird) {
            continue;
        }
        AbBirdJumpFrame jump;
        if (!ab_bird_jump_frame(&object->jump, seconds, animations->level_completed,
                                object->allow_roll, &binding->jump, &jump)) {
            return ab_fail(error, "Cannot update character jump frame");
        }
        object->jump = jump.next;
        if (!jump.event_count) {
            continue;
        }
        if (!animations->apply_jump) {
            return ab_fail(error, "Missing character jump application boundary");
        }
        if (!animations->apply_jump(animations->context, index, &jump, error)) {
            return false;
        }
        if (object->has_hat) {
            if (!animations->update_hat) {
                return ab_fail(error, "Missing character hat update boundary");
            }
            if (!animations->update_hat(animations->context, index, error)) {
                return false;
            }
        }
    }
    return true;
}

bool ab_character_animations_update(AbCharacterAnimations *animations, float seconds,
                                    AbError *error) {
    if (!animations ||
        ((animations->goal_count || animations->bird_count) &&
         (!animations->objects || !animations->bindings)) ||
        !valid_order(animations->goal_order, animations->goal_count,
                     animations->count) ||
        !valid_order(animations->bird_order, animations->bird_count,
                     animations->count)) {
        return ab_fail(error, "Invalid character animation storage or table orders");
    }
    for (size_t cursor = 0; cursor < animations->goal_count; ++cursor) {
        size_t index = animations->goal_order[cursor];
        if (animations->removed_objects && animations->removed_objects[index]) {
            continue;
        }
        AbCharacterAnimationObject *object = animations->objects + index;
        if (!ab_goal_expression_update(&object->goal, seconds, goal_eligible(object),
                                       &animations->bindings[index].goal, error)) {
            return false;
        }
    }
    for (size_t cursor = 0; cursor < animations->goal_count; ++cursor) {
        size_t index = animations->goal_order[cursor];
        if (animations->removed_objects && animations->removed_objects[index]) {
            continue;
        }
        AbCharacterAnimationObject *object = animations->objects + index;
        if (!ab_goal_oink_update(&object->goal, seconds, goal_eligible(object),
                                 &animations->bindings[index].goal, error)) {
            return false;
        }
    }
    return update_birds(animations, seconds, error);
}
