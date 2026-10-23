#ifndef AB_ORACLE_LEVEL_EGG_PROBE_H
#define AB_ORACLE_LEVEL_EGG_PROBE_H

#include "../../tests/level_egg_cases.h"

static LevelEggCase egg_input;
static uint32_t egg_queries, egg_achievements;

static int egg_query(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 5 ||
        !damage_named(lua, 2, level_egg_names[egg_input.script][2]))
        fail("Unexpected original golden-egg unlock query\n");
    ++egg_queries;
    damage_lua.push_boolean(lua, egg_input.unlocked);
    return 1;
}

static int egg_achieve(void *lua) {
    if (damage_lua.get_top(lua) != 1 ||
        !damage_named(lua, 1, level_egg_names[egg_input.script][2]))
        fail("Unexpected original golden-egg achievement\n");
    ++egg_achievements;
    return 0;
}

static int egg_loader(void *lua) {
    (void)lua;
    return 0;
}

static void egg_source(void *lua) {
    static char bytes[65536];
    const char *path = "/data/local/tmp/ab520/level-scripts.lua";
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    if (file < 0)
        fail("Cannot open original level-script module\n");
    size_t size = 0;
    while (size < sizeof(bytes)) {
        int count = system_call(3, file, bytes + size, sizeof(bytes) - size);
        if (count < 0)
            fail("Cannot read original level-script module\n");
        if (!count)
            break;
        size += (size_t)count;
    }
    if (system_call(6, file, NULL, 0) || size == sizeof(bytes) ||
        damage_lua.load(lua, bytes, (unsigned)size, "original-level-scripts") ||
        damage_lua.call(lua, 0, 0, 0))
        fail("Cannot execute original level-script module\n");
    damage_lua.set_top(lua, 0);
}

static void egg_prepare(void *lua) {
    void (*raw_set)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    damage_lua.push_boolean(lua, !egg_input.full);
    damage_lua.set_field(lua, -10002, "g_is_free_version");
    damage_lua.push_string(lua, "android");
    damage_lua.set_field(lua, -10002, "deviceModel");
    damage_lua.push_string(lua, "");
    damage_lua.set_field(lua, -10002, "scriptPath");
    damage_function(lua, -10002, "loadLuaFile", egg_loader);
    damage_function(lua, -10002, "loadLuaFileToObject", egg_loader);
    damage_function(lua, -10002, "goldenEggAchieved", egg_achieve);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "isGoldenEggUnlocked", egg_query);
    damage_lua.set_field(lua, -10002, "settingsWrapper");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 4, "levels");
    raw_set(lua, 3, 2);
    damage_lua.set_field(lua, 2, "pages");
    raw_set(lua, 1, 7);
    damage_lua.set_field(lua, -10002, "g_episodes");
    egg_source(lua);
    damage_lua.create_table(lua, 0, 2);
    damage_lua.create_table(lua, 0, (int)level_egg_capacity(egg_input.world));
    for (uint32_t index = 0; index < egg_input.world; ++index) {
        char name[12];
        level_egg_key(index, name);
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_number(lua, (float)index);
        damage_lua.set_field(lua, 3, "_ab_id");
        damage_lua.set_field(
            lua, 2,
            index == egg_input.watched ? level_egg_names[egg_input.script][1] : name);
    }
    damage_lua.set_field(lua, 1, "world");
    damage_lua.create_table(lua, 0, (int)level_egg_capacity(egg_input.joints));
    for (uint32_t index = 0; index < egg_input.joints; ++index) {
        char name[12];
        level_egg_key(index, name);
        damage_lua.create_table(lua, 0, 3);
        damage_lua.push_number(lua, (float)index);
        damage_lua.set_field(lua, 3, "_ab_id");
        for (uint32_t endpoint = 0; endpoint < 2; ++endpoint) {
            const char *target = (index + endpoint) % 3 == 0
                                     ? level_egg_names[egg_input.script][1]
                                     : "other";
            damage_lua.push_string(lua, target);
            damage_lua.set_field(lua, 3, endpoint ? "end2" : "end1");
        }
        damage_lua.set_field(lua, 2, name);
    }
    damage_lua.set_field(lua, 1, "joints");
    damage_lua.set_field(lua, -10002, "loadedObjects");
    if (egg_input.eagle == 0)
        damage_lua.push_nil(lua);
    else if (egg_input.eagle == 1)
        damage_lua.push_boolean(lua, 0);
    else if (egg_input.eagle == 2)
        damage_lua.push_number(lua, 0);
    else
        damage_lua.push_string(lua, "");
    damage_lua.set_field(lua, -10002, "eagleBaitLaunched");
}

static uint32_t egg_order(void *lua, const char *name, uint32_t *output) {
    int (*next)(void *, int) =
        (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    damage_lua.get_field(lua, -10002, "loadedObjects");
    damage_lua.get_field(lua, 1, name);
    damage_lua.push_nil(lua);
    uint32_t count = 0;
    while (next(lua, 2)) {
        if (count == AB_LEVEL_EGG_MAXIMUM)
            fail("Original pruned table exceeds probe storage\n");
        damage_lua.get_field(lua, 4, "_ab_id");
        output[count++] = (uint32_t)damage_lua.to_number(lua, 5);
        damage_lua.set_top(lua, 3);
    }
    damage_lua.set_top(lua, 0);
    return count;
}

static void probe_level_egg(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot create original golden-egg state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    semihost(4, "AB_ANDROID_LEVEL_EGG_V1 1152\n");
    for (uint32_t sample = 0; sample < AB_LEVEL_EGG_CASES; ++sample) {
        egg_input = level_egg_case(sample);
        egg_queries = egg_achievements = 0;
        egg_prepare(lua);
        damage_lua.get_field(lua, -10002, "g_level_scripts");
        damage_lua.get_field(lua, 1, level_egg_names[egg_input.script][0]);
        damage_lua.get_field(lua, 2, "onLoadLevel");
        if (damage_lua.type(lua, 3) != 0 && damage_lua.call(lua, 0, 0, 0))
            fail("Original golden-egg level preparation failed\n");
        damage_lua.set_top(lua, 2);
        damage_lua.get_field(lua, 2, "onRemoveBlock");
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_string(
            lua, egg_input.match ? level_egg_names[egg_input.script][1] : "other");
        damage_lua.set_field(lua, 4, "name");
        if (damage_lua.call(lua, 1, 0, 0))
            fail("Original golden-egg removal callback failed\n");
        damage_lua.set_top(lua, 0);
        uint32_t world[AB_LEVEL_EGG_MAXIMUM], joints[AB_LEVEL_EGG_MAXIMUM];
        uint32_t header[] = {egg_queries, egg_achievements,
                             egg_order(lua, "world", world),
                             egg_order(lua, "joints", joints)};
        print_words(header, 4);
        if (header[2])
            print_words(world, header[2]);
        if (header[3])
            print_words(joints, header[3]);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB level-egg reference complete\n");
}

#endif
