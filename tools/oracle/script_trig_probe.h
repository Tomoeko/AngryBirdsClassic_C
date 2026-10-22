#ifndef AB_ORACLE_SCRIPT_TRIG_H
#define AB_ORACLE_SCRIPT_TRIG_H

static void probe_script_trig(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original script trigonometry reference state\n");
    }
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    const char *names[] = {"sin", "cos"};
    uint32_t random = UINT32_C(0x520c051e);
    static const uint32_t edges[] = {
        0,          0x80000000, 1,          0x80000001, 0x007fffff, 0x807fffff,
        0x39800000, 0xb9800000, 0x3fc90fda, 0x3fc90fdb, 0x3fc90fdc, 0xbfc90fda,
        0xbfc90fdb, 0xbfc90fdc, 0x40490fda, 0x40490fdb, 0x40490fdc, 0xc0490fda,
        0xc0490fdb, 0xc0490fdc, 0x44800000, 0xc4800000};
    semihost(4, "AB_ANDROID_SCRIPT_TRIG_V1 131072\n");
    for (size_t item = 0; item < 131072; ++item) {
        FloatBits angle;
        if (item < 65536) {
            angle.number = (float)((int)item - 32768) * 0.03125f;
        } else {
            angle.bits = random_float(&random, 0, 137, 1);
        }
        if (item < sizeof(edges) / sizeof(*edges)) {
            angle.bits = edges[item];
        }
        uint32_t output[3] = {angle.bits, 0, 0};
        for (size_t function = 0; function < 2; ++function) {
            damage_lua.get_field(lua, -10002, "math");
            damage_lua.get_field(lua, 1, names[function]);
            damage_lua.push_number(lua, angle.number);
            if (damage_lua.call(lua, 1, 1, 0) || damage_lua.type(lua, 2) != 3) {
                fail("Original script trigonometry call failed\n");
            }
            FloatBits value = {.number = damage_lua.to_number(lua, 2)};
            output[function + 1] = value.bits;
            damage_lua.set_top(lua, 0);
        }
        print_words(output, 3);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB script trigonometry reference complete\n");
}

#endif
