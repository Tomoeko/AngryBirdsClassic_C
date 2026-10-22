#ifndef AB_ORACLE_GUI_QUERIES_H
#define AB_ORACLE_GUI_QUERIES_H

#include "reference_gui_queries.h"

static unsigned gq_roots, gq_children;

static void gq_flag(void *lua, unsigned kind) {
    if (!kind)
        damage_lua.push_nil(lua);
    else if (kind < 3)
        damage_lua.push_boolean(lua, kind == 2);
    else
        damage_lua.push_number(lua, 0);
}

static int gq_root(void *lua) {
    if (damage_lua.get_top(lua) != 1)
        fail("Unexpected level-flow root query arguments\n");
    ++gq_roots;
    damage_lua.get_field(lua, -10002, "_ab_gui_root");
    return 1;
}

static int gq_child(void *lua) {
    if (damage_lua.get_top(lua) != 2 || !damage_named(lua, 2, "pausePage"))
        fail("Unexpected level-flow pause query arguments\n");
    ++gq_children;
    damage_lua.get_field(lua, -10002, "_ab_gui_pause");
    return 1;
}

static void probe_gui_queries(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original GUI-query state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    void (*set_index)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    load_selected_function(lua, "/data/local/tmp/ab520/gui-ingame.lua", "_ab_ingame");
    load_selected_function(lua, "/data/local/tmp/ab520/gui-pausing.lua", "_ab_pausing");
    semihost(4, "AB_ANDROID_GUI_QUERIES_V1 256\n");
    for (unsigned profile = 0; profile < 256; ++profile) {
        ReferenceGuiQuery input = reference_gui_query(profile);
        unsigned root = input.root;
        gq_roots = gq_children = 0;
        if (input.pause) {
            damage_lua.create_table(lua, 0, 1);
            gq_flag(lua, input.pause);
            damage_lua.set_field(lua, 1, "visible");
        } else
            damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "_ab_gui_pause");
        if (root) {
            damage_lua.create_table(lua, 0, 2);
            damage_lua.push_string(lua, root == 1   ? "gameHud"
                                        : root == 2 ? "mainMenu"
                                                    : "other");
            damage_lua.set_field(lua, 1, "name");
            damage_function(lua, 1, "getChild", gq_child);
        } else
            damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "_ab_gui_root");
        damage_lua.create_table(lua, 0, 2);
        damage_lua.create_table(lua, 0, 1);
        damage_function(lua, 2, "getRoot", gq_root);
        damage_lua.set_field(lua, 1, "menuManager");
        damage_lua.create_table(lua, 0, 2);
        damage_lua.get_field(lua, -10002, "_ab_pausing");
        damage_lua.set_field(lua, 2, "isAnyPausingChildVisible");
        damage_lua.create_table(lua, 1, 0);
        damage_lua.create_table(lua, 0, 2);
        gq_flag(lua, input.notification_visible);
        damage_lua.set_field(lua, 4, "visible");
        gq_flag(lua, input.notification_pause);
        damage_lua.set_field(lua, 4, "notificationsFramePause");
        set_index(lua, 3, 1);
        damage_lua.set_field(lua, 2, "children");
        damage_lua.set_field(lua, 1, "notificationsFrame");
        damage_lua.set_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, -10002, "_ab_ingame");
        damage_lua.create_table(lua, 0, 0);
        int status = damage_lua.call(lua, 1, 1, 0);
        unsigned type = (unsigned)damage_lua.type(lua, 1);
        uint32_t row[] = {profile, type, 0, 0, gq_roots, gq_children};
        if (!status && type == 1)
            row[2] = (uint32_t)damage_lua.to_boolean(lua, 1);
        else if (status || type != 0)
            fail("Unexpected original in-game query result\n");
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_pausing");
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, 2, "notificationsFrame");
        /* Keep function/self adjacent without changing the original method. */
        void (*remove)(void *, int) =
            (void (*)(void *, int))find_function("_Z10lua_removeP9lua_Statei");
        remove(lua, 2);
        if (damage_lua.call(lua, 1, 1, 0) || damage_lua.type(lua, 1) != 1)
            fail("Unexpected original notifications pause result\n");
        row[3] = (uint32_t)damage_lua.to_boolean(lua, 1);
        print_words(row, 6);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB GUI queries reference complete\n");
}

#endif
