#ifndef AB_ORACLE_INTEGER_MAP_H
#define AB_ORACLE_INTEGER_MAP_H

static void integer_map_snapshot(void *lua, int index) {
    const uint32_t *table = damage_lua.to_pointer(lua, index);
    uint32_t controls[3] = {table[7], 0, 0};
    if (table[4] != integer_dummy_node) {
        unsigned exponent = ((const unsigned char *)table)[7];
        if (exponent > 8)
            fail("Unexpected integer mutation hash capacity\n");
        controls[1] = 1u << exponent;
    }
    uint32_t entries[256];
    int top = damage_lua.get_top(lua);
    damage_lua.push_nil(lua);
    while (integer_next(lua, index)) {
        if (controls[2] >= 128)
            fail("Integer mutation snapshot overflow\n");
        entries[controls[2] * 2] = (uint32_t)(int32_t)damage_lua.to_number(lua, -2);
        entries[controls[2] * 2 + 1] = (uint32_t)damage_lua.to_number(lua, -1);
        ++controls[2];
        damage_lua.set_top(lua, top + 1);
    }
    print_words(controls, 3);
    if (controls[2])
        print_words(entries, controls[2] * 2);
}

static void probe_integer_map(void) {
    initialize_damage_lua();
    integer_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    integer_raw_set =
        (void (*)(void *, int))find_function("_Z10lua_rawsetP9lua_Statei");
    integer_raw_seti =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    integer_next = (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original integer mutation state\n");
    damage_lua.create_table(lua, 0, 0);
    integer_dummy_node = ((const uint32_t *)damage_lua.to_pointer(lua, 1))[4];
    damage_lua.set_top(lua, 0);
    uint32_t random = UINT32_C(0x520d31a9);
    semihost(4, "AB_ANDROID_INTEGER_MAP_V1 64 128\n");
    for (size_t item = 0; item < 64; ++item) {
        damage_lua.create_table(lua, 0, 0);
        for (uint32_t step = 0; step < 128; ++step) {
            int32_t key = (int32_t)((random_word(&random) >> 16) % 97) - 32;
            if (item % 4 == 0)
                key = (int32_t)step + 1;
            else if (item % 4 == 1)
                key = (int32_t)(step % 24) + 1;
            else if (item % 4 == 2)
                key = 16777216 - (int32_t)(step % 17);
            uint32_t present = (random_word(&random) >> 16) % 3 != 0;
            uint32_t operation[3] = {(uint32_t)key, present, step + 1};
            print_words(operation, 3);
            if (present)
                damage_lua.push_number(lua, (float)(step + 1));
            else
                damage_lua.push_nil(lua);
            integer_raw_seti(lua, 1, key);
            integer_map_snapshot(lua, 1);
            damage_lua.create_table(lua, 0, 0);
            damage_lua.push_nil(lua);
            while (integer_next(lua, 1)) {
                integer_push_value(lua, -2);
                integer_push_value(lua, -2);
                integer_raw_set(lua, 2);
                damage_lua.set_top(lua, 3);
            }
            integer_map_snapshot(lua, 2);
            damage_lua.set_top(lua, 1);
        }
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB integer map reference complete\n");
}

#endif
