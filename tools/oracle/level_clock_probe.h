#ifndef AB_ORACLE_LEVEL_CLOCK_PROBE_H
#define AB_ORACLE_LEVEL_CLOCK_PROBE_H

#include "selected_function.h"
#include "../../tests/level_clock_cases.h"

static LevelClockCase clock_input;
static uint32_t clock_queries;

static int clock_now(void *lua) {
    if (damage_lua.get_top(lua))
        fail("Unexpected original current-time parameters\n");
    ++clock_queries;
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_number(lua, clock_input.minutes);
    damage_lua.set_field(lua, 1, "minutes");
    damage_lua.push_number(lua, clock_input.seconds);
    damage_lua.set_field(lua, 1, "seconds");
    return 1;
}

static void clock_prepare(void *lua, uint32_t sample) {
    void (*raw_set)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void (*copy)(void *, int) =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    damage_lua.create_table(lua, 0, 4);
    damage_lua.create_table(lua, 4, 0);
    for (uint32_t index = 0; index < 4; ++index) {
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_number(lua, clock_input.positions[index * 2]);
        damage_lua.set_field(lua, 3, "x");
        damage_lua.push_number(lua, clock_input.positions[index * 2 + 1]);
        damage_lua.set_field(lua, 3, "y");
        if (clock_input.absent != index)
            copy(lua, 3);
        else if (sample & 1)
            damage_lua.push_boolean(lua, 0);
        else
            damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 1, level_clock_names[index]);
        raw_set(lua, 2, (int)index + 1);
    }
    damage_lua.set_field(lua, -10002, "_ab_clock_items");
    damage_lua.create_table(lua, 0, 1);
    copy(lua, 1);
    damage_lua.set_field(lua, 2, "world");
    damage_lua.set_field(lua, -10002, "loadedObjects");
    damage_lua.set_top(lua, 0);
}

static void probe_level_clock(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot create original clock-level state\n");
    (void)damage_lua.open_base(lua);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "getCurrentTime", clock_now);
    load_selected_function(lua, "/data/local/tmp/ab520/level-clock.lua", "_ab_clock");
    void (*raw_get)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    semihost(4, "AB_ANDROID_LEVEL_CLOCK_V1 4096\n");
    for (uint32_t sample = 0; sample < AB_LEVEL_CLOCK_CASES; ++sample) {
        clock_input = level_clock_case(sample);
        clock_queries = 0;
        clock_prepare(lua, sample);
        damage_lua.get_field(lua, -10002, "_ab_clock");
        if (damage_lua.call(lua, 0, 0, 0))
            fail("Original clock-level preparation failed\n");
        uint32_t output[9] = {clock_queries};
        damage_lua.get_field(lua, -10002, "_ab_clock_items");
        for (uint32_t index = 0; index < 4; ++index) {
            raw_get(lua, 1, (int)index + 1);
            damage_lua.get_field(lua, 2, "x");
            FloatBits x = {.number = damage_lua.to_number(lua, 3)};
            damage_lua.get_field(lua, 2, "y");
            FloatBits y = {.number = damage_lua.to_number(lua, 4)};
            output[index * 2 + 1] = x.bits;
            output[index * 2 + 2] = y.bits;
            damage_lua.set_top(lua, 1);
        }
        damage_lua.set_top(lua, 0);
        print_words(output, 9);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB level-clock reference complete\n");
}

#endif
