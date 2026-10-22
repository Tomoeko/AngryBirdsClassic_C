#ifndef AB_ORACLE_RELOAD_GEOMETRY_H
#define AB_ORACLE_RELOAD_GEOMETRY_H

#include "reference_reload_geometry.h"

static int rg_snap(void *lua) {
    if (damage_named(lua, 2, "bird"))
        damage_lua.get_field(lua, 1, "bird");
    else if (damage_named(lua, 2, "castle"))
        damage_lua.get_field(lua, 1, "castle");
    else
        fail("Unexpected original reload snap name\n");
    return 1;
}

static void rg_position(void *lua, int owner, const char *name, float x, float y) {
    damage_lua.create_table(lua, 0, 2);
    FloatBits first = {.number = x}, second = {.number = y};
    damage_number(lua, owner + 1, "x", first.bits);
    damage_number(lua, owner + 1, "y", second.bits);
    damage_lua.set_field(lua, owner, name);
}

static void probe_reload_geometry(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original reload geometry state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/reload-distance.lua",
                           "vectorDistance");
    load_selected_function(lua, "/data/local/tmp/ab520/reload-subtract.lua",
                           "vectorSubtract");
    load_selected_function(lua, "/data/local/tmp/ab520/reload-length.lua",
                           "vectorLength");
    load_selected_function(lua, "/data/local/tmp/ab520/reload-vlength.lua", "vLength");
    load_selected_function(lua, "/data/local/tmp/ab520/reload-between.lua",
                           "_ab_between");
    load_selected_function(lua, "/data/local/tmp/ab520/reload-to.lua", "_ab_to");
    load_selected_function(lua, "/data/local/tmp/ab520/reload-position.lua",
                           "_ab_position");
    semihost(4, "AB_ANDROID_RELOAD_GEOMETRY_V1 128\n");
    for (unsigned profile = 0; profile < 128; ++profile) {
        float coordinates[4];
        reference_reload_geometry(profile, coordinates);
        damage_lua.create_table(lua, 0, 7);
        damage_function(lua, 1, "getSnapPoint", rg_snap);
        damage_lua.get_field(lua, -10002, "_ab_position");
        damage_lua.set_field(lua, 1, "getScreenPosition");
        rg_position(lua, 1, "screen", 0, 0);
        rg_position(lua, 1, "bird", coordinates[0], coordinates[1]);
        rg_position(lua, 1, "castle", coordinates[2], coordinates[3]);
        damage_lua.set_field(lua, -10002, "_ab_reload_camera");
        uint32_t row[] = {profile, 0, 0, 0, 0, 0, 0};
        for (unsigned index = 0; index < 4; ++index) {
            FloatBits value = {.number = coordinates[index]};
            row[index + 1] = value.bits;
        }
        damage_lua.get_field(lua, -10002, "_ab_between");
        damage_lua.get_field(lua, -10002, "_ab_reload_camera");
        damage_lua.push_string(lua, "castle");
        damage_lua.push_string(lua, "bird");
        if (damage_lua.call(lua, 3, 1, 0) || damage_lua.type(lua, 1) != 3)
            fail("Original reload distance-between failed\n");
        FloatBits value = {.number = damage_lua.to_number(lua, 1)};
        row[5] = value.bits;
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_to");
        damage_lua.get_field(lua, -10002, "_ab_reload_camera");
        damage_lua.push_string(lua, "bird");
        if (damage_lua.call(lua, 2, 1, 0) || damage_lua.type(lua, 1) != 3)
            fail("Original reload distance-to failed\n");
        value.number = damage_lua.to_number(lua, 1);
        row[6] = value.bits;
        print_words(row, 7);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB reload geometry reference complete\n");
}

#endif
