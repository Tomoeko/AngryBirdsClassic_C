#ifndef AB_ORACLE_LEVEL_ORDER_PROBE_H
#define AB_ORACLE_LEVEL_ORDER_PROBE_H

static size_t read_level_constructor(unsigned index, uint8_t *input, size_t capacity) {
    char path[] = "/data/local/tmp/ab520/levels/000.lua";
    char *digits = path + sizeof(path) - 8;
    digits[0] = (char)('0' + index / 100);
    digits[1] = (char)('0' + (index / 10) % 10);
    digits[2] = (char)('0' + index % 10);
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    if (file < 0) {
        fail("Cannot open controlled level constructor\n");
    }
    size_t length = 0;
    while (length < capacity) {
        int count = system_call(3, file, input + length, capacity - length);
        if (count < 0) {
            fail("Cannot read controlled level constructor\n");
        }
        if (!count) {
            break;
        }
        length += (size_t)count;
    }
    if (length == capacity || system_call(6, file, NULL, 0) != 0) {
        fail("Controlled level constructor exceeds input bounds\n");
    }
    return length;
}

static void probe_level_order(void) {
    typedef void *(*NewState)(void);
    typedef void (*CloseState)(void *);
    typedef int (*Load)(void *, const char *, unsigned, const char *);
    typedef int (*Call)(void *, int, int, int);
    typedef void (*CreateTable)(void *, int, int);
    typedef void (*GetField)(void *, int, const char *);
    typedef const void *(*ToPointer)(void *, int);
    typedef int (*ValueType)(void *, int);
    typedef void (*PushNil)(void *);
    typedef int (*Next)(void *, int);
    typedef void (*SetTop)(void *, int);
    typedef const char *(*ToString)(void *, int, unsigned *);
    NewState new_state = (NewState)find_function("_Z13luaL_newstatev");
    CloseState close_state = (CloseState)find_function("_Z9lua_closeP9lua_State");
    Load load = (Load)find_function("_Z15luaL_loadbufferP9lua_StatePKcjS2_");
    Call call = (Call)find_function("_Z9lua_pcallP9lua_Stateiii");
    CreateTable create_table =
        (CreateTable)find_function("_Z15lua_createtableP9lua_Stateii");
    GetField get_field = (GetField)find_function("_Z12lua_getfieldP9lua_StateiPKc");
    ToPointer to_pointer = (ToPointer)find_function("_Z13lua_topointerP9lua_Statei");
    ValueType value_type = (ValueType)find_function("_Z8lua_typeP9lua_Statei");
    PushNil push_nil = (PushNil)find_function("_Z11lua_pushnilP9lua_State");
    Next next = (Next)find_function("_Z8lua_nextP9lua_Statei");
    SetTop set_top = (SetTop)find_function("_Z10lua_settopP9lua_Statei");
    ToString to_string = (ToString)find_function("_Z13lua_tolstringP9lua_StateiPj");
    static uint8_t input[1024 * 1024];
    static uint8_t keys[2048][128];
    static unsigned lengths[2048];
    void *lua = new_state();
    if (!lua) {
        fail("Cannot allocate native level-order state\n");
    }
    create_table(lua, 0, 0);
    const uint32_t *empty = to_pointer(lua, -1);
    uint32_t dummy_node = empty[4];
    close_state(lua);
    semihost(4, "AB_ANDROID_LEVEL_ORDER_V1 655\n");
    for (unsigned index = 0; index < 655; ++index) {
        size_t size = read_level_constructor(index, input, sizeof(input));
        lua = new_state();
        if (!lua || load(lua, (const char *)input, (unsigned)size, "level") ||
            call(lua, 0, 0, 0)) {
            print_words(&index, 1);
            fail("Native level constructor failed\n");
        }
        get_field(lua, -10002, "world");
        int kind = value_type(lua, -1);
        uint32_t controls[5] = {index, 0, 0, 0, 0};
        if (kind == 0) {
            print_words(controls, 5);
            close_state(lua);
            continue;
        }
        if (kind != 5) {
            fail("Native level world has an unsupported type\n");
        }
        const uint32_t *table = to_pointer(lua, -1);
        unsigned power = table[1] >> 24;
        if ((table[1] & 255) != 5 || power > 20) {
            fail("Native level table layout is unsupported\n");
        }
        controls[1] = 1;
        controls[2] = table[4] == dummy_node ? 0 : UINT32_C(1) << power;
        controls[3] = table[7];
        push_nil(lua);
        while (next(lua, 1)) {
            uint32_t count = controls[4];
            if (count >= 2048 || value_type(lua, -2) != 4) {
                fail("Native level world exceeds string-order bounds\n");
            }
            const char *key = to_string(lua, -2, lengths + count);
            if (lengths[count] > 128) {
                fail("Native level object name exceeds bounds\n");
            }
            for (unsigned byte = 0; byte < lengths[count]; ++byte) {
                keys[count][byte] = (uint8_t)key[byte];
            }
            ++controls[4];
            set_top(lua, 2);
        }
        print_words(controls, 5);
        for (uint32_t entry = 0; entry < controls[4]; ++entry) {
            uint32_t record[33] = {lengths[entry]};
            for (unsigned byte = 0; byte < lengths[entry]; ++byte) {
                record[1 + byte / 4] |= (uint32_t)keys[entry][byte] << (byte % 4) * 8;
            }
            print_words(record, 1 + (lengths[entry] + 3) / 4);
        }
        close_state(lua);
    }
    semihost(4, "AB level order reference complete\n");
}

#endif
