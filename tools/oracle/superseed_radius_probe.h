#ifndef AB_ORACLE_SUPERSEED_RADIUS_H
#define AB_ORACLE_SUPERSEED_RADIUS_H

#include "selected_function.h"
#include "../../tests/superseed_cases.h"

static void probe_superseed_radius(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original Super Seeds radius state\n");
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/superseed-radius.lua",
                           "_ab_seed_radius");
    void (*push_value)(void *, int) =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    semihost(4, "AB_ANDROID_SUPERSEED_RADIUS_V1 2048\n");
    for (size_t index = 0; index < AB_SUPERSEED_CASES; ++index) {
        AbSuperseedCase input = ab_superseed_case(index);
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_number(lua, input.target);
        damage_lua.set_field(lua, 1, "superseed_target_radius");
        if (input.timer_present) {
            damage_lua.push_number(lua, input.timer);
            damage_lua.set_field(lua, 1, "superseed_timer");
        }
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_number(lua, input.base);
        damage_lua.set_field(lua, 2, "radius");
        damage_lua.get_field(lua, -10002, "_ab_seed_radius");
        push_value(lua, 1);
        push_value(lua, 2);
        if (damage_lua.call(lua, 2, 1, 0))
            fail("Original Super Seeds radius function failed\n");
        FloatBits radius = {.number = damage_lua.to_number(lua, 3)};
        uint32_t words[3] = {radius.bits, 0, 0};
        damage_lua.get_field(lua, 1, "superseed_timer");
        if (damage_lua.type(lua, 4) == 3) {
            words[1] = 1;
            FloatBits timer = {.number = damage_lua.to_number(lua, 4)};
            words[2] = timer.bits;
        } else if (damage_lua.type(lua, 4) != 0) {
            fail("Original Super Seeds returned a foreign timer type\n");
        }
        print_words(words, 3);
        damage_lua.set_top(lua, 0);
    }
    semihost(4, "AB Super Seeds radius reference complete\n");
    void (*close_state)(void *) =
        (void (*)(void *))find_function("_Z9lua_closeP9lua_State");
    close_state(lua);
}

#endif
