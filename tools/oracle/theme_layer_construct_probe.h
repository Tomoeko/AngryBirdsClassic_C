#ifndef AB_ORACLE_THEME_LAYER_CONSTRUCT_H
#define AB_ORACLE_THEME_LAYER_CONSTRUCT_H

static uint32_t theme_construct_sprite[16], theme_construct_present;

static void *theme_construct_get_sprite(void *resources, const void *name) {
    (void)resources;
    (void)name;
#if defined(AB_ORACLE_THEME_LAYER_CONSTRUCT_COMPOSITE)
    return NULL;
#else
    return theme_construct_present ? theme_construct_sprite : NULL;
#endif
}

static void theme_construct_value(void *lua, const uint32_t value[2]) {
    if (value[0] == 0) {
        damage_lua.push_nil(lua);
    } else if (value[0] < 3) {
        damage_lua.push_boolean(lua, value[0] == 2);
    } else if (value[0] == 3) {
        FloatBits number = {.bits = value[1]};
        damage_lua.push_number(lua, number.number);
    } else if (value[0] == 4) {
        damage_lua.create_table(lua, 0, 0);
    } else {
        fail("Unexpected native theme constructor value\n");
    }
}

static void probe_theme_layer(void) {
    initialize_damage_lua();
    typedef void (*Construct)(void *, void *);
    typedef void (*Destroy)(void *);
    typedef void (*Layer)(void *, const void *, void *, void *, void *);
    Construct construct_state =
        (Construct)find_function("_ZN3lua8LuaStateC1EP9lua_State");
    Construct construct_table =
        (Construct)find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    Destroy get_reference = (Destroy)find_function("_ZNK3lua6detail7RefBase6getRefEv");
    Destroy claim = (Destroy)find_function("_ZN4lang6Object5claimEv");
    Destroy destroy_table = (Destroy)find_function("_ZN3lua8LuaTableD1Ev");
    Destroy destroy_layer = (Destroy)find_function("_ZN10ThemeLayerD1Ev");
    void (*set_index)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    Layer construct_layer = (Layer)find_function(
        "_ZN10ThemeLayerC1ERKN3lua8LuaTableERN4game9ResourcesEP7GameLuaP9DrawUtils");
    uint32_t wrapper[8] = {0}, resources[0x4e0 / 4] = {0}, vtable[3] = {0};
    uint32_t cache_node[7] = {0}, entry[3] = {1, 0, 0};
    static const struct {
        uint32_t length, capacity, references;
        char bytes[7];
    } name = {6, 6, UINT32_MAX, "sprite"};
    resources[0] = (uint32_t)(uintptr_t)vtable;
    vtable[2] = (uint32_t)(uintptr_t)theme_construct_get_sprite;
    cache_node[4] = (uint32_t)(uintptr_t)name.bytes;
    cache_node[5] = (uint32_t)(uintptr_t)entry;
    cache_node[6] = (uint32_t)(uintptr_t)(entry + 3);
    entry[2] = (uint32_t)(uintptr_t)theme_construct_sprite;
#if defined(AB_ORACLE_THEME_LAYER_CONSTRUCT_COMPOSITE)
    uint32_t composite[15] = {0};
    entry[0] = 2;
    entry[2] = (uint32_t)(uintptr_t)composite;
#endif
    void *parent = damage_lua.new_state();
    if (!parent) {
        fail("Cannot allocate native theme constructor state\n");
    }
    construct_state(wrapper, parent);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    uint32_t random = UINT32_C(0x520c0a57);
#if defined(AB_ORACLE_THEME_LAYER_CONSTRUCT_COMPOSITE)
    semihost(4, "AB_ANDROID_THEME_LAYER_CONSTRUCT_V2 4096\n");
#else
    semihost(4, "AB_ANDROID_THEME_LAYER_CONSTRUCT_V1 4096\n");
#endif
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[17], table[6] = {0}, layer[24] = {0};
        input[0] = theme_construct_present = item % 7 != 0;
        input[1] = random_word(&random) % 1024 + 1;
        input[2] = random_word(&random) % 1024 + 1;
        input[3] = (uint32_t)((int32_t)(random_word(&random) % 257) - 128);
        input[4] = (uint32_t)((int32_t)(random_word(&random) % 257) - 128);
#if defined(AB_ORACLE_THEME_LAYER_CONSTRUCT_COMPOSITE)
        if (item % 4 == 0) {
            input[1] = input[2] = 1;
            input[3] = input[4] = UINT32_C(0x80000001);
        } else if (item % 4 == 1) {
            for (size_t index = 1; index < 5; ++index) {
                input[index] = random_word(&random);
            }
        }
        for (size_t index = 0; index < 4; ++index) {
            composite[11 + index] = input[1 + index];
        }
#endif
        theme_construct_sprite[5] = input[1] | (input[2] << 16);
        for (size_t index = 0; index < 2; ++index) {
            FloatBits value = {.number = (float)(int32_t)input[3 + index]};
            theme_construct_sprite[6 + index] = value.bits;
        }
        resources[0x4cc / 4] = input[0] ? (uint32_t)(uintptr_t)cache_node : 0;
        construct_table(table, wrapper);
        get_reference(table);
        damage_lua.push_string(lua, "sprite");
        set_index(lua, 1, 2);
        for (size_t field = 0; field < 6; ++field) {
            uint32_t *value = input + 5 + field * 2;
            value[0] = (uint32_t)((item + field * 3) % 5);
            value[1] = random_float(&random, 118, 16, 1);
            theme_construct_value(lua, value);
            if (field == 5) {
                damage_lua.set_field(lua, 1, "v");
            } else {
                set_index(lua, 1, (int)field + 3);
            }
        }
        damage_lua.set_top(lua, 0);
        construct_layer(layer, table, resources, NULL, NULL);
        print_words(input, 17);
        print_words(layer + 5, 19);
        destroy_layer(layer);
        destroy_table(table);
        if (damage_lua.get_top(lua) != 0) {
            fail("Native theme constructor changed Lua stack\n");
        }
    }
    semihost(4, "AB theme layer constructor reference complete\n");
}

#endif
