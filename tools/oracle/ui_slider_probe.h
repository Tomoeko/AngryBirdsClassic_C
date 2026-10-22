#ifndef AB_ORACLE_UI_SLIDER_H
#define AB_ORACLE_UI_SLIDER_H

#include "reference_ui_slider.h"

static const char *const slider_states[] = {"CLOSED",  "OPENING",   "OPEN",
                                            "CLOSING", "DISAPPEAR", "DISAPPEARED"};
static const char *const slider_curves[] = {"tweenLinear", "tweenEaseCubicOut",
                                            "tweenEaseCubicInOut", "tweenEaseCubicIn"};
static unsigned slider_refreshes, slider_base_calls;
static float slider_scale;

static void slider_number(void *lua, int table, const char *key, float value) {
    damage_lua.push_number(lua, value);
    damage_lua.set_field(lua, table, key);
}

static void slider_text(void *lua, int table, const char *key, const char *value) {
    damage_lua.push_string(lua, value);
    damage_lua.set_field(lua, table, key);
}

static void slider_method(void *lua, int table, const char *key, const char *method) {
    damage_lua.get_field(lua, -10002, method);
    damage_lua.set_field(lua, table, key);
}

static int slider_refresh(void *lua) {
    (void)lua;
    ++slider_refreshes;
    return 0;
}

static int slider_base(void *lua) {
    (void)lua;
    ++slider_base_calls;
    return 0;
}

static int slider_get_scale(void *lua) {
    damage_lua.push_number(lua, slider_scale);
    return 1;
}

static int slider_visible(void *lua) {
    damage_lua.push_boolean(lua, damage_lua.to_boolean(lua, 2));
    damage_lua.set_field(lua, 1, "visible");
    return 0;
}

static int slider_size(void *lua) {
    slider_number(lua, 1, "width", damage_lua.to_number(lua, 2));
    slider_number(lua, 1, "height", damage_lua.to_number(lua, 3));
    return 0;
}

static uint32_t slider_numeric_field(void *lua, int table, const char *key) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, key);
    if (damage_lua.type(lua, -1) != 3)
        fail("Missing original Slider numeric field\n");
    FloatBits value = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, top);
    return value.bits;
}

static uint32_t slider_boolean_field(void *lua, int table, const char *key) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, key);
    if (damage_lua.type(lua, -1) != 1)
        fail("Missing original Slider Boolean field\n");
    uint32_t result = (uint32_t)damage_lua.to_boolean(lua, -1);
    damage_lua.set_top(lua, top);
    return result;
}

static void slider_build(void *lua, ReferenceUiSlider input, unsigned index) {
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 32);
    slider_text(lua, 1, "state", slider_states[input.state]);
    slider_text(lua, 1, "indicator_style", index % 8 ? "ROTATE_FULL" : "OTHER");
    slider_number(lua, 1, "padding_x", input.padding_x);
    slider_number(lua, 1, "padding_y", input.padding_y);
    slider_number(lua, 1, "gap", input.gap);
    slider_number(lua, 1, "child_size", input.child_size);
    slider_number(lua, 1, "extend_state", input.extend);
    slider_number(lua, 1, "extend_adjusted", input.adjusted);
    slider_number(lua, 1, "disappear_scale", input.disappear);
    slider_number(lua, 1, "scaleX", input.scale_x);
    slider_number(lua, 1, "scaleY", input.scale_y);
    slider_number(lua, 1, "width", 500);
    slider_number(lua, 1, "height", 500);
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 1, "visible");
    slider_method(lua, 1, "_getTotalSize", "_ab_slider_total");
    slider_method(lua, 1, "open_function", slider_curves[input.curve]);
    damage_function(lua, 1, "refreshImages", slider_refresh);
    damage_function(lua, 1, "setVisible", slider_visible);
    damage_function(lua, 1, "setSize", slider_size);
    damage_lua.create_table(lua, 0, 2);
    const float x[] = {0, 0, -1, 1}, y[] = {-1, 1, 0, 0};
    slider_number(lua, 2, "x", x[input.direction]);
    slider_number(lua, 2, "y", y[input.direction]);
    damage_lua.set_field(lua, 1, "direction");
    const char *const controls[] = {"toggle_button", "indicator"};
    for (size_t control = 0; control < 2; ++control) {
        damage_lua.create_table(lua, 0, 3);
        slider_number(lua, 2, "scaleX", 1);
        slider_number(lua, 2, "scaleY", 1);
        slider_number(lua, 2, "angle", 0);
        damage_lua.set_field(lua, 1, controls[control]);
    }
    void (*raw_set)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    damage_lua.create_table(lua, (int)input.count, 0);
    for (unsigned child = 0; child < input.count; ++child) {
        damage_lua.create_table(lua, 0, 6);
        slider_number(lua, 3, "x", 0);
        slider_number(lua, 3, "y", 0);
        slider_number(lua, 3, "sliderOffsetX", input.offsets[child][0]);
        slider_number(lua, 3, "sliderOffsetY", input.offsets[child][1]);
        if (input.override_kinds[child] == 1) {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 3, "sliderOverrideSize");
        } else if (input.override_kinds[child] == 2)
            slider_number(lua, 3, "sliderOverrideSize", input.overrides[child]);
        damage_lua.push_boolean(lua, 1);
        damage_lua.set_field(lua, 3, "visible");
        damage_function(lua, 3, "setVisible", slider_visible);
        raw_set(lua, 2, (int)child + 1);
    }
    damage_lua.set_field(lua, 1, "slider_children");
    damage_lua.set_field(lua, -10002, "_ab_slider");
    damage_lua.get_field(lua, -10002, "gamelua");
    slider_number(lua, 1, "screenHeight", input.screen_height);
    damage_lua.set_top(lua, 0);
    slider_scale = input.scale;
    slider_refreshes = slider_base_calls = 0;
}

static void slider_snapshot(void *lua, unsigned index, uint32_t output[32]) {
    ReferenceUiSlider input = reference_ui_slider(index);
    for (size_t field = 0; field < 32; ++field)
        output[field] = 0;
    output[0] = index;
    damage_lua.get_field(lua, -10002, "_ab_slider");
    damage_lua.get_field(lua, 1, "state");
    output[1] = 6;
    for (unsigned state = 0; state < 6; ++state)
        if (damage_named(lua, 2, slider_states[state]))
            output[1] = state;
    if (output[1] == 6)
        fail("Unexpected original Slider state\n");
    damage_lua.set_top(lua, 1);
    output[2] = slider_numeric_field(lua, 1, "extend_state");
    output[3] = slider_numeric_field(lua, 1, "extend_adjusted");
    output[4] = slider_numeric_field(lua, 1, "disappear_scale");
    damage_lua.get_field(lua, 1, "indicator");
    output[5] = slider_numeric_field(lua, 2, "angle");
    output[8] = slider_numeric_field(lua, 2, "scaleX");
    output[9] = slider_numeric_field(lua, 2, "scaleY");
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "toggle_button");
    output[6] = slider_numeric_field(lua, 2, "scaleX");
    output[7] = slider_numeric_field(lua, 2, "scaleY");
    damage_lua.set_top(lua, 1);
    output[10] = slider_boolean_field(lua, 1, "visible");
    output[11] = slider_numeric_field(lua, 1, "width");
    output[12] = slider_numeric_field(lua, 1, "height");
    output[13] = slider_refreshes;
    output[14] = slider_base_calls;
    output[15] = input.count;
    damage_lua.get_field(lua, 1, "slider_children");
    void (*raw_get)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    for (unsigned child = 0; child < input.count; ++child) {
        raw_get(lua, 2, (int)child + 1);
        output[16 + child * 4] = slider_numeric_field(lua, 3, "x");
        output[17 + child * 4] = slider_numeric_field(lua, 3, "y");
        output[18 + child * 4] = slider_boolean_field(lua, 3, "visible");
        output[19 + child * 4] = input.override_kinds[child];
        damage_lua.set_top(lua, 2);
    }
    damage_lua.set_top(lua, 0);
}

static void probe_ui_slider(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot create original Slider reference state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/slider-update.lua",
                           "_ab_slider_update");
    load_selected_function(lua, "/data/local/tmp/ab520/slider-total.lua",
                           "_ab_slider_total");
    const char *const paths[] = {"/data/local/tmp/ab520/slider-linear.lua",
                                 "/data/local/tmp/ab520/slider-out.lua",
                                 "/data/local/tmp/ab520/slider-inout.lua",
                                 "/data/local/tmp/ab520/slider-in.lua"};
    for (size_t curve = 0; curve < 4; ++curve)
        load_selected_function(lua, paths[curve], slider_curves[curve]);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "update", slider_base);
    damage_lua.set_field(lua, 1, "BGBox");
    damage_lua.set_field(lua, -10002, "ui");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "getScaleNum", slider_get_scale);
    damage_lua.set_field(lua, -10002, "Scaling");
    semihost(4, "AB_ANDROID_UI_SLIDER_V1 512\n");
    for (unsigned index = 0; index < REFERENCE_UI_SLIDER_CASES; ++index) {
        ReferenceUiSlider input = reference_ui_slider(index);
        slider_build(lua, input, index);
        damage_lua.get_field(lua, -10002, "_ab_slider_update");
        damage_lua.get_field(lua, -10002, "_ab_slider");
        damage_lua.push_number(lua, input.seconds);
        damage_lua.push_number(lua, 0);
        if (damage_lua.call(lua, 3, 0, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original Slider update failed\n");
        }
        uint32_t output[32];
        slider_snapshot(lua, index, output);
        print_words(output, 32);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI Slider reference complete\n");
}

#endif
