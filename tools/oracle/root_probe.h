#ifndef AB_ORACLE_ROOT_H
#define AB_ORACLE_ROOT_H

static unsigned rt_case, rt_step, rt_count;
static uint32_t rt_trace[128][18];
static void (*rt_raw_set)(void *, int, int);

static void rt_value(void *lua, int index, uint32_t *output) {
    int kind = damage_lua.type(lua, index);
    output[0] = (uint32_t)kind;
    output[1] = 0;
    if (kind == 3) {
        FloatBits number = {.number = damage_lua.to_number(lua, index)};
        output[1] = number.bits;
    } else if (kind == 1)
        output[1] = (uint32_t)damage_lua.to_boolean(lua, index);
    else if (kind != 0 && kind != 4 && kind != 5)
        fail("Unsupported original root value\n");
}

static void rt_global(void *lua, const char *name, uint32_t *output) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, name);
    rt_value(lua, -1, output);
    damage_lua.set_top(lua, top);
}

static int rt_boundary(void *lua) {
    unsigned kind = (unsigned)damage_lua.to_number(lua, -10003);
    int skip = (int)damage_lua.to_number(lua, -10004);
    int top = damage_lua.get_top(lua), count = top - skip;
    if (count < 0 || count > 6 || rt_count == 128 ||
        (skip && damage_lua.type(lua, 1) != 5))
        fail("Unexpected original root boundary arguments\n");
    if (kind == 70) {
        damage_lua.get_field(lua, 2, "id");
        kind += damage_named(lua, -1, "f") ? 1u : 2u;
        if (kind == 72 && !damage_named(lua, -1, "r"))
            fail("Unexpected original root event ID\n");
        damage_lua.set_top(lua, top);
        count = 0;
    }
    uint32_t *row = rt_trace[rt_count++];
    for (unsigned index = 0; index < 18; ++index)
        row[index] = 0;
    row[0] = kind;
    row[1] = (uint32_t)count;
    rt_global(lua, "g_time", row + 2);
    rt_global(lua, "time", row + 4);
    for (int index = 0; index < count; ++index)
        rt_value(lua, index + skip + 1, row + 6 + (unsigned)index * 2);
    if (kind == 1) {
        damage_lua.push_boolean(lua, (rt_case & 1) != 0);
        return 1;
    }
    if (kind == 10) {
        if (!damage_named(lua, 1, "S"))
            fail("Unexpected original root released-key query\n");
        if ((rt_case & 2) && rt_step == 2)
            damage_lua.push_number(lua, 0);
        else
            damage_lua.push_boolean(lua, 0);
        return 1;
    }
    if (kind == 11 && !damage_named(lua, 1, "KEYBOARD_COMMAND"))
        fail("Unexpected original root audio command\n");
    if (kind == 13) {
        damage_lua.push_boolean(lua, 0);
        return 1;
    }
    if (kind == 14 || kind == 22) {
        damage_lua.push_nil(lua);
        return 1;
    }
    if (kind == 18 && (rt_case & 8) && rt_step == 2) {
        damage_lua.push_number(lua, 7);
        damage_lua.set_field(lua, -10002, "time");
    }
    return 0;
}

static void rt_bind(void *lua, int table, const char *name, unsigned kind, int self) {
    damage_lua.push_number(lua, (float)kind);
    damage_lua.push_number(lua, (float)self);
    damage_lua.push_closure(lua, rt_boundary, 2);
    damage_lua.set_field(lua, table, name);
}

static void rt_flag(void *lua, const char *name, int value) {
    damage_lua.push_boolean(lua, value);
    damage_lua.set_field(lua, -10002, name);
}

static void rt_table(void *lua, const char *name, const char *method, unsigned kind,
                     int self) {
    damage_lua.create_table(lua, 0, 1);
    rt_bind(lua, 1, method, kind, self);
    damage_lua.set_field(lua, -10002, name);
}

static void rt_list(void *lua, const char *name, unsigned first, unsigned count,
                    int subsystems) {
    damage_lua.create_table(lua, (int)count, 0);
    for (unsigned index = 0; index < count; ++index) {
        if (subsystems) {
            damage_lua.create_table(lua, 0, 1);
            rt_bind(lua, 2, "update", first + index, 1);
        } else {
            damage_lua.push_number(lua, (float)(first + index));
            damage_lua.push_number(lua, 0);
            damage_lua.push_closure(lua, rt_boundary, 2);
        }
        rt_raw_set(lua, 1, (int)index + 1);
    }
    if (name)
        damage_lua.set_field(lua, -10002, name);
}

static void rt_clock(void *lua, const char *name, unsigned profile) {
    if (!profile)
        damage_lua.push_nil(lua);
    else if (profile == 1)
        damage_lua.push_boolean(lua, 0);
    else
        damage_lua.push_number(lua, profile == 2 ? 0 : 16777216.0f);
    damage_lua.set_field(lua, -10002, name);
}

static void rt_prepare(void *lua) {
    damage_lua.set_top(lua, 0);
    rt_bind(lua, -10002, "isSafeToQuit", 1, 0);
    rt_table(lua, "AngerLevel", "update", 3, 0);
    rt_bind(lua, -10002, "setOffsetedViewport", 4, 0);
    rt_bind(lua, -10002, "setScreenClipRect", 5, 0);
    rt_bind(lua, -10002, "openURL", 7, 0);
    rt_bind(lua, -10002, "setFullScreenMode", 8, 0);
    rt_table(lua, "URLRequests", "updateURLCallbackTimers", 9, 1);
    rt_bind(lua, -10002, "wasKeyReleased", 10, 0);
    rt_bind(lua, -10002, "changeAudio", 11, 0);
    rt_bind(lua, -10002, "updateAudioRamp", 12, 0);
    rt_bind(lua, -10002, "isFreeEagleEnabled", 13, 0);
    rt_bind(lua, -10002, "setRenderState", 24, 0);
    rt_table(lua, "AnimationWrapperNative", "update", 62, 0);
    rt_table(lua, "processManager", "update", 61, 1);
    damage_lua.create_table(lua, 0, 2);
    rt_bind(lua, 1, "notify", 70, 1);
    rt_bind(lua, 1, "tick", 60, 1);
    damage_lua.set_field(lua, -10002, "eventManager");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_string(lua, "f");
    damage_lua.set_field(lua, 1, "EID_TOGGLE_FULLSCREEN");
    damage_lua.push_string(lua, "r");
    damage_lua.set_field(lua, 1, "EID_RESOLUTION_CHANGED");
    damage_lua.set_field(lua, -10002, "events");
    damage_lua.create_table(lua, 0, 4);
    rt_bind(lua, 1, "update", 15, 1);
    rt_bind(lua, 1, "setAllowInput", 16, 1);
    rt_bind(lua, 1, "draw", 19, 1);
    rt_bind(lua, 1, "getRoot", 22, 1);
    damage_lua.set_field(lua, -10002, "menuManager");
    rt_table(lua, "adSystem", "update", 17, 1);
    rt_table(lua, "notificationsFrame", "layout", 21, 1);
    rt_bind(lua, -10002, "currentGameMode", 18, 0);
    rt_flag(lua, "releaseBuild", 1);
    rt_flag(lua, "cheatsEnabled", (rt_case & 8) != 0);
    rt_flag(lua, "showEditor", 0);
    rt_flag(lua, "blockAudioChange", 0);
    rt_flag(lua, "g_settingsConverted", 1);
    rt_flag(lua, "hasIaps", 1);
    if (rt_case & 1)
        rt_table(lua, "settingsWrapper", "getEagleUsedTime", 14, 1);
    else {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "settingsWrapper");
    }
    if (rt_case & 8)
        rt_table(lua, "gamerService", "update", 2, 1);
    else {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "gamerService");
    }
    rt_flag(lua, "GAMERSERVICE_DEVMODE", 1);
    rt_clock(lua, "g_time", rt_case & 3);
    rt_clock(lua, "time", rt_case & 3);
    rt_clock(lua, "playtimeCounter", rt_case & 3);
    const char *nil_fields[] = {"g_gameWasResumed", "g_restoreFullScreenOnReactivate",
                                "lastVideoAdTime",  "doubleClickTimer",
                                "doubleClickState", "oldScreenWidth",
                                "oldScreenHeight",  "ShopOffer"};
    for (unsigned index = 0; index < sizeof(nil_fields) / sizeof(*nil_fields);
         ++index) {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, nil_fields[index]);
    }
    damage_number(lua, -10002, "g_enableInputDelay",
                  rt_case & 4 ? 0 : UINT32_C(0x3e800000));
    damage_number(lua, -10002, "eagleInfoTimer", UINT32_C(0x3f800000));
    rt_flag(lua, "showBG", 0);
    damage_lua.push_string(lua, rt_case & 4 ? "osx" : "android");
    damage_lua.set_field(lua, -10002, "deviceModel");
    if (rt_case & 4) {
        rt_flag(lua, "lastVideoAdTime", 0);
        damage_number(lua, -10002, "oldScreenWidth", UINT32_C(0x43f00000));
        damage_number(lua, -10002, "oldScreenHeight", UINT32_C(0x43a00000));
        rt_table(lua, "ShopOffer", "update", 25, 0);
    }
    if (rt_case & 8) {
        damage_number(lua, -10002, "g_gameWasResumed", 0);
        damage_number(lua, -10002, "g_restoreFullScreenOnReactivate",
                      UINT32_C(0x40000000));
    }
    const char *dimensions[] = {"screenWidth", "screenHeight", "g_updatedScreenWidth",
                                "g_updatedScreenHeight"};
    for (unsigned index = 0; index < 4; ++index)
        damage_number(lua, -10002, dimensions[index],
                      index & 1 ? UINT32_C(0x43a00000) : UINT32_C(0x43f00000));
    rt_table(lua, "debugUtils", "update", 6, 0);
    damage_lua.get_field(lua, -10002, "debugUtils");
    rt_bind(lua, 1, "draw", 23, 0);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 1, "time", UINT32_C(0x3e800000));
    damage_lua.push_string(lua, "ab-reference://root");
    damage_lua.set_field(lua, 1, "url");
    damage_lua.set_field(lua, -10002, "g_delayedURLOpen");
    rt_list(lua, "subSystemsList", 100, 13, 1);
    rt_list(lua, "g_updatableFunctions", 200, rt_case & 4 ? 2u : 0u, 0);
    rt_list(lua, "_ab_root_tail", 300, rt_case & 8 ? 2u : 0u, 0);
    damage_lua.get_field(lua, -10002, "_ab_root");
    damage_lua.get_field(lua, -10002, "_ab_root_tail");
    const char *(*setup)(void *, int, int) =
        (const char *(*)(void *, int, int))find_function(
            "_Z14lua_setupvalueP9lua_Stateii");
    if (!setup(lua, 1, 1))
        fail("Cannot bind original root tail registry\n");
    damage_lua.set_top(lua, 0);
}

static void rt_keys(void *lua) {
    const char *tables[] = {"keyHold", "keyPressed", "keyReleased"};
    for (unsigned index = 0; index < 3; ++index) {
        damage_lua.create_table(lua, 0, 3);
        if ((index == 1 && (rt_step == 1 || rt_step == 3)) ||
            (index == 2 && (rt_step == 2 || rt_step == 4))) {
            if (rt_case & 2)
                damage_lua.push_number(lua, 0);
            else
                damage_lua.push_boolean(lua, 1);
            damage_lua.set_field(lua, 1, "LBUTTON");
        }
        if (rt_step == 1 && (rt_case & 8) && index < 2) {
            damage_lua.push_boolean(lua, 1);
            damage_lua.set_field(lua, 1, index ? "F" : "CONTROL");
        }
        if (rt_step == 4 && (rt_case & 2) && index == 1) {
            damage_lua.push_number(lua, 0);
            damage_lua.set_field(lua, 1, "9");
        }
        damage_lua.set_field(lua, -10002, tables[index]);
    }
}

static void rt_snapshot(void *lua) {
    static const char *fields[] = {"g_safeToQuit",
                                   "g_dt",
                                   "g_time",
                                   "time",
                                   "playtimeCounter",
                                   "lastVideoAdTime",
                                   "g_enableInputDelay",
                                   "doubleClick",
                                   "doubleClickTimer",
                                   "doubleClickState",
                                   "showBG",
                                   "g_gameWasResumed",
                                   "g_restoreFullScreenOnReactivate",
                                   "g_updatedScreenWidth",
                                   "g_updatedScreenHeight",
                                   "oldScreenWidth",
                                   "oldScreenHeight",
                                   "eagleInfoTimer",
                                   "g_delayedURLOpen"};
    uint32_t header[] = {rt_case, rt_step, rt_count};
    print_words(header, 3);
    for (unsigned index = 0; index < sizeof(fields) / sizeof(*fields); ++index) {
        uint32_t value[2];
        rt_global(lua, fields[index], value);
        print_words(value, 2);
    }
    const char *geometry[] = {"screenWidth", "screenHeight", "g_touchesOffsetX",
                              "g_touchesOffsetY"};
    uint32_t dimensions[4];
    for (unsigned index = 0; index < 4; ++index) {
        uint32_t value[2];
        rt_global(lua, geometry[index], value);
        if (value[0] != 3)
            fail("Missing original root geometry\n");
        dimensions[index] = value[1];
    }
    print_words(dimensions, 4);
    for (unsigned index = 0; index < rt_count; ++index)
        for (unsigned part = 0; part < 3; ++part)
            print_words(rt_trace[index] + part * 6, 6);
}

static void probe_root(void) {
    initialize_damage_lua();
    rt_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original root state\n");
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/root-update.lua", "_ab_root");
    semihost(4, "AB_ANDROID_ROOT_V1 16 4\n");
    static const float seconds[] = {0.125f, 0.125f, 0, 0.0625f};
    for (rt_case = 0; rt_case < 16; ++rt_case) {
        rt_prepare(lua);
        for (rt_step = 1; rt_step <= 4; ++rt_step) {
            rt_count = 0;
            rt_keys(lua);
            damage_lua.get_field(lua, -10002, "_ab_root");
            damage_lua.push_number(lua, seconds[rt_step - 1]);
            damage_lua.push_number(lua, 99);
            if (damage_lua.call(lua, 2, 0, 0)) {
                const char *error = damage_lua.to_string(lua, -1, NULL);
                fail(error ? error : "Original root update failed\n");
            }
            rt_snapshot(lua);
            damage_lua.set_top(lua, 0);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB root reference complete\n");
}

#endif
