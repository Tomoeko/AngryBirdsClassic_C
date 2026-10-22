#include "ab/classic_collision_hooks.h"
#include "ab/classic_shot_effects.h"

#include <string.h>

static const char red_name[] = "flyingRedBirdCollided";

static bool same(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || (first.data && second.data &&
                            !memcmp(first.data, second.data, first.size)));
}

static bool red_collision(void *context, const size_t *flying,
                          const AbBirdCollisionNamedInput *contact, AbError *error) {
    (void)flying;
    (void)contact;
    AbClassicCollisionHooks *owner = context;
    AbDataValue enabled = *owner->config.red_special_available;
    bool truthy = enabled.kind != AB_DATA_NIL &&
                  (enabled.kind != AB_DATA_BOOLEAN || enabled.bits);
    return ab_classic_red_bird_collided(truthy, error);
}

static const AbClassicCollisionFunction *
registered(const AbClassicCollisionHooks *owner, uintptr_t identity) {
    if (identity == (uintptr_t)&owner->red_builtin)
        return &owner->red_builtin;
    for (size_t index = 0; index < owner->config.count; ++index) {
        const AbClassicCollisionFunction *function =
            owner->config.bindings[index].function;
        if (function && identity == (uintptr_t)function)
            return function;
    }
    return NULL;
}

bool ab_classic_collision_hooks_initialize(AbClassicCollisionHookConfig config,
                                           AbClassicCollisionHooks *output,
                                           AbError *error) {
    if (!output || !config.red_special_available ||
        (config.count && !config.bindings) || config.count > 4096)
        return ab_fail(error, "Invalid collision hook registry");
    AbSpan builtin = {(const uint8_t *)red_name, sizeof(red_name) - 1};
    for (size_t index = 0; index < config.count; ++index) {
        AbClassicCollisionHookBinding entry = config.bindings[index];
        if ((entry.name.size && !entry.name.data) || same(entry.name, builtin) ||
            (entry.function && !entry.function->call))
            return ab_fail(error, "Invalid collision hook name or method");
        for (size_t earlier = 0; earlier < index; ++earlier)
            if (same(entry.name, config.bindings[earlier].name))
                return ab_fail(error, "Duplicate collision hook binding");
    }
    *output = (AbClassicCollisionHooks){.config = config, .initialized = true};
    output->red_builtin = (AbClassicCollisionFunction){output, red_collision};
    output->red_slot = &output->red_builtin;
    return true;
}

bool ab_classic_collision_hooks_binding(void *context, AbDataValue key,
                                        AbUiPointerValue *output, AbError *error) {
    AbClassicCollisionHooks *owner = context;
    if (!owner || !owner->initialized || !output || key.kind != AB_DATA_STRING ||
        (key.string.size && !key.string.data))
        return ab_fail(error, "Invalid collision hook lookup");
    const AbClassicCollisionFunction *function = NULL;
    bool found =
        same(key.string, (AbSpan){(const uint8_t *)red_name, sizeof(red_name) - 1});
    if (found)
        function = owner->red_slot;
    else {
        for (size_t index = 0; index < owner->config.count; ++index) {
            const AbClassicCollisionHookBinding *binding =
                owner->config.bindings + index;
            if (same(key.string, binding->name)) {
                function = binding->function;
                found = true;
                break;
            }
        }
    }
    if (!found)
        return ab_fail(error, "Unimplemented collision hook lookup");
    if (function && !registered(owner, (uintptr_t)function))
        return ab_fail(error, "Unregistered live collision callback");
    *output = function ? (AbUiPointerValue){.kind = AB_UI_POINTER_FUNCTION,
                                            .function = (uintptr_t)function}
                       : (AbUiPointerValue){0};
    return true;
}

bool ab_classic_collision_hooks_call(void *context, AbUiPointerValue value,
                                     const size_t *flying,
                                     const AbBirdCollisionNamedInput *contact,
                                     AbError *error) {
    AbClassicCollisionHooks *owner = context;
    if (!owner || !owner->initialized || owner->calling ||
        value.kind != AB_UI_POINTER_FUNCTION)
        return ab_fail(error, "Invalid or reentrant collision callback call");
    const AbClassicCollisionFunction *function = registered(owner, value.function);
    if (!function || !function->call)
        return ab_fail(error, "Unknown collision callback identity");
    owner->calling = true;
    bool okay = function->call(function->context, flying, contact, error);
    owner->calling = false;
    return okay;
}

bool ab_classic_collision_hooks_print(void *context, AbUiPointerValue value,
                                      AbError *error) {
    AbClassicCollisionHooks *owner = context;
    if (!owner || !owner->initialized || owner->printing || !owner->config.print)
        return ab_fail(error, "Missing or reentrant collision print service");
    owner->printing = true;
    bool okay = owner->config.print(owner->config.print_context, value, error);
    owner->printing = false;
    return okay;
}
