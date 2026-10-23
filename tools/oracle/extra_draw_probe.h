#ifndef AB_ORACLE_EXTRA_DRAW_PROBE_H
#define AB_ORACLE_EXTRA_DRAW_PROBE_H

#include "selected_function.h"
#include "game_draw_probe.h"
#include "../../tests/extra_draw_cases.h"

static uint32_t extra_events[128], extra_count, extra_remove;

static void extra_event(uint32_t id) {
    if (extra_count == 128)
        fail("Original extra-draw event overflow\n");
    extra_events[extra_count++] = id;
}

static int extra_callback(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 3)
        fail("Unexpected original extra-draw parameter\n");
    uint32_t id = (uint32_t)damage_lua.to_number(lua, 1);
    damage_lua.get_field(lua, -10002, "hasExtraZOrderedDraws");
    extra_event(id | (damage_lua.to_boolean(lua, 2) ? 256u : 0u));
    damage_lua.set_top(lua, 1);
    if (id == extra_remove) {
        char name[9];
        extra_draw_name(id, name);
        damage_lua.get_field(lua, -10002, "g_extra_draw_functions");
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 2, name);
        damage_lua.set_top(lua, 1);
    }
    return 0;
}

static int extra_z_draw(void *lua) {
    if (damage_lua.get_top(lua) != 1)
        fail("Unexpected original z-draw arguments\n");
    const char *(*get)(void *, int, int) =
        (const char *(*)(void *, int, int))find_function(
            "_Z14lua_getupvalueP9lua_Stateii");
    const char *(*set)(void *, int, int) =
        (const char *(*)(void *, int, int))find_function(
            "_Z14lua_setupvalueP9lua_Stateii");
    void (*copy)(void *, int) =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    damage_lua.get_field(lua, -10002, "_ab_draw");
    if (!get(lua, 2, 1) || damage_lua.type(lua, 3) != 5)
        fail("Original drawGame has no prepared z array\n");
    damage_lua.get_field(lua, -10002, "_ab_z_draw");
    copy(lua, 3);
    if (!set(lua, 4, 1))
        fail("Cannot share original prepared z array\n");
    copy(lua, 1);
    if (damage_lua.call(lua, 1, 0, 0))
        fail("Original drawZOrdered failed\n");
    damage_lua.set_top(lua, 1);
    return 0;
}

static int extra_camera(void *lua) {
    damage_lua.get_field(lua, -10002, "hasExtraZOrderedDraws");
    bool active = damage_lua.to_boolean(lua, -1) != 0;
    damage_lua.set_top(lua, 0);
    const float layers[] = {-2, 0, 1, 3, 7};
    for (uint32_t layer = 0; layer < 5; ++layer) {
        if (active) {
            damage_lua.get_field(lua, -10002, "drawZOrdered");
            damage_lua.push_number(lua, layers[layer]);
            if (damage_lua.call(lua, 1, 0, 0))
                fail("Original per-layer z draw failed\n");
        }
        extra_event(100 + layer);
    }
    return 0;
}

static void extra_prepare(void *lua, ExtraDrawCase input) {
    gd_case = 0;
    gd_prepare(lua);
    const char *(*set)(void *, int, int) =
        (const char *(*)(void *, int, int))find_function(
            "_Z14lua_setupvalueP9lua_Stateii");
    damage_lua.get_field(lua, -10002, "_ab_draw");
    damage_lua.get_field(lua, -10002, "_ab_z_compare");
    if (!set(lua, 1, 2))
        fail("Cannot bind original z comparator\n");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "g_extra_draw_functions");
    for (uint32_t index = 0; index < input.count; ++index) {
        char name[9];
        extra_draw_name(index + 1, name);
        damage_lua.create_table(lua, 0, 4);
        damage_function(lua, 2, "func", extra_callback);
        damage_lua.push_number(lua, (float)(index + 1));
        damage_lua.set_field(lua, 2, "param");
        damage_lua.push_boolean(lua, input.before[index]);
        damage_lua.set_field(lua, 2, "beforeBlocks");
        if (input.z_present[index]) {
            damage_lua.push_number(lua, input.z[index]);
            damage_lua.set_field(lua, 2, "z");
        }
        damage_lua.set_field(lua, 1, name);
    }
    damage_lua.set_top(lua, 0);
    extra_remove = input.remove;
}

static void probe_extra_draw(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot create original extra-draw state\n");
    (void)damage_lua.open_base(lua);
    (void)damage_lua.open_math(lua);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/draw-game/original.lua",
                           "_ab_draw");
    load_selected_function(lua, "/data/local/tmp/ab520/extra-z-draw.lua", "_ab_z_draw");
    load_selected_function(lua, "/data/local/tmp/ab520/z-order-compare.lua",
                           "_ab_z_compare");
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
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "drawGame", extra_camera);
    damage_lua.set_field(lua, -10002, "CameraNative");
    damage_function(lua, -10002, "drawZOrdered", extra_z_draw);
    damage_lua.create_table(lua, 0, 2);
    gd_function(lua, 1, "drawBackground", 5);
    gd_function(lua, 1, "drawForeground", 14);
    damage_lua.set_field(lua, -10002, "ThemeSystem");
    semihost(4, "AB_ANDROID_EXTRA_DRAW_V2 256 2\n");
    for (uint32_t sample = 0; sample < AB_EXTRA_DRAW_CASES; ++sample) {
        extra_prepare(lua, extra_draw_case(sample));
        for (uint32_t frame = 0; frame < 2; ++frame) {
            extra_count = gd_count = 0;
            damage_lua.get_field(lua, -10002, "_ab_draw");
            if (damage_lua.call(lua, 0, 0, 0))
                fail("Original registered extra-draw frame failed\n");
            damage_lua.get_field(lua, -10002, "hasExtraZOrderedDraws");
            uint32_t header[] = {extra_count, (uint32_t)damage_lua.to_boolean(lua, -1)};
            print_words(header, 2);
            print_words(extra_events, extra_count);
            damage_lua.set_top(lua, 0);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB extra-draw reference complete\n");
}

#endif
