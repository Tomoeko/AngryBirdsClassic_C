#ifndef AB_ORACLE_BLOCK_SCRIPT_H
#define AB_ORACLE_BLOCK_SCRIPT_H

#include <stdbool.h>

static const char *const bs_objects[] = {"first", "second", "third"};
static const char *const bs_definitions[] = {"def1", "def2", "def3"};
static const char *const bs_scripts[] = {"_ab_script1", "_ab_script2"};
static uint32_t bs_input[20], bs_events[128][7], bs_event_count, bs_version;
static bool bs_recording;
static const char *(*bs_get_upvalue)(void *, int, int);
static const char *(*bs_set_upvalue)(void *, int, int);
static int (*bs_next)(void *, int);
static unsigned (*bs_length)(void *, int);
static void (*bs_raw_set)(void *, int, int);

static int bs_noop(void *lua) {
    (void)lua;
    return 0;
}

static uint32_t bs_identity(void *lua, int table) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, "id");
    if (damage_lua.type(lua, -1) != 3) {
        fail("Missing original block-script fixture identity\n");
    }
    uint32_t id = (uint32_t)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, top);
    return id;
}

static size_t bs_table_order(void *lua, const char *method, int upvalue, bool keys,
                             uint32_t ids[8]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "BlockScriptManager");
    damage_lua.get_field(lua, top + 1, method);
    if (!bs_get_upvalue(lua, top + 2, upvalue) || damage_lua.type(lua, top + 3) != 5) {
        fail("Cannot read original block-script manager upvalue table\n");
    }
    size_t count = 0;
    damage_lua.push_nil(lua);
    while (bs_next(lua, top + 3)) {
        if (count >= 8) {
            fail("Original block-script fixture table exceeds its bound\n");
        }
        ids[count++] = bs_identity(lua, top + (keys ? 4 : 5));
        damage_lua.set_top(lua, top + 4);
    }
    damage_lua.set_top(lua, top);
    return count;
}

static void bs_catalog(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 3);
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.create_table(lua, 0, 1);
        FloatBits id = {.number = (float)(index + 1 + bs_version * 3)};
        damage_number(lua, top + 4, "id", id.bits);
        damage_lua.set_field(lua, top + 3, bs_definitions[index]);
    }
    damage_lua.set_field(lua, top + 2, "blocks");
    damage_lua.set_field(lua, top + 1, "blockTable");
    damage_lua.set_top(lua, top);
}

static void bs_record(void *lua, uint32_t kind, int object, int definition) {
    if (!bs_recording) {
        return;
    }
    int expected = kind == 1 ? 2 : kind == 3 ? 3 : 1;
    if (damage_lua.get_top(lua) != expected || bs_event_count >= 128) {
        fail("Unexpected original block-script callback arity\n");
    }
    uint32_t *event = bs_events[bs_event_count++], ids[8];
    event[0] = kind;
    event[1] = bs_identity(lua, 1);
    event[2] = object ? bs_identity(lua, object) : 0;
    event[3] = definition ? bs_identity(lua, definition) : 0;
    event[4] = 0;
    if (object) {
        damage_lua.get_field(lua, object, "blockScripts");
        event[4] = bs_length(lua, -1);
        damage_lua.set_top(lua, expected);
    }
    event[5] = (uint32_t)bs_table_order(lua, "clear", 1, true, ids);
    event[6] = (uint32_t)bs_table_order(lua, "applyScriptsToObject", 3, false, ids);
}

static void bs_rebind(void *lua, uint32_t mask) {
    if (bs_input[3] & mask) {
        bs_version ^= 1;
        bs_catalog(lua);
    }
}

static int bs_can_apply(void *lua) {
    uint32_t script = bs_identity(lua, 1), definition = bs_identity(lua, 2);
    bs_record(lua, 1, 0, 2);
    bs_rebind(lua, 4);
    unsigned shift = ((script - 1) * 3 + (definition - 1) % 3) * 2;
    uint32_t kind = (bs_input[0] >> shift) & 3;
    if (!kind) {
        damage_lua.push_nil(lua);
    } else if (kind == 2) {
        damage_lua.push_number(lua, 0);
    } else {
        damage_lua.push_boolean(lua, kind == 3);
    }
    return 1;
}

static int bs_initialize(void *lua) {
    bs_record(lua, 2, 0, 0);
    bs_rebind(lua, 1);
    return 0;
}

static int bs_apply(void *lua) {
    bs_record(lua, 3, 2, 3);
    bs_rebind(lua, 2);
    return 0;
}

static int bs_dispose(void *lua) {
    bs_record(lua, 4, 0, 0);
    return 0;
}

static void bs_method(void *lua, const char *method, const char *argument) {
    damage_lua.get_field(lua, -10002, "BlockScriptManager");
    damage_lua.get_field(lua, 1, method);
    if (argument) {
        damage_lua.get_field(lua, -10002, argument);
    }
    if (damage_lua.call(lua, argument ? 1 : 0, 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original block-script method failed\n");
    }
    damage_lua.set_top(lua, 0);
}

static void bs_prepare(void *lua, size_t item) {
    bs_recording = false;
    bs_method(lua, "clear", NULL);
    damage_lua.get_field(lua, -10002, "BlockScriptManager");
    damage_lua.get_field(lua, 1, "registerScript");
    damage_lua.create_table(lua, 0, 0);
    if (!bs_set_upvalue(lua, 2, 1)) {
        fail("Cannot reset shared original block-script registrations\n");
    }
    damage_lua.set_top(lua, 0);
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.create_table(lua, 0, 5);
        FloatBits id = {.number = (float)index + 1};
        damage_number(lua, 1, "id", id.bits);
        damage_function(lua, 1, "canApply", bs_can_apply);
        damage_function(lua, 1, "dispose", bs_dispose);
        if (bs_input[1] & (1u << index)) {
            damage_function(lua, 1, "init", bs_initialize);
        } else if (item & 1) {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 1, "init");
        }
        if (bs_input[2] & (1u << index)) {
            damage_function(lua, 1, "apply", bs_apply);
        } else if (item & 2) {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 1, "apply");
        }
        damage_lua.set_field(lua, -10002, bs_scripts[index]);
    }
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.create_table(lua, 0, 3);
        FloatBits id = {.number = (float)index + 1};
        damage_number(lua, 1, "id", id.bits);
        damage_lua.push_string(lua, bs_definitions[index]);
        damage_lua.set_field(lua, 1, "definition");
        damage_lua.push_string(lua, bs_objects[index]);
        damage_lua.set_field(lua, 1, "name");
        if (bs_input[6] & (1u << index)) {
            damage_lua.create_table(lua, 0, 0);
            if (bs_input[7] & (1u << index)) {
                damage_lua.get_field(lua, -10002, bs_scripts[0]);
                bs_raw_set(lua, 2, 1);
            }
            damage_lua.set_field(lua, 1, "blockScripts");
        } else if (item & 1) {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 1, "blockScripts");
        }
        damage_lua.set_field(lua, -10002, bs_objects[index]);
    }
    bs_version = bs_input[19];
    bs_catalog(lua);
    bs_event_count = 0;
    bs_recording = true;
}

static void bs_snapshot(void *lua, uint32_t output[11]) {
    uint32_t ids[8];
    output[0] = bs_version;
    output[1] = (uint32_t)bs_table_order(lua, "registerScript", 1, false, ids);
    output[2] = (uint32_t)bs_table_order(lua, "clear", 1, true, ids);
    output[3] = (uint32_t)bs_table_order(lua, "applyScriptsToObject", 3, false, ids);
    output[8] = output[9] = 0;
    for (size_t index = 0; index < output[3]; ++index) {
        output[8] |= 1u << (ids[index] - 1);
        output[9] |= ids[index] << (index * 4);
    }
    output[7] = 0;
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.get_field(lua, -10002, bs_objects[index]);
        damage_lua.get_field(lua, 1, "blockScripts");
        output[4 + index] = damage_lua.type(lua, 2) == 5 ? bs_length(lua, 2) : 0;
        output[7] |= damage_lua.to_boolean(lua, 2) ? 1u << index : 0;
        damage_lua.set_top(lua, 0);
    }
    output[10] = bs_event_count;
}

static void probe_block_script(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original block-script manager state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    bs_get_upvalue = (const char *(*)(void *, int, int))find_function(
        "_Z14lua_getupvalueP9lua_Stateii");
    bs_set_upvalue = (const char *(*)(void *, int, int))find_function(
        "_Z14lua_setupvalueP9lua_Stateii");
    bs_next = (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    bs_length = (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    bs_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    damage_lua.create_table(lua, 0, 4);
    damage_function(lua, 1, "loadLuaFileToObject", bs_noop);
    damage_function(lua, 1, "print", bs_noop);
    damage_lua.push_string(lua, "");
    damage_lua.set_field(lua, 1, "commonScriptPath");
    damage_lua.set_field(lua, -10002, "gamelua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/block-event/manager.lua");
    uint32_t random = UINT32_C(0x520b105c);
    semihost(4, "AB_ANDROID_BLOCK_SCRIPT_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        bs_input[0] = random_word(&random) & 4095;
        bs_input[1] = (uint32_t)((item / 35) & 3);
        bs_input[2] = (uint32_t)((item / 140) & 3);
        bs_input[3] = (uint32_t)((item / 560) & 7);
        bs_input[4] = (uint32_t)(item % 5);
        bs_input[5] = (uint32_t)((item / 5) % 7);
        bs_input[6] = random_word(&random) & 7;
        bs_input[7] = random_word(&random) & 7;
        bs_input[8] = random_word(&random) % 3 + 1;
        for (size_t index = 9; index < 19; ++index) {
            bs_input[index] = random_word(&random) % (index < 13 ? 2u : 3u) + 1;
        }
        bs_input[19] = (uint32_t)((item / 2) & 1);
        bs_prepare(lua, item);
        for (size_t index = 0; index < bs_input[4]; ++index) {
            bs_method(lua, "registerScript", bs_scripts[bs_input[9 + index] - 1]);
        }
        for (size_t index = 0; index < bs_input[5]; ++index) {
            bs_method(lua, "applyScriptsToObject",
                      bs_objects[bs_input[13 + index] - 1]);
        }
        uint32_t before[11], after[11], ids[8], disposal[3] = {0};
        bs_snapshot(lua, before);
        disposal[0] = (uint32_t)bs_table_order(lua, "clear", 1, true, ids);
        for (size_t index = 0; index < disposal[0]; ++index) {
            disposal[index + 1] = ids[index];
        }
        bs_method(lua, "clear", NULL);
        bs_method(lua, "applyScriptsToObject", bs_objects[bs_input[8] - 1]);
        bs_snapshot(lua, after);
        print_words(bs_input, 20);
        print_words(disposal, 3);
        print_words(before, 11);
        print_words(after, 11);
        for (size_t index = 0; index < bs_event_count; ++index) {
            print_words(bs_events[index], 7);
        }
    }
    bs_recording = false;
    bs_method(lua, "clear", NULL);
    damage_lua.close_state(lua);
    semihost(4, "AB block script reference complete\n");
}

#endif
