#ifndef AB_ORACLE_THEME_RECT_H
#define AB_ORACLE_THEME_RECT_H

#include "game_rectangle_probe.h"
#include "../../tests/theme_rect_cases.h"

static uint32_t theme_rect_width, theme_rect_height;

static int theme_rect_get_width(void *context) {
    (void)context;
    return (int)theme_rect_width;
}

static int theme_rect_get_height(void *context) {
    (void)context;
    return (int)theme_rect_height;
}

static void theme_rect_value(void *lua, uint32_t kind, uint32_t bits) {
    if (kind == 0)
        damage_lua.push_nil(lua);
    else if (kind == 1 || kind == 2)
        damage_lua.push_boolean(lua, kind == 2);
    else if (kind == 3) {
        FloatBits value = {.bits = bits};
        damage_lua.push_number(lua, value.number);
    } else if (kind == 4)
        damage_lua.create_table(lua, 0, 0);
    else
        fail("Invalid rectangle theme input kind\n");
}

static void probe_theme_rect(void) {
    initialize_damage_lua();
    typedef void (*Construct)(void *, void *);
    typedef void (*Operation)(void *);
    Construct construct_state =
        (Construct)find_function("_ZN3lua8LuaStateC1EP9lua_State");
    Construct construct_table =
        (Construct)find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    Operation get_reference =
        (Operation)find_function("_ZNK3lua6detail7RefBase6getRefEv");
    Operation claim = (Operation)find_function("_ZN4lang6Object5claimEv");
    Operation destroy_table = (Operation)find_function("_ZN3lua8LuaTableD1Ev");
    Operation destroy_layer = (Operation)find_function("_ZN14RectThemeLayerD1Ev");
    void (*construct_layer)(void *, const void *, void *, void *) =
        (void (*)(void *, const void *, void *, void *))find_function(
            "_ZN14RectThemeLayerC1ERKN3lua8LuaTableERN4game9ResourcesEP7GameLua");
    void (*draw)(void *, void *, void *, unsigned) =
        (void (*)(void *, void *, void *, unsigned))find_function(
            "_ZN14RectThemeLayer4drawEPN2gr7ContextEPN3gfx6CameraEj");
    void *parent = damage_lua.new_state();
    if (!parent)
        fail("Cannot allocate native rectangle theme state\n");
    uint32_t wrapper[8] = {0}, game[18] = {0}, vtable[29] = {0};
    uint32_t context[] = {(uint32_t)(uintptr_t)vtable};
    game[0x44 / 4] = (uint32_t)(uintptr_t)context;
    vtable[16] = (uint32_t)(uintptr_t)game_rectangle_fill;
    vtable[24] = (uint32_t)(uintptr_t)game_rectangle_get_state;
    vtable[27] = (uint32_t)(uintptr_t)theme_rect_get_width;
    vtable[28] = (uint32_t)(uintptr_t)theme_rect_get_height;
    construct_state(wrapper, parent);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    const char *const names[] = {"r",       "g",         "b",        "a",
                                 "relLeft", "relBottom", "relWidth", "relHeight"};
    semihost(4, "AB_ANDROID_THEME_RECT_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[18], table[6] = {0}, layer[16] = {0}, output[14] = {0};
        theme_rect_case(sample, input);
        theme_rect_width = input[0];
        theme_rect_height = input[1];
        construct_table(table, wrapper);
        get_reference(table);
        damage_lua.create_table(lua, 0, 8);
        for (size_t field = 0; field < 8; ++field) {
            theme_rect_value(lua, input[2 + field * 2], input[3 + field * 2]);
            damage_lua.set_field(lua, 2, names[field]);
        }
        damage_lua.set_field(lua, 1, "rect");
        damage_lua.set_top(lua, 0);
        construct_layer(layer, table, NULL, game);
        for (size_t field = 0; field < 4; ++field) {
            output[field] = layer[7 + field];
            output[4 + field] = layer[3 + field];
        }
        game_rectangle_queries = game_rectangle_calls = 0;
        for (size_t field = 0; field < 8; ++field)
            game_rectangle_result[field] = 0;
        draw(layer, context, NULL, sample % 4);
        output[8] = game_rectangle_calls;
        for (size_t field = 0; field < 5; ++field)
            output[9 + field] = game_rectangle_result[field];
        print_words(input, 18);
        print_words(output, 14);
        destroy_layer(layer);
        destroy_table(table);
        if (damage_lua.get_top(lua) != 0)
            fail("Native rectangle theme changed Lua stack\n");
    }
    semihost(4, "AB rectangle theme reference complete\n");
}

#endif
