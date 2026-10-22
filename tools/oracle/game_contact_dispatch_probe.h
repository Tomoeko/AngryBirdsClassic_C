#ifndef AB_ORACLE_GAME_CONTACT_DISPATCH_H
#define AB_ORACLE_GAME_CONTACT_DISPATCH_H

#include "../../tests/game_contact_dispatch_cases.h"

typedef struct {
    uint32_t token, stage;
} DispatchContext;

static uint32_t dispatch_output[5];
static const void *dispatch_contact, *dispatch_previous;

static void dispatch_record(void *context, void *contact, const void *previous) {
    const DispatchContext *entry = context;
    if (!entry || contact != dispatch_contact || previous != dispatch_previous ||
        ++dispatch_output[0] != 1) {
        fail("Unexpected original contact delegate invocation\n");
    }
    dispatch_output[1] = entry->token;
    dispatch_output[2] = contact == dispatch_contact;
    dispatch_output[3] = previous != NULL;
    dispatch_output[4] = entry->stage;
}

static void dispatch_simple(void *context, void *contact) {
    dispatch_record(context, contact, NULL);
}

static void dispatch_pre_solve(void *context, void *contact, const void *previous) {
    dispatch_record(context, contact, previous);
}

static void probe_game_contact_dispatch(void) {
    typedef void (*Simple)(void *, void *);
    typedef void (*PreSolve)(void *, void *, const void *);
    const Simple begin = (Simple)find_function("_ZN7GameLua12BeginContactEP9b2Contact");
    const Simple end = (Simple)find_function("_ZN7GameLua10EndContactEP9b2Contact");
    const PreSolve pre_solve =
        (PreSolve)find_function("_ZN7GameLua8PreSolveEP9b2ContactPK10b2Manifold");
    static uint32_t game[0x1860 / 4], objects[2][50], fixtures[2][11], contact[37];
    static uint32_t previous[16];
    static DispatchContext contexts[432];
    for (size_t stage = 0; stage < 3; ++stage) {
        for (size_t pair = 0; pair < 144; ++pair) {
            size_t index = stage * 144 + pair;
            contexts[index] = (DispatchContext){(uint32_t)index + 1, (uint32_t)stage};
        }
    }
    contact[12] = (uint32_t)(uintptr_t)fixtures[0];
    contact[13] = (uint32_t)(uintptr_t)fixtures[1];
    dispatch_contact = contact;
    semihost(4, "AB_ANDROID_GAME_CONTACT_DISPATCH_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[5];
        game_contact_dispatch_case(sample, input);
        uint32_t stage = input[0], flags = input[3];
        size_t pair = input[1] * 12u + input[2];
        size_t slot = 0x104 / 4 + pair * 10 + 1 + stage * 3;
        game[slot] =
            flags & 8u ? (uint32_t)(uintptr_t)(contexts + stage * 144 + pair) : 0;
        uint32_t method = stage == 2 ? (uint32_t)(uintptr_t)dispatch_pre_solve
                                     : (uint32_t)(uintptr_t)dispatch_simple;
        game[slot + 1] = flags & 8u ? method : 0;
        game[slot + 2] = 0;
        objects[0][0] = input[1];
        objects[1][0] = input[2];
        fixtures[0][10] = flags & 1u ? (uint32_t)(uintptr_t)objects[0] : 0;
        fixtures[1][10] = flags & 2u ? (uint32_t)(uintptr_t)objects[1] : 0;
        ((uint8_t *)game)[0x185d] = (flags & 4u) != 0;
        dispatch_previous = stage == 2 && (flags & 16u) ? previous : NULL;
        for (size_t field = 0; field < 5; ++field) {
            dispatch_output[field] = 0;
        }
        print_words(input, 5);
        if (stage == 0) {
            begin(game, contact);
        } else if (stage == 1) {
            end(game, contact);
        } else {
            pre_solve(game, contact, dispatch_previous);
        }
        print_words(dispatch_output, 5);
    }
    semihost(4, "AB game contact dispatch reference complete\n");
}

#endif
