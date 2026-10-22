#ifndef AB_ORACLE_OBJECT_REMOVED_H
#define AB_ORACLE_OBJECT_REMOVED_H

#include "../../tests/object_removed_cases.h"

static uint32_t removed_native_input[OBJECT_REMOVED_INPUT_WORDS];
static uint32_t removed_native_events[2][OBJECT_REMOVED_EVENT_WORDS];
static uint32_t removed_native_count;
static const char *const removed_native_tables[] = {
    "_ab_removed_container_a", "_ab_removed_container_b", "_ab_removed_world_a",
    "_ab_removed_world_b",     "_ab_removed_dynamic_a",   "_ab_removed_dynamic_b",
    "_ab_removed_goals_a",     "_ab_removed_goals_b",     "_ab_removed_body",
    "_ab_removed_scalar"};

static int removed_native_notify(void *lua);
static int removed_native_exited(void *lua);
static int removed_native_check(void *lua);

static void removed_native_scalar(void *lua, uint32_t kind) {
    if (!kind) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0);
    } else if (kind == 4) {
        damage_lua.push_string(lua, "truthy");
    } else {
        damage_lua.get_field(lua, -10002, removed_native_tables[9]);
    }
}

static void removed_native_binding(void *lua, uint32_t kind, int (*function)(void *)) {
    if (kind == 5) {
        damage_lua.push_closure(lua, function, 0);
    } else if (kind == 4) {
        damage_lua.get_field(lua, -10002, removed_native_tables[9]);
    } else {
        removed_native_scalar(lua, kind);
    }
}

static void removed_native_value(void *lua, uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    output[0] = (uint32_t)damage_lua.type(lua, top);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, top) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, top)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        if (!damage_named(lua, top, "truthy")) {
            fail("Unknown original object-removed string\n");
        }
        output[1] = 1;
    } else if (output[0] == 5) {
        damage_lua.get_field(lua, top, "_id");
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, top);
        if (output[1] < 201 || output[1] > 210) {
            fail("Unknown original object-removed table\n");
        }
    } else if (output[0] != 0) {
        fail("Unexpected original object-removed scalar\n");
    }
}

static void removed_native_snapshot(void *lua,
                                    uint32_t output[OBJECT_REMOVED_STATE_WORDS]) {
    int top = damage_lua.get_top(lua);
    const char *const globals[] = {
        "objects", "levelGoals", "lastScoreTime", "oldScore", "time", "score", "dt"};
    size_t next = 0;
    for (size_t index = 0; index < 7; ++index) {
        damage_lua.get_field(lua, -10002, globals[index]);
        removed_native_value(lua, output + next);
        damage_lua.set_top(lua, top);
        next += 2;
    }
    for (size_t index = 2; index < 8; ++index) {
        damage_lua.get_field(lua, -10002, removed_native_tables[index]);
        damage_lua.get_field(lua, top + 1, "Object");
        removed_native_value(lua, output + next);
        damage_lua.set_top(lua, top);
        next += 2;
    }
    for (size_t index = 0; index < 2; ++index) {
        for (size_t field = 0; field < 2; ++field) {
            damage_lua.get_field(lua, -10002, removed_native_tables[index]);
            damage_lua.get_field(lua, top + 1, field ? "dynamic" : "world");
            removed_native_value(lua, output + next);
            damage_lua.set_top(lua, top);
            next += 2;
        }
    }
    if (next != OBJECT_REMOVED_STATE_WORDS) {
        fail("Original object-removed snapshot size mismatch\n");
    }
}

static uint32_t removed_native_pc(void *lua) {
    uintptr_t ci = *(const uint32_t *)((const uint8_t *)lua + 0x14);
    uintptr_t base = *(const uint32_t *)((const uint8_t *)lua + 0x28);
    if (ci <= base || (ci - base) % 24u) {
        fail("Missing original object-removed caller\n");
    }
    const uint32_t *caller = (const uint32_t *)(ci - 24u);
    const uint32_t *function = (const uint32_t *)(uintptr_t)caller[1];
    if (!function || function[1] != 6 || !function[0]) {
        fail("Missing original object-removed function\n");
    }
    const uint8_t *closure = (const uint8_t *)(uintptr_t)function[0];
    const uint32_t *prototype =
        (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[4];
    if (closure[6] || closure[7] != 1 || !prototype || prototype[11] != 39) {
        fail("Unexpected original object-removed closure\n");
    }
    const uint32_t *code = (const uint32_t *)(uintptr_t)prototype[3];
    uintptr_t begin = prototype[3], saved = caller[3];
    if (!code || code[0] != UINT32_C(0x00000045) || code[38] != UINT32_C(0x0080001e) ||
        saved <= begin || saved > begin + 156u || (saved - begin) % 4u) {
        fail("Unexpected original object-removed instructions\n");
    }
    return (uint32_t)((saved - begin) / 4u - 1u);
}

static void removed_native_mutate(void *lua, uint32_t kind) {
    uint32_t flags = removed_native_input[9];
    int top = damage_lua.get_top(lua);
    if (flags & 1u) {
        damage_lua.get_field(lua, -10002, removed_native_tables[1]);
        damage_lua.set_field(lua, -10002, "objects");
    }
    for (uint32_t field = 0; field < 2; ++field) {
        if (flags & (2u << field)) {
            damage_lua.get_field(lua, -10002, "objects");
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, top + 1, field ? "dynamic" : "world");
            damage_lua.set_top(lua, top);
        }
    }
    if (flags & 8u) {
        damage_lua.get_field(lua, -10002, removed_native_tables[7]);
        damage_lua.set_field(lua, -10002, "levelGoals");
    }
    if (flags & 16u) {
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, -10002, "levelGoals");
    }
    if (flags & 32u) {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -10002, "checkLevelGoalsDestroyed");
    }
    if (flags & 64u) {
        damage_number(lua, -10002, "dt", removed_native_input[10] ^ kind);
        damage_number(lua, -10002, "time", removed_native_input[11] ^ kind);
        damage_number(lua, -10002, "score", removed_native_input[12] ^ kind);
    }
    if (flags & 128u) {
        damage_lua.get_field(lua, -10002, removed_native_tables[8]);
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, top + 1, "controllable");
        damage_lua.set_top(lua, top);
    }
}

static int removed_native_call(void *lua, uint32_t kind, uint32_t pc, int arguments) {
    if (removed_native_pc(lua) != pc || damage_lua.get_top(lua) != arguments ||
        removed_native_count >= 2) {
        fail("Unexpected original object-removed boundary\n");
    }
    uint32_t *event = removed_native_events[removed_native_count++];
    event[0] = pc;
    event[1] = kind;
    event[2] = (uint32_t)arguments;
    event[3] = event[4] = event[5] = 0;
    if (arguments) {
        damage_lua.get_field(lua, 1, "_id");
        event[3] = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, arguments);
        if (event[3] != 209) {
            fail("Original object-removed lost cached object identity\n");
        }
    }
    if (arguments == 2) {
        removed_native_value(lua, event + 4);
    }
    removed_native_snapshot(lua, event + 6);
    removed_native_mutate(lua, kind);
    if (kind == 3) {
        removed_native_scalar(lua, removed_native_input[8]);
    } else {
        damage_lua.push_boolean(lua, 0);
    }
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int removed_native_notify(void *lua) {
    return removed_native_call(lua, 1, 21, 1);
}

static int removed_native_exited(void *lua) {
    return removed_native_call(lua, 2, 17, 2);
}

static int removed_native_check(void *lua) {
    return removed_native_call(lua, 3, 31, 0);
}

static void removed_native_prepare(void *lua) {
    for (uint32_t index = 0; index < 10; ++index) {
        damage_lua.create_table(lua, 0, 8);
        damage_lua.push_number(lua, (float)(201u + index));
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.set_field(lua, -10002, removed_native_tables[index]);
    }
    for (uint32_t index = 2; index < 8; ++index) {
        damage_lua.get_field(lua, -10002, removed_native_tables[index]);
        if (index < 4 && removed_native_input[1] == 5) {
            damage_lua.get_field(lua, -10002, removed_native_tables[8]);
        } else if (index < 4) {
            removed_native_scalar(lua, removed_native_input[1]);
        } else {
            damage_lua.push_boolean(lua, 1);
        }
        damage_lua.set_field(lua, 1, "Object");
        damage_lua.set_top(lua, 0);
    }
    for (uint32_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, removed_native_tables[index]);
        damage_lua.get_field(lua, -10002, removed_native_tables[2 + index]);
        damage_lua.set_field(lua, 1, "world");
        damage_lua.get_field(lua, -10002, removed_native_tables[4 + index]);
        damage_lua.set_field(lua, 1, "dynamic");
        damage_lua.set_top(lua, 0);
    }
    damage_lua.get_field(lua, -10002, removed_native_tables[8]);
    const char *const fields[] = {"controllable", "isMightyBait", "isMightyEagle"};
    for (size_t index = 0; index < 3; ++index) {
        removed_native_scalar(lua, removed_native_input[2 + index]);
        damage_lua.set_field(lua, 1, fields[index]);
    }
    removed_native_binding(lua, removed_native_input[5], removed_native_exited);
    damage_lua.set_field(lua, 1, "exitedFunction");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, removed_native_tables[0]);
    damage_lua.set_field(lua, -10002, "objects");
    damage_lua.get_field(lua, -10002, removed_native_tables[6]);
    damage_lua.set_field(lua, -10002, "levelGoals");
    damage_number(lua, -10002, "lastScoreTime", UINT32_C(0x41400000));
    damage_number(lua, -10002, "oldScore", UINT32_C(0x41a00000));
    damage_number(lua, -10002, "dt", removed_native_input[10]);
    damage_number(lua, -10002, "time", removed_native_input[11]);
    damage_number(lua, -10002, "score", removed_native_input[12]);
    removed_native_binding(lua, removed_native_input[7], removed_native_check);
    damage_lua.set_field(lua, -10002, "checkLevelGoalsDestroyed");
    if (removed_native_input[6] == 2) {
        damage_lua.push_closure(lua, removed_native_notify, 0);
    } else {
        removed_native_scalar(lua, removed_native_input[6]);
    }
    damage_lua.set_field(lua, -10002, "_ab_removed_notification");
}

static void removed_native_load(void *lua) {
    char chunk[429];
    int file = system_call(
        5, (int)(uintptr_t)"/data/local/tmp/ab520/collision/object-removed.lua", NULL,
        0);
    if (file < 0 || system_call(3, file, chunk, sizeof(chunk)) != sizeof(chunk) ||
        system_call(6, file, NULL, 0) != 0 ||
        damage_lua.load(lua, chunk, sizeof(chunk), "original-object-removed")) {
        fail("Cannot load original object-removed prototype\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_removed_function");
    damage_lua.get_field(lua, -10002, "_ab_removed_function");
    damage_lua.get_field(lua, -10002, "_ab_removed_notification");
    const uint8_t *closure = damage_lua.to_pointer(lua, 1);
    if (!closure || closure[6] || closure[7] != 1) {
        fail("Unexpected original object-removed upvalue layout\n");
    }
    const uint32_t *upvalue =
        (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[5];
    if (!upvalue || upvalue[2] != (uint32_t)(uintptr_t)(upvalue + 3) ||
        (((const uint8_t *)upvalue)[5] & 4u)) {
        fail("Expected fresh closed object-removed upvalue\n");
    }
    uint32_t *value = (uint32_t *)(uintptr_t)upvalue[2];
    if (value[1]) {
        fail("Expected initially nil object-removed upvalue\n");
    }
    /* Supply the rooted notification boundary to the stripped closure's checked
     * closed slot. The original code, prototype and registers are unchanged. */
    uint32_t binding = removed_native_input[6];
    value[0] = binding == 2 ? (uint32_t)(uintptr_t)damage_lua.to_pointer(lua, 2) : 0;
    value[1] = binding == 2 ? 6u : binding == 1 ? 1u : 0u;
    damage_lua.set_top(lua, 1);
}

static int removed_native_error(const char *message, const char *wanted) {
    for (size_t start = 0; message && message[start]; ++start) {
        size_t index = 0;
        while (wanted[index] && message[start + index] == wanted[index]) {
            ++index;
        }
        if (!wanted[index]) {
            return 1;
        }
    }
    return 0;
}

static void probe_object_removed(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original object-removed state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    semihost(4, "AB_ANDROID_OBJECT_REMOVED_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        object_removed_case(sample, removed_native_input);
        print_words(removed_native_input, OBJECT_REMOVED_INPUT_WORDS);
        removed_native_count = 0;
        removed_native_prepare(lua);
        removed_native_load(lua);
        damage_lua.push_string(lua, "Object");
        uint32_t failed = damage_lua.call(lua, 1, 0, 0) != 0;
        if (failed) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            if (!removed_native_error(message, "index") &&
                !removed_native_error(message, "call")) {
                fail(message ? message : "Unknown original object-removed error\n");
            }
        }
        damage_lua.set_top(lua, 0);
        print_words(&failed, 1);
        uint32_t output[OBJECT_REMOVED_STATE_WORDS];
        removed_native_snapshot(lua, output);
        print_words(output, OBJECT_REMOVED_STATE_WORDS);
        print_words(&removed_native_count, 1);
        for (uint32_t index = 0; index < removed_native_count; ++index) {
            print_words(removed_native_events[index], OBJECT_REMOVED_EVENT_WORDS);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB object removed reference complete\n");
}

#endif
