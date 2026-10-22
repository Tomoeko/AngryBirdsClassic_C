#ifndef AB_ORACLE_OBJECT_REMOVAL_NOTIFY_H
#define AB_ORACLE_OBJECT_REMOVAL_NOTIFY_H

#include "../../tests/object_removal_notify_cases.h"

static uint32_t removal_input[15], removal_events[2][24], removal_count,
    removal_calls[2];
static uint32_t removal_level_binding;
static void (*removal_raw_get)(void *, int);
static void (*removal_raw_set)(void *, int);
static const char *const removal_tables[] = {"_ab_remove_value",   "_ab_remove_empty",
                                             "_ab_remove_catalog", "_ab_remove_blocks",
                                             "_ab_remove_raw",     "_ab_remove_key"};
static const char *const removal_strings[] = {
    "Object", "Missing", "handlerA", "handlerB", "level", "next", "truthy"};
static int removal_first(void *lua);
static int removal_second(void *lua);

static void removal_scalar(void *lua, uint32_t kind) {
    if (kind == 0) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0);
    } else if (kind == 4) {
        damage_lua.push_string(lua, "truthy");
    } else {
        damage_lua.get_field(lua, -10002, removal_tables[0]);
    }
}

static void removal_handler_key(void *lua, uint32_t kind) {
    if (kind == 4 || kind == 6) {
        damage_lua.push_string(lua, kind == 4 ? "handlerA" : "handlerB");
    } else if (kind == 5) {
        damage_lua.get_field(lua, -10002, removal_tables[5]);
    } else {
        removal_scalar(lua, kind);
    }
}

static void removal_level_key(void *lua, uint32_t kind) {
    if (kind == 4 || kind == 6) {
        damage_lua.push_string(lua, kind == 4 ? "level" : "next");
    } else if (kind == 5) {
        damage_lua.get_field(lua, -10002, removal_tables[5]);
    } else {
        removal_scalar(lua, kind);
    }
}

static void removal_binding_push(void *lua, uint32_t kind) {
    if (kind <= 3) {
        removal_scalar(lua, kind);
    } else if (kind == 4) {
        damage_lua.get_field(lua, -10002, removal_tables[0]);
    } else {
        damage_lua.push_closure(lua, kind == 5 ? removal_first : removal_second, 0);
    }
}

static uint32_t removal_binding_kind(void *lua) {
    int type = damage_lua.type(lua, -1);
    if (type == 0) {
        return 0;
    }
    if (type == 1) {
        return damage_lua.to_boolean(lua, -1) ? 2u : 1u;
    }
    if (type == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        if (value.bits) {
            fail("Unexpected original removal binding number\n");
        }
        return 3;
    }
    if (type == 5) {
        return 4;
    }
    if (type == 6) {
        const uint32_t *closure = damage_lua.to_pointer(lua, -1);
        if (closure && ((const uint8_t *)closure)[6] == 1) {
            if (closure[4] == (uintptr_t)removal_first) {
                return 5;
            }
            if (closure[4] == (uintptr_t)removal_second) {
                return 6;
            }
        }
    }
    fail("Unknown original removal notification binding\n");
    return 0;
}

static void removal_value(void *lua, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, -1) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t id = 0; id < 7; ++id) {
            if (damage_named(lua, -1, removal_strings[id])) {
                output[1] = id + 1u;
                return;
            }
        }
        fail("Unknown original removal notification string\n");
    } else if (output[0] == 5) {
        damage_lua.get_field(lua, -1, "_id");
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, -2);
        if (output[1] < 201 || output[1] > 206) {
            fail("Unknown original removal notification table\n");
        }
    } else if (output[0] != 0) {
        fail("Unexpected original removal notification value\n");
    }
}

static void removal_field(void *lua, const char *object, const char *field,
                          uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, object);
    damage_lua.get_field(lua, top + 1, field);
    removal_value(lua, output);
    damage_lua.set_top(lua, top);
}

static void removal_binding_key(void *lua, uint32_t index) {
    static const uint32_t kinds[] = {2, 3, 4, 5, 6};
    removal_handler_key(lua, kinds[index]);
}

static void removal_snapshot(void *lua, uint32_t output[20]) {
    int top = damage_lua.get_top(lua);
    removal_field(lua, "_ab_remove_object", "definition", output);
    removal_field(lua, "_ab_remove_object", "marker", output + 2);
    damage_lua.get_field(lua, -10002, "blockTable");
    removal_value(lua, output + 4);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "levelName");
    removal_value(lua, output + 6);
    damage_lua.set_top(lua, top);
    removal_field(lua, removal_tables[4], "onRemoved", output + 8);
    for (uint32_t index = 0; index < 5; ++index) {
        removal_binding_key(lua, index);
        removal_raw_get(lua, -10002);
        output[10 + index] = removal_binding_kind(lua);
        damage_lua.set_top(lua, top);
    }
    output[15] = removal_level_binding;
    const char *const levels[] = {"_ab_remove_levelA", "_ab_remove_levelB"};
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, levels[index]);
        damage_lua.get_field(lua, top + 1, "onRemoveBlock");
        output[16 + index] = removal_binding_kind(lua);
        damage_lua.set_top(lua, top);
    }
    output[18] = removal_calls[0];
    output[19] = removal_calls[1];
}

/* Same read-only ARM32 frame layout recovered for bird callbacks. This helper
 * has zero upvalues and 40 instructions: verify its independent fingerprint
 * before accepting either plain one-object call at PC18 or PC38. */
static uint32_t removal_caller_pc(void *lua) {
    const uint32_t *state = lua;
    uintptr_t ci = state[5], base = state[10];
    if (!ci || !base || ci < base + 24u || (ci - base) % 24u) {
        fail("Unexpected original removal call-frame bounds\n");
    }
    const uint32_t *caller = (const uint32_t *)(ci - 24u);
    const uint32_t *function = (const uint32_t *)(uintptr_t)caller[1];
    if (!function || function[1] != 6 || !function[0]) {
        fail("Missing original removal caller function\n");
    }
    const uint32_t *closure = (const uint32_t *)(uintptr_t)function[0];
    if (((const uint8_t *)closure)[6] || ((const uint8_t *)closure)[7] || !closure[4]) {
        fail("Unexpected original removal caller closure\n");
    }
    const uint32_t *prototype = (const uint32_t *)(uintptr_t)closure[4];
    if (prototype[11] != 40 || !prototype[3]) {
        fail("Unexpected original removal caller prototype\n");
    }
    const uint32_t *code = (const uint32_t *)(uintptr_t)prototype[3];
    uintptr_t begin = prototype[3], saved = caller[3];
    if (code[0] != UINT32_C(0x00000045) || code[39] != UINT32_C(0x0080001e) ||
        saved <= begin || saved > begin + 160u || (saved - begin) % 4u) {
        fail("Unexpected original removal caller instruction\n");
    }
    uint32_t pc = (uint32_t)((saved - begin) / 4u - 1u);
    if (pc != 18 && pc != 38) {
        fail("Original removal notification came from an unexpected instruction\n");
    }
    return pc;
}

static void removal_level_table(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.create_table(lua, 0, 5);
    static const uint32_t keys[] = {2, 3, 4, 5, 6};
    for (size_t index = 0; index < 5; ++index) {
        removal_level_key(lua, keys[index]);
        if (removal_level_binding == 1 || removal_level_binding == 2) {
            damage_lua.get_field(lua, -10002,
                                 removal_level_binding == 1 ? "_ab_remove_levelA"
                                                            : "_ab_remove_levelB");
        } else if (removal_level_binding == 3) {
            damage_lua.push_boolean(lua, 0);
        } else {
            damage_lua.push_nil(lua);
        }
        removal_raw_set(lua, top + 1);
    }
    damage_lua.set_field(lua, -10002, "g_level_scripts");
}

static void removal_level_function(void *lua, uint32_t level, uint32_t kind) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002,
                         level == 1 ? "_ab_remove_levelA" : "_ab_remove_levelB");
    removal_binding_push(lua, kind);
    damage_lua.set_field(lua, top + 1, "onRemoveBlock");
    damage_lua.set_top(lua, top);
}

static int removal_call(void *lua, uint32_t variant) {
    uint32_t pc = removal_caller_pc(lua), channel = pc == 18 ? 0u : 1u;
    if (damage_lua.get_top(lua) != 1 || removal_count >= 2) {
        fail("Unexpected original removal notification arity/capacity\n");
    }
    damage_lua.get_field(lua, 1, "_id");
    if (damage_lua.to_number(lua, -1) != 501.0f) {
        fail("Original removal notification lost object identity\n");
    }
    damage_lua.set_top(lua, 1);
    uint32_t *event = removal_events[removal_count++];
    event[0] = channel + 1u;
    event[1] = variant;
    event[2] = 501;
    event[3] = 1;
    removal_snapshot(lua, event + 4);
    ++removal_calls[channel];
    uint32_t mutation = removal_input[channel ? 11 : 10];
    if (!channel) {
        if (mutation & 1u) {
            damage_lua.push_string(lua, "next");
            damage_lua.set_field(lua, -10002, "levelName");
        }
        if (mutation & 2u) {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, -10002, "levelName");
        }
        if (mutation & 4u) {
            removal_level_binding = 2;
            removal_level_table(lua);
        }
        if (mutation & 8u) {
            removal_level_function(lua, 2, 6);
        }
        if (mutation & 16u) {
            damage_lua.get_field(lua, -10002, removal_tables[4]);
            damage_lua.push_string(lua, "handlerB");
            damage_lua.set_field(lua, 2, "onRemoved");
            damage_lua.set_top(lua, 1);
        }
        if (mutation & 32u) {
            damage_lua.push_string(lua, "Missing");
            damage_lua.set_field(lua, 1, "definition");
        }
        if (mutation & 64u) {
            damage_lua.push_boolean(lua, 1);
            damage_lua.set_field(lua, 1, "marker");
        }
        if (mutation & 128u) {
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, -10002, "blockTable");
        }
    } else {
        if (mutation & 1u) {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 1, "marker");
        }
        if (mutation & 2u) {
            damage_lua.push_string(lua, "Object");
            damage_lua.set_field(lua, 1, "definition");
        }
        if (mutation & 4u) {
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, -10002, "levelName");
        }
        if (mutation & 8u) {
            damage_lua.get_field(lua, -10002, removal_tables[4]);
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 2, "onRemoved");
            damage_lua.set_top(lua, 1);
        }
        if (mutation & 16u) {
            removal_level_function(lua, removal_level_binding, 0);
        }
        if (mutation & 32u) {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, -10002, "handlerA");
        }
    }
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int removal_first(void *lua) {
    return removal_call(lua, 1);
}

static int removal_second(void *lua) {
    return removal_call(lua, 2);
}

static void removal_prepare(void *lua) {
    for (uint32_t id = 0; id < 6; ++id) {
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_number(lua, (float)id + 201.0f);
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.set_field(lua, -10002, removal_tables[id]);
    }
    damage_lua.get_field(lua, -10002, removal_tables[4]);
    removal_handler_key(lua, removal_input[4]);
    damage_lua.set_field(lua, 1, "onRemoved");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, removal_tables[3]);
    uint32_t raw = removal_input[3];
    if (raw >= 5) {
        damage_lua.get_field(lua, -10002, removal_tables[raw == 6 ? 4u : 1u]);
    } else {
        removal_scalar(lua, raw);
    }
    damage_lua.set_field(lua, 1, "Object");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, removal_tables[2]);
    uint32_t blocks = removal_input[2];
    if (blocks >= 5) {
        damage_lua.get_field(lua, -10002, removal_tables[blocks == 6 ? 3u : 1u]);
    } else {
        removal_scalar(lua, blocks);
    }
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.set_top(lua, 0);
    uint32_t catalog = removal_input[1];
    if (catalog >= 5) {
        damage_lua.get_field(lua, -10002, removal_tables[catalog == 6 ? 2u : 1u]);
    } else {
        removal_scalar(lua, catalog);
    }
    damage_lua.set_field(lua, -10002, "blockTable");
    damage_lua.create_table(lua, 0, 3);
    damage_lua.push_number(lua, 501);
    damage_lua.set_field(lua, 1, "_id");
    uint32_t definition = removal_input[12];
    if (definition == 0 || definition == 4) {
        damage_lua.push_string(lua, definition == 0 ? "Object" : "Missing");
    } else {
        removal_scalar(lua, definition - 1u);
    }
    damage_lua.set_field(lua, 1, "definition");
    removal_scalar(lua, removal_input[13]);
    damage_lua.set_field(lua, 1, "marker");
    damage_lua.set_field(lua, -10002, "_ab_remove_object");
    for (uint32_t index = 0; index < 5; ++index) {
        removal_binding_key(lua, index);
        removal_binding_push(lua, removal_input[index == 4 ? 14 : 5]);
        removal_raw_set(lua, -10002);
    }
    for (uint32_t index = 0; index < 2; ++index) {
        damage_lua.create_table(lua, 0, 1);
        removal_binding_push(lua, removal_input[8 + index]);
        damage_lua.set_field(lua, 1, "onRemoveBlock");
        damage_lua.set_field(lua, -10002,
                             index ? "_ab_remove_levelB" : "_ab_remove_levelA");
    }
    removal_level_binding = removal_input[7];
    removal_level_table(lua);
    removal_level_key(lua, removal_input[6]);
    damage_lua.set_field(lua, -10002, "levelName");
}

static int removal_contains(const char *message, const char *wanted) {
    for (size_t start = 0; message && message[start]; ++start) {
        size_t offset = 0;
        while (wanted[offset] && message[start + offset] &&
               wanted[offset] == message[start + offset]) {
            ++offset;
        }
        if (!wanted[offset]) {
            return 1;
        }
    }
    return 0;
}

static void probe_object_removal_notify(void) {
    initialize_damage_lua();
    removal_raw_get =
        (void (*)(void *, int))find_function("_Z10lua_rawgetP9lua_Statei");
    removal_raw_set =
        (void (*)(void *, int))find_function("_Z10lua_rawsetP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original removal notification state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    char chunk[352];
    int file = system_call(
        5, (int)(uintptr_t)"/data/local/tmp/ab520/collision/removal-notify.lua", NULL,
        0);
    if (file < 0 || system_call(3, file, chunk, sizeof(chunk)) != sizeof(chunk) ||
        system_call(6, file, NULL, 0) != 0 ||
        damage_lua.load(lua, chunk, sizeof(chunk), "original-removal-notify")) {
        fail("Cannot load original removal notification prototype\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_removal_notify");
    semihost(4, "AB_ANDROID_OBJECT_REMOVAL_NOTIFY_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        object_removal_notify_case(sample, removal_input);
        print_words(removal_input, 15);
        removal_count = removal_calls[0] = removal_calls[1] = 0;
        removal_prepare(lua);
        damage_lua.get_field(lua, -10002, "_ab_removal_notify");
        damage_lua.get_field(lua, -10002, "_ab_remove_object");
        uint32_t failed = damage_lua.call(lua, 1, 0, 0) != 0;
        if (failed) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            if (!removal_contains(message, "index") &&
                !removal_contains(message, "call")) {
                fail(message ? message
                             : "Unexpected original removal notification error\n");
            }
        }
        damage_lua.set_top(lua, 0);
        print_words(&failed, 1);
        uint32_t state[20];
        removal_snapshot(lua, state);
        print_words(state, 20);
        print_words(&removal_count, 1);
        for (uint32_t event = 0; event < removal_count; ++event) {
            print_words(removal_events[event], 24);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB object removal notify reference complete\n");
}

#endif
