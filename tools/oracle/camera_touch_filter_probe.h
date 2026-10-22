#ifndef AB_ORACLE_CAMERA_TOUCH_FILTER_H
#define AB_ORACLE_CAMERA_TOUCH_FILTER_H

static int touch_filter_pointer(void *lua) {
    static const char *const names[] = {"LPRESS", "LHOLD", "LRELEASE", "WHEEL"};
    uint32_t event = 4;
    for (uint32_t index = 0; index < 4; ++index) {
        if (damage_named(lua, 2, names[index])) {
            event = index;
        }
    }
    if (event == 4) {
        fail("Unexpected integrated pointer callback\n");
    }
    damage_lua.push_boolean(lua, (system_input[7] & (1u << event)) != 0);
    damage_lua.push_nil(lua);
    return 2;
}

static void touch_filter_initialize(void *lua) {
    static const char *const names[] = {"reset", "pointer",   "clicks",
                                        "copy",  "get-input", "get-touches"};
    static const char *const paths[] = {
        "/data/local/tmp/ab520/input/pointer-reset-oracle.lua",
        "/data/local/tmp/ab520/input/pointer-pointer-oracle.lua",
        "/data/local/tmp/ab520/input/pointer-clicks-oracle.lua",
        "/data/local/tmp/ab520/input/pointer-copy-oracle.lua",
        "/data/local/tmp/ab520/input/pointer-get-input-oracle.lua",
        "/data/local/tmp/ab520/input/pointer-get-touches-oracle.lua"};
    for (size_t index = 0; index < 6; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    system_method(lua, -10002, "deepCopy", "copy");
}

static void touch_filter_reset(void *lua) {
    damage_lua.get_field(lua, -10002, "gamelua");
    system_method(lua, 1, "deepCopy", "copy");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "menuManager");
    system_method(lua, 1, "getFilteredTouches", "get-touches");
    system_method(lua, 1, "getFilteredInput", "get-input");
    system_method(lua, 1, "resetFilteredInput", "reset");
    system_method(lua, 1, "delegatePointerEvent", "pointer");
    damage_lua.create_table(lua, 1, 0);
    damage_lua.push_number(lua, 1);
    system_raw_set(lua, 2, 1);
    damage_lua.set_field(lua, 1, "input_order");
    damage_lua.create_table(lua, 1, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 3, "onPointerEvent", touch_filter_pointer);
    system_raw_set(lua, 2, 1);
    damage_lua.set_field(lua, 1, "menus");
    damage_lua.set_top(lua, 0);
}

static void touch_filter_prepare(void *lua) {
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, -10002, "_ab_touch_filtered");
    damage_lua.set_field(lua, 1, "touches");
    const char *keys[] = {"keyPressed", "keyHold", "keyReleased"};
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_boolean(lua, (int)(system_input[5] & (1u << index)));
        damage_lua.set_field(lua, 2, "LBUTTON");
        damage_lua.set_field(lua, 1, keys[index]);
    }
    damage_lua.create_table(lua, 0, 3);
    damage_lua.push_number(lua, 0);
    damage_lua.set_field(lua, 2, "x");
    damage_lua.push_number(lua, 0);
    damage_lua.set_field(lua, 2, "y");
    damage_lua.push_boolean(lua, (int)(system_input[5] & 8));
    damage_lua.set_field(lua, 2, "wheelTriggered");
    damage_lua.set_field(lua, 1, "cursor");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "menuManager");
    damage_lua.push_boolean(lua, (int)system_input[6]);
    damage_lua.set_field(lua, 1, "allowInput");
    damage_lua.set_top(lua, 0);
    const char *methods[] = {"reset", "clicks"};
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, methods[index]);
        damage_lua.get_field(lua, -10002, "menuManager");
        system_call_checked(lua, 1, 0);
    }
}

#endif
