#include "ab/classic_collision_hooks.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Collision hooks failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbClassicCollisionHooks hooks;
    size_t calls, prints;
    uintptr_t printed;
} HookTest;

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static bool owned_callback(void *context, const size_t *flying,
                           const AbBirdCollisionNamedInput *contact, AbError *error) {
    HookTest *test = context;
    CHECK(flying && *flying == 3 && contact && contact->force == 5);
    ++test->calls;
    test->hooks.red_slot = NULL;
    AbUiPointerValue current;
    CHECK(ab_classic_collision_hooks_binding(
              &test->hooks, text("flyingRedBirdCollided"), &current, error) &&
          current.kind == AB_UI_POINTER_NONE);
    CHECK(!ab_classic_collision_hooks_call(
        &test->hooks,
        (AbUiPointerValue){.kind = AB_UI_POINTER_FUNCTION,
                           .function = (uintptr_t)&test->hooks.red_builtin},
        NULL, NULL, NULL));
    return true;
}

static bool print_callback(void *context, AbUiPointerValue value, AbError *error) {
    (void)error;
    HookTest *test = context;
    CHECK(value.kind == AB_UI_POINTER_FUNCTION);
    ++test->prints;
    test->printed = value.function;
    test->hooks.red_slot = NULL;
    CHECK(!ab_classic_collision_hooks_print(&test->hooks, value, NULL));
    return true;
}

static bool check_hooks(void) {
    HookTest test = {0};
    AbDataValue red = {0};
    AbClassicCollisionFunction owned = {&test, owned_callback};
    AbClassicCollisionHookBinding bindings[] = {{text("ownedHook").string, &owned},
                                                {text("ownedHookAlias").string, &owned},
                                                {text("absentHook").string, NULL}};
    AbClassicCollisionHookConfig config = {.red_special_available = &red,
                                           .bindings = bindings,
                                           .count = 3,
                                           .print_context = &test,
                                           .print = print_callback};
    CHECK(!ab_classic_collision_hooks_initialize((AbClassicCollisionHookConfig){0},
                                                 &test.hooks, NULL) &&
          ab_classic_collision_hooks_initialize(config, &test.hooks, NULL));
    AbUiPointerValue original, alias, nil;
    CHECK(ab_classic_collision_hooks_binding(&test.hooks, text("flyingRedBirdCollided"),
                                             &original, NULL) &&
          original.kind == AB_UI_POINTER_FUNCTION &&
          original.function == (uintptr_t)&test.hooks.red_builtin &&
          ab_classic_collision_hooks_call(&test.hooks, original, NULL, NULL, NULL));
    CHECK(ab_classic_collision_hooks_print(&test.hooks, original, NULL) &&
          test.prints == 1 && test.printed == original.function &&
          ab_classic_collision_hooks_binding(&test.hooks, text("flyingRedBirdCollided"),
                                             &nil, NULL) &&
          nil.kind == AB_UI_POINTER_NONE &&
          ab_classic_collision_hooks_call(&test.hooks, original, NULL, NULL, NULL));
    CHECK(ab_classic_collision_hooks_binding(&test.hooks, text("ownedHook"), &alias,
                                             NULL) &&
          ab_classic_collision_hooks_binding(&test.hooks, text("ownedHookAlias"), &nil,
                                             NULL) &&
          alias.function == nil.function && alias.function == (uintptr_t)&owned);
    test.hooks.red_slot = &owned;
    CHECK(ab_classic_collision_hooks_binding(&test.hooks, text("flyingRedBirdCollided"),
                                             &nil, NULL) &&
          nil.function == alias.function);
    size_t flying = 3;
    AbBirdCollisionNamedInput contact = {.force = 5};
    CHECK(
        ab_classic_collision_hooks_call(&test.hooks, alias, &flying, &contact, NULL) &&
        test.calls == 1 && !test.hooks.calling &&
        ab_classic_collision_hooks_call(&test.hooks, original, NULL, NULL, NULL));
    red.kind = AB_DATA_NUMBER;
    CHECK(!ab_classic_collision_hooks_call(&test.hooks, original, NULL, NULL, NULL));
    red.kind = AB_DATA_BOOLEAN;
    CHECK(ab_classic_collision_hooks_call(&test.hooks, original, NULL, NULL, NULL));
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER, .number_bits = 123};
    CHECK(
        !ab_classic_collision_hooks_binding(&test.hooks, text("unportedHook"), &output,
                                            NULL) &&
        output.kind == AB_UI_POINTER_NUMBER && output.number_bits == 123 &&
        ab_classic_collision_hooks_binding(&test.hooks, text("absentHook"), &nil,
                                           NULL) &&
        nil.kind == AB_UI_POINTER_NONE &&
        !ab_classic_collision_hooks_call(
            &test.hooks,
            (AbUiPointerValue){.kind = AB_UI_POINTER_FUNCTION, .function = UINTPTR_MAX},
            NULL, NULL, NULL));
    AbClassicCollisionFunction foreign = {&test, owned_callback};
    test.hooks.red_slot = &foreign;
    CHECK(!ab_classic_collision_hooks_binding(
              &test.hooks, text("flyingRedBirdCollided"), &output, NULL) &&
          output.number_bits == 123);
    bindings[1].name = bindings[0].name;
    CHECK(!ab_classic_collision_hooks_initialize(config, &test.hooks, NULL));
    puts("Collision hooks retain actual callback identities across rebinding/printing, "
         "read the live red flag, and reject unported or foreign calls without "
         "dereferencing them.");
    return true;
}

int main(void) {
    return check_hooks() ? 0 : 1;
}
