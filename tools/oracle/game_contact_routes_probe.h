#ifndef AB_ORACLE_GAME_CONTACT_ROUTES_H
#define AB_ORACLE_GAME_CONTACT_ROUTES_H

#include "../../tests/game_contact_route_cases.h"

static void contact_routes_print(const uint32_t *game, const uint32_t methods[7]) {
    uint32_t output[432];
    for (size_t stage = 0; stage < 3; ++stage) {
        for (size_t pair = 0; pair < 144; ++pair) {
            uint32_t method = game[0x104 / 4 + pair * 10 + 2 + stage * 3];
            size_t tag = 0;
            while (tag < 7 && methods[tag] != method) {
                ++tag;
            }
            if (tag == 7) {
                print_words(&method, 1);
                fail("Unclassified native contact handler method\n");
            }
            output[stage * 144 + pair] = (uint32_t)tag;
        }
    }
    for (size_t row = 0; row < 36; ++row) {
        print_words(output + row * 12, 12);
    }
}

static void probe_game_contact_routes(void) {
    typedef void (*Setup)(void *);
    typedef void (*Set)(void *, int, int, const uint32_t *);
    Setup setup = (Setup)find_function("_ZN7GameLua19setupCollisionTableEv");
    const Set setters[] = {
        (Set)find_function("_ZN7GameLua22setBeginContactHandlerE12ColliderTypeS0_"
                           "N4lang13FastDelegate1IP9b2ContactvEE"),
        (Set)find_function("_ZN7GameLua20setEndContactHandlerE12ColliderTypeS0_"
                           "N4lang13FastDelegate1IP9b2ContactvEE"),
        (Set)find_function("_ZN7GameLua18setPreSolveHandlerE12ColliderTypeS0_"
                           "N4lang13FastDelegate2IP9b2ContactPK10b2ManifoldvEE"),
    };
    const uint32_t methods[] = {
        0,
        (uint32_t)(uintptr_t)find_function(
            "_ZN7GameLua17basicBeginContactEP9b2Contact"),
        (uint32_t)(uintptr_t)find_function("_ZN7GameLua15basicEndContactEP9b2Contact"),
        (uint32_t)(uintptr_t)find_function(
            "_ZN7GameLua18portalBeginContactEP9b2Contact"),
        (uint32_t)(uintptr_t)find_function("_ZN7GameLua16portalEndContactEP9b2Contact"),
        (uint32_t)(uintptr_t)find_function(
            "_ZN7GameLua18bubbleBeginContactEP9b2Contact"),
        UINT32_C(0x10000),
    };
    static uint32_t game[0x1784 / 4];
    semihost(4, "AB_ANDROID_GAME_CONTACT_ROUTES_V1 512\n");
    for (uint32_t sample = 0; sample < 512; ++sample) {
        uint32_t input[436];
        game_contact_route_case(sample, input);
        for (size_t row = 0; row < 36; ++row) {
            print_words(input + row * 12, 12);
        }
        print_words(input + 432, 4);
        for (size_t stage = 0; stage < 3; ++stage) {
            for (size_t pair = 0; pair < 144; ++pair) {
                size_t slot = 0x104 / 4 + pair * 10 + 1 + stage * 3;
                uint32_t tag = input[stage * 144 + pair];
                game[slot] = tag ? (uint32_t)(uintptr_t)game : 0;
                game[slot + 1] = methods[tag];
                game[slot + 2] = 0;
            }
        }
        setup(game);
        contact_routes_print(game, methods);
        uint32_t tag = input[435];
        uint32_t delegate[] = {tag ? (uint32_t)(uintptr_t)game : 0, methods[tag], 0};
        setters[input[432]](game, (int)input[433], (int)input[434], delegate);
        contact_routes_print(game, methods);
    }
    semihost(4, "AB game contact routes reference complete\n");
}

#endif
