#ifndef AB_ORACLE_WATER_EFFECT_H
#define AB_ORACLE_WATER_EFFECT_H

static void water_effect_field(void *lua, int table, const char *name, uint32_t kind,
                               uint32_t bits) {
    if (kind == 0) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        FloatBits value = {.bits = bits};
        damage_lua.push_number(lua, value.number);
    } else {
        damage_lua.create_table(lua, 0, 0);
    }
    damage_lua.set_field(lua, table, name);
}

static void probe_water_effect(void) {
    typedef void *(*Allocate)(unsigned);
    typedef void (*Construct)(void *);
    typedef void (*TableConstruct)(void *, void *);
    typedef void (*Reference)(void *);
    typedef void (*WaterConstruct)(void *, const void *);
    Allocate allocate = (Allocate)find_function("_Znwj");
    Construct state_construct = (Construct)find_function("_ZN3lua8LuaStateC1Ev");
    TableConstruct table_construct =
        (TableConstruct)find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    Construct table_destroy = (Construct)find_function("_ZN3lua8LuaTableD1Ev");
    Reference claim = (Reference)find_function("_ZN4lang6Object5claimEv");
    Reference release = (Reference)find_function("_ZN4lang6Object7releaseEv");
    Reference get_reference =
        (Reference)find_function("_ZNK3lua6detail7RefBase6getRefEv");
    WaterConstruct construct =
        (WaterConstruct)find_function("_ZN11WaterEffectC1ERKN3lua8LuaTableE");
    initialize_damage_lua();
    uint32_t *wrapper = allocate(20);
    if (!wrapper) {
        fail("Cannot allocate original water-effect Lua wrapper\n");
    }
    state_construct(wrapper);
    claim(wrapper);
    uint32_t table[6] = {0};
    table_construct(table, wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    uint32_t random = UINT32_C(0x5207a7e2);
    semihost(4, "AB_ANDROID_WATER_EFFECT_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[8] = {(uint32_t)(item % 5),
                             random_float(&random, 112, 30, 1),
                             (uint32_t)((item / 5) % 5),
                             (uint32_t)((item / 25) % 5),
                             random_float(&random, 112, 30, 1),
                             (uint32_t)((item / 125) % 5),
                             (uint32_t)((item / 625) % 5),
                             random_float(&random, 112, 30, 1)};
        if (item < 512) {
            input[1] = item & 1 ? UINT32_C(0x80000000) : 0;
            input[4] = item & 2 ? UINT32_C(0x80000000) : 0;
            input[7] = item & 4 ? UINT32_C(0x80000000) : 0;
        }
        get_reference(table);
        water_effect_field(lua, 1, "waterLevel", input[0], input[1]);
        if (input[2] == 4) {
            damage_lua.create_table(lua, 0, 3);
            water_effect_field(lua, 2, "waterLevel", input[3], input[4]);
            water_effect_field(lua, 2, "freezing", input[5], input[4]);
            water_effect_field(lua, 2, "killLevel", input[6], input[7]);
            damage_lua.set_field(lua, 1, "water");
        } else {
            water_effect_field(lua, 1, "water", input[2], input[4]);
        }
        damage_lua.set_top(lua, 0);
        uint32_t raw[3] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
        construct(raw, table);
        const uint8_t *flags = (const uint8_t *)raw;
        uint32_t enabled = flags[0] != 0;
        uint32_t kill = enabled && flags[1] != 0;
        uint32_t output[] = {enabled, enabled && flags[2] != 0, kill,
                             enabled ? raw[1] : 0, kill ? raw[2] : 0};
        print_words(input, 8);
        print_words(output, 5);
    }
    table_destroy(table);
    release(wrapper);
    semihost(4, "AB water effect reference complete\n");
}

#endif
