#ifndef AB_ORACLE_MUSEUM_PAUSE_H
#define AB_ORACLE_MUSEUM_PAUSE_H

#include "reference_museum_pause.h"

static unsigned mq_roots, mq_children;

static int mq_root(void *lua) {
    ++mq_roots;
    damage_lua.get_field(lua, -10002, "_ab_museum_root");
    return 1;
}

static int mq_child(void *lua) {
    if (!damage_named(lua, 2, "pausePage"))
        fail("Unexpected Museum pause child query\n");
    ++mq_children;
    damage_lua.get_field(lua, -10002, "_ab_museum_pause");
    return 1;
}

static void mq_flag(void *lua, unsigned kind) {
    if (!kind)
        damage_lua.push_nil(lua);
    else if (kind < 3)
        damage_lua.push_boolean(lua, kind == 2);
    else if (kind < 5)
        damage_lua.push_number(lua, (float)(kind - 3));
    else if (kind < 7)
        damage_lua.push_string(lua, kind == 5 ? "true" : "");
    else
        damage_lua.create_table(lua, 0, 0);
}

static void probe_museum_pause(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original Museum pause state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/museum-pause-query.lua",
                           "_ab_pause_query");
    load_selected_function(lua, "/data/local/tmp/ab520/museum-level-query.lua",
                           "_ab_level_query");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "getRoot", mq_root);
    damage_lua.set_field(lua, 1, "menuManager");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_pause_query");
    damage_lua.set_field(lua, 2, "isPauseMenuVisible");
    damage_lua.set_field(lua, 1, "subsystemsapi");
    damage_lua.set_field(lua, -10002, "gamelua");
    semihost(4, "AB_ANDROID_MUSEUM_PAUSE_V1 256\n");
    for (unsigned profile = 0; profile < 256; ++profile) {
        ReferenceMuseumPause input = reference_museum_pause(profile);
        mq_roots = mq_children = 0;
        if (input.child) {
            damage_lua.create_table(lua, 0, 2);
            mq_flag(lua, input.show);
            damage_lua.set_field(lua, 1, "show");
            mq_flag(lua, input.visible);
            damage_lua.set_field(lua, 1, "visible");
        } else
            damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "_ab_museum_pause");
        if (input.root) {
            damage_lua.create_table(lua, 0, 2);
            const char *names[] = {"", "gameHud", "mainMenu", "other"};
            damage_lua.push_string(lua, names[input.root]);
            damage_lua.set_field(lua, 1, "name");
            damage_function(lua, 1, "getChild", mq_child);
        } else
            damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "_ab_museum_root");
        uint32_t row[] = {profile, 0, 0, 0, 0, 0};
        damage_lua.get_field(lua, -10002, "_ab_pause_query");
        if (damage_lua.call(lua, 0, 1, 0) || damage_lua.type(lua, 1) != 1)
            fail("Unexpected original pause query result\n");
        row[1] = (uint32_t)damage_lua.to_boolean(lua, 1);
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_level_query");
        if (damage_lua.call(lua, 0, 1, 0))
            fail("Original Museum level query failed\n");
        row[2] = (uint32_t)damage_lua.type(lua, 1);
        if (row[2] == 1)
            row[3] = (uint32_t)damage_lua.to_boolean(lua, 1);
        else if (row[2] != 0)
            fail("Unexpected original Museum level result\n");
        row[4] = mq_roots;
        row[5] = mq_children;
        print_words(row, 6);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB Museum pause reference complete\n");
}

#endif
