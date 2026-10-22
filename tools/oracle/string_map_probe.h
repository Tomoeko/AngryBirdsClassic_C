#ifndef AB_ORACLE_STRING_MAP_H
#define AB_ORACLE_STRING_MAP_H

static uint32_t string_map_keys[64][65], string_map_key_objects[64];
static uint32_t string_map_dummy;
static void (*string_map_push_bytes)(void *, const char *, unsigned);
static void (*string_map_raw_set)(void *, int);
static void (*string_map_raw_get)(void *, int);
static int (*string_map_next)(void *, int);
static void (*string_map_push_value)(void *, int);
static int (*string_map_gc)(void *, int, int);
static uint32_t string_map_key_id(uint32_t object);

static void string_map_collect(void *lua) {
    (void)string_map_gc(lua, 2, 0);
    (void)string_map_gc(lua, 0, 0);
}

static uint32_t string_map_erase_next(void *lua, uint32_t removed[64], int collect) {
    uint32_t count = 0, visited = 0;
    damage_lua.push_nil(lua);
    while (string_map_next(lua, 2)) {
        damage_lua.set_top(lua, 3);
        if (visited++ % 2 == 0) {
            if (count >= 64) {
                fail("Native string map erase iteration exceeds bounded keys\n");
            }
            const char *key = damage_lua.to_string(lua, 3, NULL);
            removed[count++] = string_map_key_id((uint32_t)(uintptr_t)key - 16);
            string_map_push_value(lua, 3);
            damage_lua.push_nil(lua);
            string_map_raw_set(lua, 2);
            if (collect) {
                string_map_collect(lua);
            }
        }
    }
    return count;
}

static uint32_t string_map_key_id(uint32_t object) {
    for (uint32_t index = 0; index < 64; ++index) {
        if (string_map_key_objects[index] == object) {
            return index;
        }
    }
    fail("Original string map contains an unpinned key\n");
}

static uint32_t string_map_value(void *lua, int index) {
    int type = damage_lua.type(lua, index);
    if (type == 0 || (type == 1 && !damage_lua.to_boolean(lua, index))) {
        return 0;
    }
    if (type != 3) {
        fail("Unexpected native string map value\n");
    }
    return (uint32_t)damage_lua.to_number(lua, index);
}

static void string_map_push_key(void *lua, uint32_t key) {
    string_map_push_bytes(lua, (const char *)(string_map_keys[key] + 1),
                          string_map_keys[key][0]);
}

static void string_map_snapshot(void *lua) {
    const uint32_t *table = damage_lua.to_pointer(lua, 2);
    uint32_t capacity = table[4] == string_map_dummy
                            ? 0
                            : UINT32_C(1) << (((const unsigned char *)table)[7]);
    if (capacity > 128 || table[7]) {
        fail("Native string map exceeded bounded hash-only layout\n");
    }
    const uint32_t *nodes = (const uint32_t *)(uintptr_t)table[4];
    uint32_t count = 0;
    for (uint32_t slot = 0; slot < capacity; ++slot) {
        count += nodes[slot * 5 + 1] != 0;
    }
    uint32_t last_free = capacity ? (table[5] - table[4]) / 20 : 0;
    if (last_free > capacity) {
        fail("Native string map lastfree cursor exceeds nodes\n");
    }
    uint32_t layout[] = {count, capacity, last_free};
    print_words(layout, 3);
    for (uint32_t slot = 0; slot < capacity; ++slot) {
        const uint32_t *node = nodes + slot * 5;
        uint32_t output[5] = {0, node[1] != 0, 0, 0, node[3]};
        if (node[3]) {
#if defined(AB_ORACLE_STRING_MAP_GC)
            if (node[3] != 4 && node[3] != 11) {
#else
            if (node[3] != 4) {
#endif
                fail("Native string map key was collected or changed type\n");
            }
            output[0] = string_map_key_id(node[2]) + 1;
        }
        if (node[1] == 3) {
            FloatBits value = {.bits = node[0]};
            output[2] = (uint32_t)value.number;
        } else if (node[1] && (node[1] != 1 || node[0])) {
            fail("Native string map node has an unsupported value\n");
        }
        if (node[4]) {
            output[3] = (node[4] - table[4]) / 20 + 1;
            if (output[3] > capacity) {
                fail("Native string map collision link exceeds nodes\n");
            }
        }
#if defined(AB_ORACLE_STRING_MAP_GC)
        print_words(output, 5);
#else
        print_words(output, 4);
#endif
    }
    for (uint32_t key = 0; key < 64; ++key) {
        string_map_push_key(lua, key);
        string_map_raw_get(lua, 2);
        uint32_t lookup[] = {damage_lua.type(lua, 3) != 0, string_map_value(lua, 3)};
        print_words(lookup, 2);
        damage_lua.set_top(lua, 2);
    }
    damage_lua.push_nil(lua);
    uint32_t visited = 0;
    while (string_map_next(lua, 2)) {
        const char *key = damage_lua.to_string(lua, 3, NULL);
        uint32_t output[] = {string_map_key_id((uint32_t)(uintptr_t)key - 16),
                             string_map_value(lua, 4)};
        print_words(output, 2);
        ++visited;
        damage_lua.set_top(lua, 3);
    }
    if (visited != count || damage_lua.get_top(lua) != 2) {
        fail("Native string map next traversal lost a value\n");
    }
}

static void probe_string_map(void) {
    initialize_damage_lua();
    string_map_push_bytes = (void (*)(void *, const char *, unsigned))find_function(
        "_Z15lua_pushlstringP9lua_StatePKcj");
    string_map_raw_set =
        (void (*)(void *, int))find_function("_Z10lua_rawsetP9lua_Statei");
    string_map_raw_get =
        (void (*)(void *, int))find_function("_Z10lua_rawgetP9lua_Statei");
    string_map_next = (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    string_map_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    string_map_gc = (int (*)(void *, int, int))find_function("_Z6lua_gcP9lua_Stateii");
    void (*raw_seti)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate native string map state\n");
    }
#if defined(AB_ORACLE_STRING_MAP_GC)
    (void)string_map_gc(lua, 0, 0);
#endif
    damage_lua.create_table(lua, 64, 0);
    uint32_t random = UINT32_C(0x52057a91);
    const uint32_t lengths[] = {0,  4,  5,  15,  31,  32,  33,
                                63, 64, 65, 127, 128, 255, 256};
#if defined(AB_ORACLE_STRING_MAP_GC)
    semihost(4, "AB_ANDROID_STRING_MAP_GC_V1 16 256 64\n");
#else
    semihost(4, "AB_ANDROID_STRING_MAP_V2 16 256 64\n");
#endif
    for (uint32_t key = 0; key < 64; ++key) {
        uint32_t *input = string_map_keys[key];
        input[0] = lengths[key % 14];
        if (key && !input[0]) {
            input[0] = 4;
        }
        for (size_t word = 1; word < 65; ++word) {
            input[word] = random_word(&random);
        }
        input[1] = key;
        uint32_t count = (input[0] + 3) / 4;
        if (input[0] % 4) {
            input[count] &= UINT32_MAX >> (32 - (input[0] % 4) * 8);
        }
        print_words(input, 1 + count);
        string_map_push_key(lua, key);
        const char *bytes = damage_lua.to_string(lua, 2, NULL);
        string_map_key_objects[key] = (uint32_t)(uintptr_t)bytes - 16;
        raw_seti(lua, 1, (int)key + 1);
    }
    damage_lua.create_table(lua, 0, 0);
    string_map_dummy = ((const uint32_t *)damage_lua.to_pointer(lua, 2))[4];
    damage_lua.set_top(lua, 1);
    for (uint32_t instance = 0; instance < 16; ++instance) {
        uint32_t hint = instance % 17;
        print_words(&hint, 1);
        damage_lua.create_table(lua, 0, (int)hint);
        for (uint32_t step = 0; step < 256; ++step) {
            uint32_t command[] = {random_word(&random) % 6, random_word(&random) % 64,
                                  instance * 256 + step + 1};
#if defined(AB_ORACLE_STRING_MAP_GC)
            if (step >= 128) {
                command[0] = (random >> 16) % 8;
            }
#endif
            if (step < 32) {
                command[0] = step % 3 == 0 ? 2 : 0;
                command[1] = step;
            } else if (step < 48) {
                command[0] = 1;
                command[1] = step - 32;
            } else if (step < 64) {
                command[0] = 1;
                command[1] = step - 16;
            } else if (step < 96) {
                command[0] = 1;
                command[1] = step - 48;
            } else if (step < 128) {
                command[0] = 0;
                command[1] = (step - 48) % 64;
            } else if (command[0] == 4) {
                command[1] %= 21;
            }
#if defined(AB_ORACLE_STRING_MAP_GC)
            if (step == 48) {
                command[0] = 6;
            } else if (step == 112) {
                command[0] = 7;
            }
#endif
            print_words(command, 3);
            uint32_t removed[64], removed_count = 0;
            if (command[0] == 6) {
                string_map_collect(lua);
            } else if (command[0] == 5 || command[0] == 7) {
                removed_count = string_map_erase_next(lua, removed, command[0] == 7);
            } else if (command[0] == 4) {
                damage_lua.set_top(lua, 1);
                damage_lua.create_table(lua, 0, (int)command[1]);
            } else if (command[0] != 3) {
                string_map_push_key(lua, command[1]);
                if (command[0] == 1) {
                    damage_lua.push_nil(lua);
                } else if (command[0] == 2) {
                    damage_lua.push_boolean(lua, 0);
                } else {
                    damage_lua.push_number(lua, (float)command[2]);
                }
                string_map_raw_set(lua, 2);
            }
            print_words(&removed_count, 1);
            if (removed_count) {
                print_words(removed, removed_count);
            }
            string_map_snapshot(lua);
        }
        damage_lua.set_top(lua, 1);
    }
    damage_lua.close_state(lua);
#if defined(AB_ORACLE_STRING_MAP_GC)
    semihost(4, "AB string map GC reference complete\n");
#else
    semihost(4, "AB string map reference complete\n");
#endif
}

#endif
