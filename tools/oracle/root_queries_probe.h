#ifndef AB_ORACLE_ROOT_QUERIES_H
#define AB_ORACLE_ROOT_QUERIES_H

#include "reference_root_queries.h"

static void (*rq_set_index)(void *, int, int);

static void rq_flag(void *lua, unsigned kind) {
    if (!kind)
        damage_lua.push_nil(lua);
    else if (kind < 3)
        damage_lua.push_boolean(lua, kind == 2);
    else
        damage_lua.push_number(lua, 0);
}

static int rq_root(void *lua) {
    if (damage_lua.get_top(lua) != 1)
        fail("Unexpected original safe-quit getRoot arguments\n");
    damage_lua.get_field(lua, -10002, "_ab_query_root");
    return 1;
}

static void rq_prepare(void *lua, ReferenceRootQuery input) {
    static const char *names[] = {
        "", "", "mainMenu", "mainMenuRoot", "ForceUpdatePopup", "gameHud"};
    if (input.root < 2)
        damage_lua.push_nil(lua);
    else {
        damage_lua.create_table(lua, 0, 2);
        if (input.root < 6)
            damage_lua.push_string(lua, names[input.root]);
        else if (input.root == 6)
            damage_lua.push_number(lua, 0);
        else
            damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 1, "name");
        damage_lua.create_table(lua, (int)input.children, 0);
        for (unsigned child = 0; child < input.children; ++child) {
            damage_lua.create_table(lua, 0, 3);
            rq_flag(lua, child ? 1u : input.require);
            damage_lua.set_field(lua, 3, "requireInput");
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 3, "visible");
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 3, "active");
            rq_set_index(lua, 2, (int)child + 1);
        }
        damage_lua.set_field(lua, 1, "children");
    }
    damage_lua.set_field(lua, -10002, "_ab_query_root");
    if (!input.root)
        damage_lua.push_nil(lua);
    else {
        damage_lua.create_table(lua, 0, 1);
        damage_function(lua, 1, "getRoot", rq_root);
    }
    damage_lua.set_field(lua, -10002, "menuManager");
    damage_lua.create_table(lua, 3, 0);
    for (unsigned index = 0; index < 3; ++index) {
        damage_lua.create_table(lua, 0, 1);
        rq_flag(lua, index ? 1u : input.unlock);
        damage_lua.set_field(lua, 2, "enabled");
        rq_set_index(lua, 1, (int)index + 1);
    }
    damage_lua.set_field(lua, -10002, "g_freeEagleUnlockStatus");
    damage_lua.create_table(lua, 0, 1);
    if (input.power < 2)
        rq_flag(lua, input.power);
    else {
        damage_lua.create_table(lua, 0, 1);
        rq_flag(lua, input.power == 2 ? 1u : 3u);
        damage_lua.set_field(lua, 2, "free");
    }
    damage_lua.set_field(lua, 1, "mightyPower");
    damage_lua.set_field(lua, -10002, "_ab_query_episode");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.get_field(lua, -10002, "_ab_query_episode");
    damage_lua.set_field(lua, 1, "e");
    damage_lua.get_field(lua, -10002, "_ab_query_episode");
    rq_set_index(lua, 1, 0);
    damage_lua.set_field(lua, -10002, "g_episodes");
    if (input.episode < 2)
        rq_flag(lua, input.episode);
    else if (input.episode == 2)
        damage_lua.push_string(lua, "e");
    else
        damage_lua.push_number(lua, 0);
    damage_lua.set_field(lua, -10002, "currentEpisode");
    damage_lua.push_string(lua, input.main_menu ? "MAIN_MENU" : "INGAME");
    damage_lua.set_field(lua, -10002, "g_eagleClickedFrom");
}

static void probe_root_queries(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original root query state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    rq_set_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    load_selected_function(lua, "/data/local/tmp/ab520/query-safe.lua", "_ab_safe");
    load_selected_function(lua, "/data/local/tmp/ab520/query-eagle.lua", "_ab_eagle");
    semihost(4, "AB_ANDROID_ROOT_QUERIES_V1 512\n");
    for (unsigned profile = 0; profile < 512; ++profile) {
        ReferenceRootQuery input = reference_root_query(profile);
        rq_prepare(lua, input);
        uint32_t values[] = {profile,
                             input.root,
                             input.children,
                             input.require,
                             input.episode,
                             input.power,
                             input.main_menu,
                             input.unlock,
                             0,
                             0};
        const char *functions[] = {"_ab_safe", "_ab_eagle"};
        for (unsigned index = 0; index < 2; ++index) {
            damage_lua.get_field(lua, -10002, functions[index]);
            if (damage_lua.call(lua, 0, 1, 0) || damage_lua.type(lua, 1) != 1)
                fail("Original root query failed or returned non-Boolean\n");
            values[8 + index] = (uint32_t)damage_lua.to_boolean(lua, 1);
            damage_lua.set_top(lua, 0);
        }
        print_words(values, 5);
        print_words(values + 5, 5);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB root queries reference complete\n");
}

#endif
