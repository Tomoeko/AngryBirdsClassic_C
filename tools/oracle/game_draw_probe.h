#ifndef AB_ORACLE_GAME_DRAW_PROBE_H
#define AB_ORACLE_GAME_DRAW_PROBE_H

/* Run the original selected drawGame prototype in an isolated native Lua state.
 * Record controlled service boundaries, not a live game/GPU frame. The world
 * and extra registries are explicitly empty; host comparison supplies the real
 * cached producers in the resulting original order. */
static uint32_t gd_case, gd_count, gd_events[32][8];

static int gd_boundary(void *lua) {
    uint32_t kind = (uint32_t)damage_lua.to_number(lua, -10003);
    int count = damage_lua.get_top(lua);
    if (!kind || kind > 17 || count < 0 || count > 5 || gd_count == 32)
        fail("Unexpected original drawGame boundary\n");
    uint32_t *event = gd_events[gd_count++];
    for (size_t word = 0; word < 8; ++word)
        event[word] = 0;
    event[0] = kind;
    event[1] = (uint32_t)count;
    for (int argument = 1; argument <= count; ++argument) {
        int type = damage_lua.type(lua, argument);
        event[7] |= (uint32_t)type << (3u * (unsigned)(argument - 1));
        if (type == 3) {
            FloatBits value = {.number = damage_lua.to_number(lua, argument)};
            event[(size_t)argument + 1] = value.bits;
        } else if (type == 1) {
            event[(size_t)argument + 1] =
                (uint32_t)damage_lua.to_boolean(lua, argument);
        } else if (type != 5 &&
                   !(kind == 3 && damage_named(lua, argument, "FONT_BASIC"))) {
            fail("Unexpected original drawGame boundary value\n");
        }
    }
    if (kind == 4) {
        if (gd_case & 1)
            damage_lua.push_number(lua, 0);
        else
            damage_lua.push_boolean(lua, 0);
        return 1;
    }
    if (kind == 6) {
        damage_lua.push_number(lua, 1);
        return 1;
    }
    return 0;
}

static void gd_function(void *lua, int table, const char *name, unsigned kind) {
    damage_lua.push_number(lua, (float)kind);
    damage_lua.push_closure(lua, gd_boundary, 1);
    damage_lua.set_field(lua, table, name);
}

static void gd_table(void *lua, const char *name, const char *method, unsigned kind) {
    damage_lua.create_table(lua, 0, 1);
    gd_function(lua, 1, method, kind);
    damage_lua.set_field(lua, -10002, name);
}

static void gd_prepare(void *lua) {
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 3);
    damage_number(lua, 4, "r", UINT32_C(0x43120000));
    damage_number(lua, 4, "g", UINT32_C(0x434c0000));
    damage_number(lua, 4, "b", UINT32_C(0x435e0000));
    damage_lua.set_field(lua, 3, "color");
    damage_lua.set_field(lua, 2, "theme1");
    damage_lua.set_field(lua, 1, "themes");
    damage_lua.set_field(lua, -10002, "blockTable");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_string(lua, "theme1");
    damage_lua.set_field(lua, 1, "theme");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_field(lua, -10002, "objects");
    const char *empty[] = {"g_extra_draw_functions", "currentGameMode", "updateEditor"};
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, -10002, empty[index]);
    }
    damage_lua.push_string(lua, "theme1");
    damage_lua.set_field(lua, -10002, "currentTheme");
    damage_lua.push_string(lua, "FONT_BASIC");
    damage_lua.set_field(lua, -10002, "fontBasic");
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, -10002, "g_powerups_enabled");
    damage_lua.push_boolean(lua, (gd_case & 8) != 0);
    damage_lua.set_field(lua, -10002, "g_levelParticlesEnabled");
    damage_lua.push_nil(lua);
    damage_lua.set_field(lua, -10002, "popupPage");
    damage_lua.push_number(lua, 1.25f);
    damage_lua.set_field(lua, -10002, "zoomLevel");
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, -10002, "hasExtraZOrderedDraws");
    if (gd_case & 2) {
        damage_lua.create_table(lua, 0, 4);
        damage_number(lua, 1, "r", UINT32_C(0x3dcccccd));
        damage_number(lua, 1, "g", UINT32_C(0x3e4ccccd));
        damage_number(lua, 1, "b", UINT32_C(0x3e99999a));
        damage_number(lua, 1, "a", UINT32_C(0x3ecccccd));
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, -10002, "g_background_overlay_color");
    if (gd_case & 4)
        damage_lua.push_number(lua, 0.25f);
    else
        damage_lua.push_nil(lua);
    damage_lua.set_field(lua, -10002, "eagleDarkness");
    damage_lua.get_field(lua, -10002, "_ab_draw");
    damage_lua.create_table(lua, 0, 4);
    gd_function(lua, 2, "calculateRubberBand", 7);
    gd_function(lua, 2, "drawBackPart", 8);
    gd_function(lua, 2, "drawSlingHolder", 12);
    gd_function(lua, 2, "drawFrontPart", 15);
    const char *(*setup)(void *, int, int) =
        (const char *(*)(void *, int, int))find_function(
            "_Z14lua_setupvalueP9lua_Stateii");
    if (!setup(lua, 1, 3))
        fail("Cannot bind original drawGame sling upvalue\n");
    damage_lua.set_top(lua, 0);
}

#if !defined(AB_ORACLE_EXTRA_DRAW_ONLY)
static void gd_value(void *lua, const char *name, uint32_t words[2]) {
    damage_lua.get_field(lua, -10002, name);
    words[0] = (uint32_t)damage_lua.type(lua, -1);
    words[1] = 0;
    if (words[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        words[1] = value.bits;
    } else if (words[0] == 1) {
        words[1] = (uint32_t)damage_lua.to_boolean(lua, -1);
    } else if (words[0] != 0) {
        fail("Unexpected original drawGame result type\n");
    }
    damage_lua.set_top(lua, 0);
}

static void probe_game_draw(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot open isolated drawGame state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/draw-game/original.lua",
                           "_ab_draw");
    const char *names[] = {"setBGColor",
                           "drawFullscreenRect",
                           "setFont",
                           "drawParticlesNative",
                           "drawLevelParticlesNative",
                           "setRenderState",
                           "drawBordersForRestrictedScreenArea"};
    const unsigned kinds[] = {1, 2, 3, 11, 13, 16, 17};
    for (size_t index = 0; index < 7; ++index)
        gd_function(lua, -10002, names[index], kinds[index]);
    gd_table(lua, "settingsWrapper", "isGfxLowQuality", 4);
    gd_table(lua, "cameraSystem", "getZoomScale", 6);
    gd_table(lua, "powerupManager", "draw", 9);
    gd_table(lua, "CameraNative", "drawGame", 10);
    damage_lua.create_table(lua, 0, 2);
    gd_function(lua, 1, "drawBackground", 5);
    gd_function(lua, 1, "drawForeground", 14);
    damage_lua.set_field(lua, -10002, "ThemeSystem");
    semihost(4, "AB_ANDROID_GAME_DRAW_V1 16\n");
    for (gd_case = 0; gd_case < 16; ++gd_case) {
        gd_prepare(lua);
        gd_count = 0;
        damage_lua.get_field(lua, -10002, "_ab_draw");
        if (damage_lua.call(lua, 0, 0, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original drawGame failed\n");
        }
        uint32_t header[2] = {gd_case, gd_count};
        print_words(header, 2);
        for (uint32_t index = 0; index < gd_count; ++index)
            print_words(gd_events[index], 8);
        uint32_t output[6];
        gd_value(lua, "oldZoomLevel", output);
        gd_value(lua, "hasExtraZOrderedDraws", output + 2);
        gd_value(lua, "eagleDarkness", output + 4);
        print_words(output, 6);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB drawGame reference complete\n");
}
#endif

#endif
