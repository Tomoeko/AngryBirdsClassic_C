#ifndef AB_ORACLE_BODY_VELOCITY_H
#define AB_ORACLE_BODY_VELOCITY_H

static void probe_body_velocity(void) {
    typedef void *(*GetBody)(void *, const char **);
    typedef void (*SetVelocity)(void *, const char **, float, float);
    typedef void (*SetAngularVelocity)(void *, const char **, float);
    typedef void (*ApplyImpulse)(void *, void *);
    GetBody get_body = (GetBody)find_function("_ZN7GameLua7getBodyERKSs");
    SetVelocity set_velocity =
        (SetVelocity)find_function("_ZN7GameLua11setVelocityESsff");
    SetAngularVelocity set_angular =
        (SetAngularVelocity)find_function("_ZN7GameLua18setAngularVelocityESsf");
    ApplyImpulse apply_impulse =
        (ApplyImpulse)find_function("_ZN7GameLua12applyImpulseEPN3lua8LuaStateE");
    static uint32_t game[0x1800 / 4], object[50], body[38], node[6];
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } name = {5, 5, UINT32_C(0xffffffff), "first"};
    const char *identifier = name.bytes;
    uint32_t header = (uint32_t)(uintptr_t)((uint8_t *)game + 0x17a8);
    node[0] = 1;
    node[1] = header;
    node[4] = (uint32_t)(uintptr_t)name.bytes;
    node[5] = (uint32_t)(uintptr_t)object;
    game[0x17ac / 4] = (uint32_t)(uintptr_t)node;
    game[0x17b0 / 4] = (uint32_t)(uintptr_t)node;
    game[0x17b4 / 4] = (uint32_t)(uintptr_t)node;
    game[0x17b8 / 4] = 1;
    object[11] = (uint32_t)(uintptr_t)body;
    if (get_body(game, &identifier) != body) {
        fail("Controlled native GameLua body lookup failed\n");
    }
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate native impulse argument state\n");
    }
    uint32_t wrapper[3] = {0, 0, (uint32_t)(uintptr_t)lua};
    uint32_t state = UINT32_C(0x520b0d71);
#if defined(AB_ORACLE_BODY_ANGULAR)
    semihost(4, "AB_ANDROID_BODY_ANGULAR_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_BODY_VELOCITY_V1 4096\n");
#endif
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t input[17] = {(uint32_t)(index % 3), (uint32_t)((index / 3) % 3),
                              (uint32_t)(index & 1)};
#if defined(AB_ORACLE_BODY_ANGULAR)
        input[0] = 3;
#endif
        for (size_t field = 3; field < 17; ++field) {
            input[field] =
                random_float(&state, 122, 7, field != 6 && field != 7 && field != 8);
        }
        if (index % 7 == 0) {
            input[13] = index & 1 ? UINT32_C(0x80000000) : 0;
            input[14] = 0;
        } else if (index % 13 == 0) {
            input[13] = 1;
            input[14] = UINT32_C(0x80000001);
        }
        if (index % 11 == 0) {
            input[7] = 0;
            input[8] = 0;
        }
        if (index < 32) {
            input[1] = 2;
            input[3] = index & 1 ? UINT32_C(0x80000000) : 0;
            input[4] = index & 2 ? UINT32_C(0x80000000) : 0;
            input[5] = index & 4 ? UINT32_C(0x80000000) : 0;
            input[7] = UINT32_C(0x3f800000);
            input[8] = UINT32_C(0x3f800000);
            for (size_t field = 9; field < 17; ++field) {
                input[field] =
                    index & ((field & 1) ? 8u : 16u) ? UINT32_C(0x80000000) : 0;
            }
        }
        for (size_t field = 0; field < 38; ++field) {
            body[field] = 0;
        }
        body[0] = input[1];
        body[1] = 36 | (input[2] ? 2u : 0u);
        body[3] = input[11];
        body[4] = input[12];
        body[11] = input[9];
        body[12] = input[10];
        body[16] = input[3];
        body[17] = input[4];
        body[18] = input[5];
        body[30] = input[7];
        body[32] = input[8];
        body[36] = input[6];
        print_words(input, 17);
        FloatBits horizontal = {.bits = input[13]}, vertical = {.bits = input[14]};
        if (input[0] == 3) {
            set_angular(game, &identifier, horizontal.number);
        } else if (!input[0]) {
            set_velocity(game, &identifier, horizontal.number, vertical.number);
        } else {
            damage_lua.set_top(lua, 0);
            damage_lua.push_string(lua, "first");
            damage_lua.push_number(lua, horizontal.number);
            damage_lua.push_number(lua, vertical.number);
            if (input[0] == 2) {
                FloatBits x = {.bits = input[15]}, y = {.bits = input[16]};
                damage_lua.push_number(lua, x.number);
                damage_lua.push_number(lua, y.number);
            }
            apply_impulse(game, wrapper);
        }
        uint32_t output[] = {body[1], body[16], body[17], body[18], body[36]};
        print_words(output, 5);
    }
    damage_lua.close_state(lua);
#if defined(AB_ORACLE_BODY_ANGULAR)
    semihost(4, "AB body angular reference complete\n");
#else
    semihost(4, "AB body velocity reference complete\n");
#endif
}

#endif
