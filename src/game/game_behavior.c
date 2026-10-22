#include "ab/game_behavior.h"

#include <string.h>

static bool empty_group(AbGameBehaviorRegistration *registration, AbSpan name,
                        AbError *error) {
    if (registration->group_count >= registration->group_capacity ||
        !registration->new_groups) {
        return ab_fail(error, "Game behavior group storage exhausted");
    }
    AbGameBehaviorGroup *group = registration->new_groups + registration->group_count;
    if (group->capacity && !group->items) {
        return ab_fail(error, "Missing new game behavior item storage");
    }
    for (size_t index = 0; index < group->capacity; ++index) {
        if (group->items[index]) {
            return ab_fail(error, "New game behavior group is not empty");
        }
    }
    ++registration->group_count;
    return ab_string_map_set(registration->groups, name, (uintptr_t)group, true, error);
}

bool ab_game_object_add_behavior(AbGameBehaviorRegistration *registration, AbSpan name,
                                 AbDataValue properties, AbError *error) {
    if (!registration || !registration->groups || (name.size && !name.data)) {
        return ab_fail(error, "Invalid game behavior registration");
    }
    uintptr_t value = 0;
    bool present = false;
    if (!ab_string_map_get(registration->groups, name, &value, &present, error)) {
        return false;
    }
    if ((!present || !value) && !empty_group(registration, name, error)) {
        return false;
    }
    if (!ab_string_map_get(registration->groups, name, &value, &present, error) ||
        !present || !value) {
        return ab_fail(error, "Missing current game behavior group");
    }
    AbGameBehaviorGroup *group = (AbGameBehaviorGroup *)value;
    if (group->capacity && !group->items) {
        return ab_fail(error, "Missing current game behavior item storage");
    }
    if (!registration->create) {
        return ab_fail(error, "Missing game behavior factory");
    }
    AbGameBehavior *behavior = NULL;
    if (!registration->create(registration->context, name, registration->receiver,
                              properties, &behavior, error)) {
        return false;
    }
    if (!behavior) {
        return ab_fail(error, "Game behavior factory did not return an instance");
    }
    for (size_t index = 0; index < group->capacity; ++index) {
        if (!group->items[index]) {
            group->items[index] = behavior;
            return true;
        }
    }
    return ab_fail(error, "Game behavior group item storage exhausted");
}

static bool same_name(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || (first.data && second.data &&
                            memcmp(first.data, second.data, first.size) == 0));
}

static bool delegate_behavior(AbGameBehavior *behavior, AbSpan event,
                              const AbUiPointerValue *arguments, size_t count,
                              AbError *error) {
    if (behavior->count && !behavior->methods) {
        return ab_fail(error, "Missing game behavior method storage");
    }
    for (size_t index = 0; index < behavior->count; ++index) {
        AbGameBehaviorMethod *method = behavior->methods + index;
        if (!same_name(method->event, event)) {
            continue;
        }
        if (!method->truthy) {
            return true;
        }
        if (!method->call) {
            return ab_fail(error, "Game behavior event binding is not callable");
        }
        return method->call(method->context, arguments, count, error);
    }
    return true;
}

bool ab_game_object_delegate(const AbStringMap *groups, AbSpan event,
                             const AbUiPointerValue *arguments, size_t count,
                             AbError *error) {
    if (!groups || (event.size && !event.data) || (count && !arguments)) {
        return ab_fail(error, "Invalid game behavior delegation arguments");
    }
    size_t cursor = 0;
    for (;;) {
        AbStringMapEntry entry;
        if (!ab_string_map_next(groups, &cursor, &entry, error)) {
            return false;
        }
        if (!entry.present) {
            return true;
        }
        AbGameBehaviorGroup *group = (AbGameBehaviorGroup *)entry.value;
        if (!group || (group->capacity && !group->items)) {
            return ab_fail(error, "Invalid game behavior group");
        }
        for (size_t index = 0; index < group->capacity; ++index) {
            AbGameBehavior *behavior = group->items[index];
            if (!behavior) {
                break;
            }
            if (!delegate_behavior(behavior, event, arguments, count, error)) {
                return false;
            }
        }
    }
}

static AbUiPointerValue numeric(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER, .number_bits = bits};
}

static void contact_arguments(const AbBlockCollisionNotice *notice,
                              AbUiPointerValue arguments[6]) {
    arguments[0] = numeric(notice->force);
    arguments[1] = numeric(notice->damage);
    arguments[2] =
        notice->normal_present ? numeric(notice->normal.x) : (AbUiPointerValue){0};
    arguments[3] =
        notice->normal_present ? numeric(notice->normal.y) : (AbUiPointerValue){0};
    arguments[4] =
        notice->point_present ? numeric(notice->point.x) : (AbUiPointerValue){0};
    arguments[5] =
        notice->point_present ? numeric(notice->point.y) : (AbUiPointerValue){0};
}

bool ab_game_object_bird_collision(void *context, const AbBlockCollisionNotice *notice,
                                   AbError *error) {
    if (!notice || !notice->first || !notice->second) {
        return ab_fail(error, "Missing bird behavior collision objects");
    }
    AbUiPointerValue arguments[8] = {
        {.kind = AB_UI_POINTER_OBJECT, .object = (uintptr_t)notice->first},
        {.kind = AB_UI_POINTER_OBJECT, .object = (uintptr_t)notice->second}};
    contact_arguments(notice, arguments + 2);
    return ab_game_object_delegate(
        context, (AbSpan){(const uint8_t *)"onBirdCollision", 15}, arguments, 8, error);
}

bool ab_game_object_block_collision(void *context, const AbBlockCollisionNotice *notice,
                                    AbError *error) {
    if (!notice) {
        return ab_fail(error, "Missing block behavior collision notice");
    }
    AbUiPointerValue arguments[7] = {
        {.kind = AB_UI_POINTER_TEXT, .text = notice->other_name}};
    contact_arguments(notice, arguments + 1);
    return ab_game_object_delegate(context,
                                   (AbSpan){(const uint8_t *)"onBlockCollision", 16},
                                   arguments, 7, error);
}

bool ab_game_object_destroy(void *context, AbError *error) {
    return ab_game_object_delegate(context, (AbSpan){(const uint8_t *)"onDestroy", 9},
                                   NULL, 0, error);
}
