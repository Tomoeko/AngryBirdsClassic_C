#ifndef AB_ORACLE_Z_ORDER_PROBE_H
#define AB_ORACLE_Z_ORDER_PROBE_H

#include "../../tests/z_order_cases.h"

static void probe_z_order(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot create original z-order state\n");
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    void (*raw_set)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void (*raw_get)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    void (*copy)(void *, int) =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    (void)damage_lua.open_base(lua);
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    char comparator[512];
    int file = system_call(
        5, (int)(uintptr_t)"/data/local/tmp/ab520/z-order-compare.lua", NULL, 0);
    int size = file < 0 ? -1 : system_call(3, file, comparator, sizeof(comparator));
    if (size <= 0 || size >= (int)sizeof(comparator) || system_call(6, file, NULL, 0))
        fail("Cannot read unchanged original z comparator\n");
    semihost(4, "AB_ANDROID_Z_ORDER_V1 2048\n");
    for (uint32_t sample = 0; sample < AB_Z_ORDER_CASES; ++sample) {
        uint32_t keys[AB_Z_ORDER_MAXIMUM], order[AB_Z_ORDER_MAXIMUM];
        uint32_t count = z_order_case(sample, keys);
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "table");
        damage_lua.get_field(lua, 1, "sort");
        damage_lua.create_table(lua, (int)count, 0);
        for (uint32_t index = 0; index < count; ++index) {
            damage_lua.create_table(lua, 0, 2);
            FloatBits value = {.bits = keys[index]};
            damage_lua.push_number(lua, value.number);
            damage_lua.set_field(lua, 4, "z");
            damage_lua.push_number(lua, (float)(index + 1));
            damage_lua.set_field(lua, 4, "id");
            raw_set(lua, 3, (int)index + 1);
        }
        copy(lua, 3);
        damage_lua.set_field(lua, -10002, "_ab_z_order_result");
        if (damage_lua.load(lua, comparator, (unsigned)size, "native-z-compare") ||
            damage_lua.call(lua, 2, 0, 0))
            fail("Original numeric z sort failed\n");
        damage_lua.get_field(lua, -10002, "_ab_z_order_result");
        for (uint32_t index = 0; index < count; ++index) {
            raw_get(lua, 2, (int)index + 1);
            damage_lua.get_field(lua, 3, "id");
            order[index] = (uint32_t)damage_lua.to_number(lua, 4);
            damage_lua.set_top(lua, 2);
        }
        print_words(&count, 1);
        if (count)
            print_words(order, count);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB z-order reference complete\n");
}

#endif
