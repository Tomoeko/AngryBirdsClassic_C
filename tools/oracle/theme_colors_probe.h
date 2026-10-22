#ifndef AB_ORACLE_THEME_COLORS_H
#define AB_ORACLE_THEME_COLORS_H

static void probe_theme_colors(void) {
    initialize_damage_lua();
    typedef void (*Construct)(void *, void *);
    typedef void (*Destroy)(void *);
    typedef void (*Composition)(void *, const void *, void *, void *, void *);
    Construct construct_state =
        (Construct)find_function("_ZN3lua8LuaStateC1EP9lua_State");
    Construct construct_table =
        (Construct)find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    Destroy get_reference = (Destroy)find_function("_ZNK3lua6detail7RefBase6getRefEv");
    Destroy claim = (Destroy)find_function("_ZN4lang6Object5claimEv");
    Destroy destroy_table = (Destroy)find_function("_ZN3lua8LuaTableD1Ev");
    Composition construct_composition =
        (Composition)find_function("_ZN16ThemeCompositionC1ERKN3lua8LuaTableERN4game9Re"
                                   "sourcesEP7GameLuaP9DrawUtils");
    uint32_t wrapper[8] = {0};
    void *parent = damage_lua.new_state();
    if (!parent) {
        fail("Cannot allocate native theme color state\n");
    }
    construct_state(wrapper, parent);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    uint32_t random = UINT32_C(0x520c0104);
    const char *const groups[] = {"color", "groundColor", "backgroundTintColor"};
    const char *const channels[] = {"r", "g", "b"};
    semihost(4, "AB_ANDROID_THEME_COLORS_V1 8192\n");
    for (size_t item = 0; item < 8192; ++item) {
        uint32_t input[10], table[6] = {0}, composition[17] = {0};
        input[0] = (uint32_t)(item % 6);
        for (size_t index = 1; index < 10; ++index) {
            FloatBits value = {
                .number = (float)((int32_t)(random_word(&random) % 12289) - 4096) / 16};
            if (item % 13 == 0) {
                value.bits = random_float(&random, 118, 48, 1);
            }
            if ((item + index) % 31 == 0) {
                value.bits &= UINT32_C(0x80000000);
            }
            input[index] = value.bits;
        }
        construct_table(table, wrapper);
        get_reference(table);
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, 1, "bgLayers");
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, 1, "fgLayers");
        for (size_t group = 0; group < 3; ++group) {
            if (group == 2 && input[0] != 5) {
                if (input[0] == 0) {
                    damage_lua.push_nil(lua);
                } else if (input[0] < 3) {
                    damage_lua.push_boolean(lua, input[0] == 2);
                } else if (input[0] == 3) {
                    damage_lua.push_number(lua, 0);
                } else {
                    damage_lua.push_string(lua, "unused");
                }
            } else {
                damage_lua.create_table(lua, 0, 3);
                for (size_t channel = 0; channel < 3; ++channel) {
                    damage_number(lua, 2, channels[channel],
                                  input[1 + group * 3 + channel]);
                }
            }
            damage_lua.set_field(lua, 1, groups[group]);
        }
        damage_lua.set_top(lua, 0);
        construct_composition(composition, table, NULL, NULL, NULL);
        /* Both vectors are empty, so this stack composition owns no entries or
         * vector storage. The target destructor is inlined into its callers. */
        for (size_t index = 2; index < 8; ++index) {
            if (composition[index]) {
                fail("Native color fixture unexpectedly owns layer storage\n");
            }
        }
        print_words(input, 10);
        print_words(composition + 8, 9);
        destroy_table(table);
        if (damage_lua.get_top(lua) != 0) {
            fail("Native theme color constructor changed Lua stack\n");
        }
    }
    semihost(4, "AB theme colors reference complete\n");
}

#endif
