#ifndef AB_ORACLE_INTEGER_ORDER_H
#define AB_ORACLE_INTEGER_ORDER_H

static void (*integer_push_value)(void *, int);
static void (*integer_raw_set)(void *, int);
static void (*integer_raw_seti)(void *, int, int);
static int (*integer_next)(void *, int);
static uint32_t integer_dummy_node;

static void integer_snapshot(void *lua, int index, uint32_t count) {
    const uint32_t *table = damage_lua.to_pointer(lua, index);
    uint32_t controls[3] = {table[7], 0, 0};
    if (table[4] != integer_dummy_node) {
        unsigned exponent = ((const unsigned char *)table)[7];
        if (exponent > 7) {
            fail("Unexpected integer table hash capacity\n");
        }
        controls[1] = 1u << exponent;
    }
    uint32_t order[64];
    int top = damage_lua.get_top(lua);
    damage_lua.push_nil(lua);
    while (integer_next(lua, index)) {
        if (controls[2] >= count) {
            fail("Original integer table member overflow\n");
        }
        float label = damage_lua.to_number(lua, -1);
        if (!(label >= 1 && label <= (float)count)) {
            fail("Unexpected original integer entry label\n");
        }
        order[controls[2]++] = (uint32_t)label - 1;
        damage_lua.set_top(lua, top + 1);
    }
    if (controls[2] != count) {
        fail("Original integer table lost a key\n");
    }
    print_words(controls, 3);
    if (count) {
        print_words(order, count);
    }
    damage_lua.set_top(lua, top);
}

static void integer_keys(int32_t keys[64], uint32_t count, size_t item,
                         uint32_t *random) {
    for (uint32_t index = 0; index < count; ++index) {
        switch (item % 8) {
            case 0:
            case 1:
                keys[index] = (int32_t)index + 1;
                break;
            case 2:
                keys[index] = -(int32_t)index - 1;
                break;
            case 3:
                keys[index] = (int32_t)index * 17 - 34;
                break;
            case 4:
                keys[index] = (int32_t)index;
                break;
            case 5:
                keys[index] = 16777216 - (int32_t)index;
                break;
            case 6:
                keys[index] = index % 2 ? -(int32_t)index - 1 : (int32_t)index + 1;
                break;
            default: {
                int duplicate;
                do {
                    duplicate = 0;
                    keys[index] = (int32_t)((random_word(random) >> 8) % 257) - 128;
                    for (uint32_t earlier = 0; earlier < index; ++earlier) {
                        duplicate |= keys[index] == keys[earlier];
                    }
                } while (duplicate);
                break;
            }
        }
    }
    if (item % 8) {
        for (uint32_t end = count; end > 1; --end) {
            uint32_t other = (random_word(random) >> 16) % end;
            int32_t value = keys[end - 1];
            keys[end - 1] = keys[other];
            keys[other] = value;
        }
    }
}

static void probe_integer_order(void) {
    initialize_damage_lua();
    integer_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    integer_raw_set =
        (void (*)(void *, int))find_function("_Z10lua_rawsetP9lua_Statei");
    integer_raw_seti =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    integer_next = (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original integer-order state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/input/pointer-copy-oracle.lua",
                           "deepCopy");
    damage_lua.create_table(lua, 0, 0);
    const uint32_t *empty = damage_lua.to_pointer(lua, 1);
    integer_dummy_node = empty[4];
    damage_lua.set_top(lua, 0);
    uint32_t random = UINT32_C(0x5201a311);
    semihost(4, "AB_ANDROID_INTEGER_ORDER_V1 4096 3\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t count = (uint32_t)(item % 65);
        uint32_t setup[2] = {count, (uint32_t)(item % 2)};
        int32_t keys[64];
        integer_keys(keys, count, item, &random);
        print_words(setup, 2);
        if (count) {
            uint32_t input[64];
            for (uint32_t index = 0; index < count; ++index) {
                input[index] = (uint32_t)keys[index];
            }
            print_words(input, count);
        }
        damage_lua.create_table(lua, 0, 0);
        for (uint32_t index = 0; index < count; ++index) {
            if (!setup[1]) {
                damage_lua.push_number(lua, (float)(index + 1));
                integer_raw_seti(lua, 1, keys[index]);
            } else {
                damage_lua.push_number(lua, (float)keys[index]);
                damage_lua.push_number(lua, (float)(index + 1));
                integer_raw_set(lua, 1);
            }
        }
        for (int pass = 0; pass < 3; ++pass) {
            if (pass) {
                damage_lua.get_field(lua, -10002, "deepCopy");
                integer_push_value(lua, pass);
                if (damage_lua.call(lua, 1, 1, 0)) {
                    const char *message = damage_lua.to_string(lua, -1, NULL);
                    fail(message ? message : "Original integer copy failed\n");
                }
            }
            integer_snapshot(lua, pass + 1, count);
        }
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB integer order reference complete\n");
}

#endif
