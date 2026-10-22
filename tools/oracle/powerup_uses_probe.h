#ifndef AB_ORACLE_POWERUP_USES_H
#define AB_ORACLE_POWERUP_USES_H

#include "../../tests/powerup_uses_cases.h"

static int pu_call(void *lua, const char *method, const char *name) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "PowerupManager");
    damage_lua.get_field(lua, top + 1, method);
    damage_lua.get_field(lua, -10002, "_ab_pu_self");
    if (name)
        damage_lua.push_string(lua, name);
    if (damage_lua.call(lua, name ? 2 : 1, 1, 0))
        fail("Original powerup use method failed\n");
    return top;
}

static void pu_clear(void *lua) {
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_pu_self");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "m_powerupsUsed");
    damage_lua.set_top(lua, 0);
}

static void probe_powerup_uses(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original powerup use state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    int (*open_string)(void *) =
        (int (*)(void *))find_function("_Z14luaopen_stringP9lua_State");
    (void)open_string(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "requireFile", damage_require);
    damage_lua.get_field(lua, -10002, "_G");
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/event-manager/class.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/event-manager/utils.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/powerup-uses/manager.lua");
    semihost(4, "AB_ANDROID_POWERUP_USES_V1 8 16\n");
    for (uint32_t sample = 0; sample < POWERUP_USE_CASES; ++sample) {
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, -10002, "_ab_pu_self");
        pu_clear(lua);
        for (uint32_t initial = 0; initial < 3; ++initial)
            if (sample & (1u << initial)) {
                int top = pu_call(lua, "markPowerupAsUsed",
                                  powerup_use_initial_names[initial]);
                damage_lua.set_top(lua, top);
            }
        print_words(&sample, 1);
        for (uint32_t step = 0; step < POWERUP_USE_ACTIONS; ++step) {
            uint32_t output[5] = {0};
            PowerupUseAction action = powerup_use_actions[step];
            if (action.operation == POWERUP_USE_CLEAR) {
                pu_clear(lua);
            } else {
                int top = pu_call(lua,
                                  action.operation == POWERUP_USE_MARK
                                      ? "markPowerupAsUsed"
                                      : "hasPowerupBeenUsedInLevel",
                                  action.name);
                if (action.operation == POWERUP_USE_QUERY)
                    output[0] = damage_lua.to_boolean(lua, -1) != 0;
                damage_lua.set_top(lua, top);
            }
            int top = pu_call(lua, "getPowerupsUsed", NULL);
            output[1] = (uint32_t)damage_lua.to_number(lua, -1);
            damage_lua.set_top(lua, top);
            for (uint32_t query = 0; query < 3; ++query) {
                top = pu_call(lua, "hasPowerupBeenUsedInLevel",
                              powerup_use_queries[query]);
                output[query + 2] = damage_lua.to_boolean(lua, -1) != 0;
                damage_lua.set_top(lua, top);
            }
            print_words(output, 5);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB powerup uses reference complete\n");
}

#endif
