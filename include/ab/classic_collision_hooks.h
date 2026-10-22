#ifndef AB_CLASSIC_COLLISION_HOOKS_H
#define AB_CLASSIC_COLLISION_HOOKS_H

#include "ab/bird_collision_specialty.h"

typedef struct {
    void *context;
    bool (*call)(void *context, const size_t *flying,
                 const AbBirdCollisionNamedInput *contact, AbError *error);
} AbClassicCollisionFunction;

typedef struct {
    AbSpan name;
    const AbClassicCollisionFunction *function;
} AbClassicCollisionHookBinding;

typedef struct {
    const AbDataValue *red_special_available;
    const AbClassicCollisionHookBinding *bindings;
    size_t count;
    void *print_context;
    AbBirdCollisionPrint print;
} AbClassicCollisionHookConfig;

typedef struct {
    AbClassicCollisionHookConfig config;
    AbClassicCollisionFunction red_builtin;
    const AbClassicCollisionFunction *red_slot;
    bool initialized, calling, printing;
} AbClassicCollisionHooks;

/* Actual C callback identities for the recovered bird specialty lookup/call.
 * Bind flyingRedBirdCollided to its owned first-party implementation. Additional
 * source bindings must supply real immutable C function records, or explicit
 * resolved nil entries. Unknown names reject; missing implementations never
 * become absent handlers. This is a typed C registry, not a script evaluator.
 *
 * Keep this owner, raw flag, registrations and function records at stable
 * addresses. red_slot represents the live source global and may be rebound to
 * nil, the builtin, or a registered record. Earlier resolved aliases remain
 * callable while their records live. No allocation, gameplay defaults, effects
 * or RNG at initialize. Printing requires its real platform/log observer when
 * reached. Supported red inactive gates read the live typed flag; active red
 * collision cleanup retains its existing explicit unsupported boundary.
 * Source lookup/print/call order belongs to AbClassicBirdCollisions. Calls are
 * serialized; callback reentry rejects without invalidating the running call.
 */
bool ab_classic_collision_hooks_initialize(AbClassicCollisionHookConfig config,
                                           AbClassicCollisionHooks *output,
                                           AbError *error);
bool ab_classic_collision_hooks_binding(void *context, AbDataValue key,
                                        AbUiPointerValue *output, AbError *error);
bool ab_classic_collision_hooks_call(void *context, AbUiPointerValue function,
                                     const size_t *flying,
                                     const AbBirdCollisionNamedInput *contact,
                                     AbError *error);
bool ab_classic_collision_hooks_print(void *context, AbUiPointerValue value,
                                      AbError *error);

#endif
