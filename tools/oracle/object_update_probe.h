#ifndef AB_ORACLE_OBJECT_UPDATE_H
#define AB_ORACLE_OBJECT_UPDATE_H

#include "selected_function.h"

static const char *const ou_functions[] = {"_ab_ou_first", "_ab_ou_second",
                                           "_ab_ou_third"};
static uint32_t ou_calls[4][2], ou_count;
static uint32_t ou_mutation, ou_mutated;
static unsigned (*ou_length)(void *, int);
static void (*ou_raw_get)(void *, int, int);

static int ou_invoke(void *lua, uint32_t id) {
    if (damage_lua.get_top(lua) != 2 || ou_count >= 4 || damage_lua.type(lua, 1) != 5 ||
        damage_lua.type(lua, 2) != 3) {
        fail("Unexpected original object-update invocation\n");
    }
    damage_lua.get_field(lua, 1, "name");
    if (!damage_named(lua, 3, "first")) {
        fail("Unexpected original object-update receiver\n");
    }
    FloatBits seconds = {.number = damage_lua.to_number(lua, 2)};
    ou_calls[ou_count][0] = id;
    ou_calls[ou_count++][1] = seconds.bits;
    uint32_t operation = ou_mutated ? 0 : ou_mutation;
    ou_mutated = 1;
    damage_lua.push_number(lua, (float)operation);
    return 1;
}

static int ou_first(void *lua) {
    return ou_invoke(lua, 1);
}

static int ou_second(void *lua) {
    return ou_invoke(lua, 2);
}

static int ou_third(void *lua) {
    return ou_invoke(lua, 3);
}

static uint32_t ou_identity(void *lua, int index) {
    if (damage_lua.type(lua, index) != 6) {
        fail("Unexpected original object-update function value\n");
    }
    int top = damage_lua.get_top(lua);
    const void *pointer = damage_lua.to_pointer(lua, index);
    for (size_t item = 0; item < 3; ++item) {
        damage_lua.get_field(lua, -10002, ou_functions[item]);
        int same = pointer == damage_lua.to_pointer(lua, -1);
        damage_lua.set_top(lua, top);
        if (same) {
            return (uint32_t)item + 1;
        }
    }
    fail("Unknown original object-update function identity\n");
}

static void ou_value(void *lua, int index, uint32_t output[6]) {
    int kind = damage_lua.type(lua, index);
    for (size_t field = 0; field < 6; ++field) {
        output[field] = 0;
    }
    output[0] = (uint32_t)kind;
    if (kind == 0) {
        return;
    }
    if (kind == 6) {
        output[1] = 1;
        output[2] = ou_identity(lua, index);
        return;
    }
    if (kind != 5 || ou_length(lua, index) > 4) {
        fail("Invalid original dense object-update list\n");
    }
    output[1] = ou_length(lua, index);
    int top = damage_lua.get_top(lua);
    for (uint32_t item = 0; item < output[1]; ++item) {
        ou_raw_get(lua, index, (int)item + 1);
        output[item + 2] = ou_identity(lua, top + 1);
        damage_lua.set_top(lua, top);
    }
}

static int ou_operation(void *lua, uint32_t kind, uint32_t id, uint32_t seconds,
                        uint32_t *query) {
    const char *const methods[] = {"_ab_ou_add", "_ab_ou_remove", "_ab_ou_clear",
                                   "_ab_ou_has"};
    ou_count = 0;
    ou_mutated = 0;
    int arguments;
    if (kind <= 3) {
        damage_lua.get_field(lua, -10002, methods[kind]);
        damage_lua.get_field(lua, -10002, "_ab_ou_object");
        if (kind != 2) {
            damage_lua.get_field(lua, -10002, ou_functions[id - 1]);
        }
        arguments = kind == 2 ? 1 : 2;
    } else {
        damage_lua.get_field(lua, -10002, kind == 4 ? "_ab_ou_object" : "_ab_ou_map");
        damage_lua.get_field(lua, 1, kind == 4 ? "updateFunction" : "first");
        damage_lua.get_field(lua, -10002, "_ab_ou_object");
        FloatBits elapsed = {.bits = seconds};
        damage_lua.push_number(lua, elapsed.number);
        arguments = 2;
    }
    int status = damage_lua.call(lua, arguments, kind == 3 ? 1 : 0, 0);
    *query = 0;
    if (!status && kind == 3) {
        if (damage_lua.type(lua, 1) != 1) {
            fail("Unexpected original object-update membership result\n");
        }
        *query = (uint32_t)damage_lua.to_boolean(lua, 1);
    }
    damage_lua.set_top(lua, 0);
    return status != 0;
}

static void probe_object_update(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original object-update state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    ou_length = (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    ou_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    const char *(*upvalue)(void *, int, int) =
        (const char *(*)(void *, int, int))find_function(
            "_Z14lua_setupvalueP9lua_Stateii");
    const char *names[] = {"_ab_ou_add", "_ab_ou_remove", "_ab_ou_clear",
                           "_ab_ou_has", "contains",      "find_key"};
    const char *paths[] = {"/data/local/tmp/ab520/block-event/object-add.lua",
                           "/data/local/tmp/ab520/block-event/object-remove.lua",
                           "/data/local/tmp/ab520/block-event/object-clear.lua",
                           "/data/local/tmp/ab520/block-event/object-has.lua",
                           "/data/local/tmp/ab520/block-event/object-contains.lua",
                           "/data/local/tmp/ab520/block-event/object-find.lua"};
    for (size_t index = 0; index < 6; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    int (*functions[])(void *) = {ou_first, ou_second, ou_third};
    /* First-party oracle adapter only. Run requested mutations from a Lua frame
     * so native assertion exceptions do not unwind across our freestanding C
     * recorder. Original selected helpers and the callable-list closure remain
     * unchanged; no interpreter or adapter enters the first-party game runtime. */
    static const char adapter[] = "return function(record, name)\n"
                                  "    return function(object, seconds)\n"
                                  "        local operation = record(object, seconds)\n"
                                  "        if operation == 1 then\n"
                                  "            _ab_ou_add(object, _ab_ou_third)\n"
                                  "        elseif operation == 2 then\n"
                                  "            _ab_ou_remove(object, _G[name])\n"
                                  "        elseif operation == 3 then\n"
                                  "            _ab_ou_clear(object)\n"
                                  "        end\n"
                                  "    end\n"
                                  "end\n";
    for (size_t index = 0; index < 3; ++index) {
        if (damage_lua.load(lua, adapter, sizeof(adapter) - 1, "first-party-adapter") ||
            damage_lua.call(lua, 0, 1, 0)) {
            fail("Cannot compile first-party native recorder adapter\n");
        }
        damage_lua.push_closure(lua, functions[index], 0);
        damage_lua.push_string(lua, ou_functions[index]);
        if (damage_lua.call(lua, 2, 1, 0)) {
            fail("Cannot instantiate first-party native recorder adapter\n");
        }
        damage_lua.set_field(lua, -10002, ou_functions[index]);
    }
    uint32_t random = UINT32_C(0x5200b1ec);
    semihost(4, "AB_ANDROID_OBJECT_UPDATE_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[18];
        input[0] = random_float(&random, 112, 30, 1);
        if (item % 31 == 0) {
            input[0] = UINT32_C(0x7fc12345);
        }
        input[1] = (uint32_t)(item % 4);
        for (size_t index = 2; index < 5; ++index) {
            input[index] = random_word(&random) % 3 + 1;
        }
        if (input[1] == 3 && (input[4] == input[2] || input[4] == input[3])) {
            input[4] = 1;
            while (input[4] == input[2] || input[4] == input[3]) {
                ++input[4];
            }
        }
        for (size_t index = 5; index < 17; ++index) {
            input[index] = random_word(&random) % 18;
        }
        input[17] = (uint32_t)((item / 4) & 3);
        ou_mutation = input[17];
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, -10002, "_ab_ou_map");
        for (size_t index = 0; index < 3; ++index) {
            damage_lua.get_field(lua, -10002, names[index]);
            damage_lua.get_field(lua, -10002, "_ab_ou_map");
            if (!upvalue(lua, 1, 1)) {
                fail("Cannot bind original object-update registry\n");
            }
            damage_lua.set_top(lua, 0);
        }
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_string(lua, "first");
        damage_lua.set_field(lua, 1, "name");
        damage_lua.set_field(lua, -10002, "_ab_ou_object");
        for (size_t index = 0; index < input[1]; ++index) {
            uint32_t ignored;
            if (ou_operation(lua, 0, input[index + 2], input[0], &ignored)) {
                fail("Original object-update fixture initialization failed\n");
            }
        }
        print_words(input, 18);
        for (size_t index = 5; index < 17; ++index) {
            uint32_t output[15];
            uint32_t kind = input[index] % 6, id = input[index] / 6 + 1;
            output[0] = (uint32_t)ou_operation(lua, kind, id, input[0], output + 1);
            output[2] = ou_count;
            damage_lua.get_field(lua, -10002, "_ab_ou_object");
            damage_lua.get_field(lua, 1, "updateFunction");
            ou_value(lua, 2, output + 3);
            damage_lua.set_top(lua, 0);
            damage_lua.get_field(lua, -10002, "_ab_ou_map");
            damage_lua.get_field(lua, 1, "first");
            ou_value(lua, 2, output + 9);
            damage_lua.set_top(lua, 0);
            print_words(output, 15);
            for (size_t call = 0; call < ou_count; ++call) {
                print_words(ou_calls[call], 2);
            }
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB object update reference complete\n");
}

#endif
