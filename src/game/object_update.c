#include "ab/object_update.h"

static bool valid_function(const AbObjectUpdateFunction *function) {
    return function && function->invoke;
}

static bool valid_list(const AbObjectUpdateList *list) {
    if (!list || list->count > list->capacity || list->capacity > UINT32_C(16777216) ||
        (list->capacity && !list->items)) {
        return false;
    }
    for (size_t index = 0; index < list->count; ++index) {
        if (!valid_function(list->items[index])) {
            return false;
        }
    }
    return true;
}

static bool valid_value(AbObjectUpdateValue value) {
    switch (value.kind) {
        case AB_OBJECT_UPDATE_NONE:
            return true;
        case AB_OBJECT_UPDATE_FUNCTION:
            return valid_function(value.function);
        case AB_OBJECT_UPDATE_LIST:
            return valid_list(value.list);
        default:
            return false;
    }
}

static bool valid_state(const AbObjectUpdates *state) {
    return state && valid_value(state->object) && valid_value(state->registered);
}

static size_t find_function(const AbObjectUpdateList *list,
                            const AbObjectUpdateFunction *function) {
    for (size_t index = 0; index < list->count; ++index) {
        if (list->items[index] == function) {
            return index;
        }
    }
    return list->count;
}

bool ab_object_update_add(AbObjectUpdates *state,
                          const AbObjectUpdateFunction *function,
                          AbObjectUpdateList *promotion, AbError *error) {
    if (!valid_state(state) || !valid_function(function)) {
        return ab_fail(error, "Invalid object update addition");
    }
    switch (state->object.kind) {
        case AB_OBJECT_UPDATE_NONE:
            state->object = (AbObjectUpdateValue){AB_OBJECT_UPDATE_FUNCTION,
                                                  {.function = function}};
            break;
        case AB_OBJECT_UPDATE_FUNCTION:
            if (!valid_list(promotion) || promotion->count || promotion->capacity < 2 ||
                (state->registered.kind == AB_OBJECT_UPDATE_LIST &&
                 promotion == state->registered.list)) {
                return ab_fail(error, "Invalid object update promotion storage");
            }
            promotion->items[0] = state->object.function;
            promotion->items[1] = function;
            promotion->count = 2;
            state->object =
                (AbObjectUpdateValue){AB_OBJECT_UPDATE_LIST, {.list = promotion}};
            break;
        case AB_OBJECT_UPDATE_LIST: {
            AbObjectUpdateList *list = state->object.list;
            if (find_function(list, function) != list->count) {
                return ab_fail(error,
                               "Duplicate object update addition assertion failed");
            }
            if (list->count == list->capacity) {
                return ab_fail(error, "Object update list exceeds its capacity");
            }
            list->items[list->count++] = function;
            break;
        }
        default:
            return ab_fail(error, "Invalid object update value");
    }
    state->registered = state->object;
    return true;
}

bool ab_object_update_remove(AbObjectUpdates *state,
                             const AbObjectUpdateFunction *function, AbError *error) {
    if (!valid_state(state) || !valid_function(function)) {
        return ab_fail(error, "Invalid object update removal");
    }
    if (state->object.kind == AB_OBJECT_UPDATE_FUNCTION &&
        state->object.function == function) {
        state->object = state->registered = (AbObjectUpdateValue){0};
        return true;
    }
    if (state->object.kind != AB_OBJECT_UPDATE_LIST) {
        return ab_fail(error, "Object update removal assertion failed");
    }
    AbObjectUpdateList *list = state->object.list;
    size_t found = find_function(list, function);
    if (found != list->count) {
        for (size_t index = found; index + 1 < list->count; ++index) {
            list->items[index] = list->items[index + 1];
        }
        list->items[--list->count] = NULL;
        if (list->count == 1) {
            state->object = (AbObjectUpdateValue){AB_OBJECT_UPDATE_FUNCTION,
                                                  {.function = list->items[0]}};
        }
    }
    return true;
}

bool ab_object_update_clear(AbObjectUpdates *state, AbError *error) {
    if (!valid_state(state)) {
        return ab_fail(error, "Invalid object update clear");
    }
    state->object = state->registered = (AbObjectUpdateValue){0};
    return true;
}

bool ab_object_update_has(const AbObjectUpdates *state,
                          const AbObjectUpdateFunction *function, bool *output,
                          AbError *error) {
    if (!valid_state(state) || !valid_function(function) || !output) {
        return ab_fail(error, "Invalid object update membership query");
    }
    *output = state->object.kind == AB_OBJECT_UPDATE_FUNCTION
                  ? state->object.function == function
                  : state->object.kind == AB_OBJECT_UPDATE_LIST &&
                        find_function(state->object.list, function) !=
                            state->object.list->count;
    return true;
}

bool ab_object_update_call(AbObjectUpdateValue value, void *object, float seconds,
                           AbError *error) {
    if (!valid_value(value) || value.kind == AB_OBJECT_UPDATE_NONE) {
        return ab_fail(error, "Invalid callable object update value");
    }
    if (value.kind == AB_OBJECT_UPDATE_FUNCTION) {
        return value.function->invoke(value.function->context, object, seconds, error);
    }
    AbObjectUpdateList *list = value.list;
    for (size_t index = 0; index < list->count; ++index) {
        if (!valid_list(list)) {
            return ab_fail(error, "Invalid mutated object update list");
        }
        const AbObjectUpdateFunction *function = list->items[index];
        if (!function->invoke(function->context, object, seconds, error)) {
            return false;
        }
    }
    return true;
}
