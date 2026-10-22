#ifndef AB_ORACLE_STRING_ORDER_PROBE_H
#define AB_ORACLE_STRING_ORDER_PROBE_H

static void probe_string_order(void) {
    typedef void *(*NewState)(void);
    typedef void (*CloseState)(void *);
    typedef void (*CreateTable)(void *, int, int);
    typedef void (*PushString)(void *, const char *, unsigned);
    typedef void (*PushNumber)(void *, float);
    typedef void (*StackOperation)(void *, int);
    typedef void (*PushNil)(void *);
    typedef int (*Next)(void *, int);
    typedef float (*ToNumber)(void *, int);
    NewState new_state = (NewState)find_function("_Z13luaL_newstatev");
    CloseState close_state = (CloseState)find_function("_Z9lua_closeP9lua_State");
    CreateTable create_table =
        (CreateTable)find_function("_Z15lua_createtableP9lua_Stateii");
    PushString push_string =
        (PushString)find_function("_Z15lua_pushlstringP9lua_StatePKcj");
    PushNumber push_number =
        (PushNumber)find_function("_Z14lua_pushnumberP9lua_Statef");
    StackOperation set_table =
        (StackOperation)find_function("_Z12lua_settableP9lua_Statei");
    StackOperation set_top =
        (StackOperation)find_function("_Z10lua_settopP9lua_Statei");
    PushNil push_nil = (PushNil)find_function("_Z11lua_pushnilP9lua_State");
    Next next = (Next)find_function("_Z8lua_nextP9lua_Statei");
    ToNumber to_number = (ToNumber)find_function("_Z12lua_tonumberP9lua_Statei");
#if defined(AB_ORACLE_STRING_GROWTH)
    const void *(*to_pointer)(void *, int) =
        (const void *(*)(void *, int))find_function("_Z13lua_topointerP9lua_Statei");
#endif
    void *lua = new_state();
    if (!lua) {
        fail("Native string-order state allocation failed\n");
    }
    uint32_t state = UINT32_C(0x9a03d14c);
    const uint32_t lengths[] = {0,  4,  5,  15,  31,  32,  33,
                                63, 64, 65, 127, 128, 255, 256};
#if defined(AB_ORACLE_STRING_GROWTH)
    semihost(4, "AB_ANDROID_STRING_GROWTH_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_STRING_ORDER_V1 4096\n");
#endif
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t capacity = index % 17 == 0 ? 0 : UINT32_C(1) << (index % 7);
        uint32_t count = random_word(&state) % (capacity + 1);
        if (index % 8 == 0) {
            count = capacity;
        }
        uint32_t controls[] = {capacity, count};
        print_words(controls, 2);
#if defined(AB_ORACLE_STRING_GROWTH)
        create_table(lua, 0, 0);
        const uint32_t *table = to_pointer(lua, 1);
        uint32_t dummy_node = table[4];
#else
        create_table(lua, 0, (int)capacity);
#endif
        for (uint32_t entry = 0; entry < count; ++entry) {
            uint32_t input[65];
            uint32_t length = lengths[(index + entry) % 14];
            if (entry && !length) {
                length = 4;
            }
            input[0] = length;
            for (size_t word = 1; word < 65; ++word) {
                input[word] = random_word(&state);
            }
            input[1] = entry;
            uint32_t words = (length + 3) / 4;
            if (length % 4) {
                input[words] &= UINT32_MAX >> (32 - (length % 4) * 8);
            }
            print_words(input, 1 + words);
            push_string(lua, (const char *)(input + 1), length);
            push_number(lua, (float)entry);
            set_table(lua, -3);
        }
        uint32_t order[64];
#if defined(AB_ORACLE_STRING_GROWTH)
        uint32_t actual_capacity =
            table[4] == dummy_node ? 0 : UINT32_C(1) << (table[1] >> 24);
        print_words(&actual_capacity, 1);
#endif
        uint32_t visited = 0;
        push_nil(lua);
        while (next(lua, 1)) {
            if (visited >= count) {
                fail("Native string-order iteration exceeds input\n");
            }
            order[visited++] = (uint32_t)to_number(lua, -1);
            set_top(lua, 2);
        }
        if (visited != count) {
            fail("Native string-order iteration omitted input\n");
        }
        print_words(order, count);
        set_top(lua, 0);
    }
    close_state(lua);
#if defined(AB_ORACLE_STRING_GROWTH)
    semihost(4, "AB string growth reference complete\n");
#else
    semihost(4, "AB string order reference complete\n");
#endif
}

#endif
