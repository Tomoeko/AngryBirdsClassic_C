#ifndef AB_ORACLE_BLOCK_REMOVAL_PARTICLES_H
#define AB_ORACLE_BLOCK_REMOVAL_PARTICLES_H

#if defined(AB_ORACLE_BLOCK_REMOVAL_PARTICLES_ONLY)
#include "../../tests/block_removal_particles_cases.h"
#endif

static uint32_t removal_particles_input[17], removal_particles_events[8][56];
static uint32_t removal_particles_count, removal_particles_calls[5],
    removal_particles_cutoff;
static char removal_particles_chunk[3359];
static void (*removal_particles_raw_set)(void *, int, int);
static void (*removal_particles_raw_get)(void *, int, int);
static const char *const removal_particles_strings[] = {
    "body", "renamed", "old", "new", "descriptor", "truthy", "list"};
static const char *const removal_particles_tables[] = {
    "_ab_particles_object", "_ab_particles_unused", "_ab_particles_current",
    "_ab_particles_value"};
static const char *const removal_particles_lists[] = {"_ab_particles_list1",
                                                      "_ab_particles_list2"};

static uint32_t removal_particles_pc(void *lua) {
    const uint32_t *state = lua;
    uintptr_t ci = state[5], base = state[10];
    if (!ci || !base || ci < base + 24u || (ci - base) % 24u) {
        fail("Unexpected original removal particle frame bounds\n");
    }
    const uint32_t *caller = (const uint32_t *)(ci - 24u);
    const uint32_t *function = (const uint32_t *)(uintptr_t)caller[1];
    if (!function || function[1] != 6 || !function[0]) {
        fail("Missing original removal particle caller\n");
    }
    const uint32_t *closure = (const uint32_t *)(uintptr_t)function[0];
    if (((const uint8_t *)closure)[6] || ((const uint8_t *)closure)[7] != 1 ||
        !closure[4]) {
        fail("Unexpected original removal particle closure\n");
    }
    const uint32_t *prototype = (const uint32_t *)(uintptr_t)closure[4];
    if (prototype[11] != 479 || !prototype[3]) {
        fail("Unexpected original removal particle prototype\n");
    }
    const uint32_t *code = (const uint32_t *)(uintptr_t)prototype[3];
    uintptr_t begin = prototype[3], saved = caller[3];
    if (code[0] != UINT32_C(0x00000005) || code[478] != UINT32_C(0x0080001e) ||
        saved <= begin || saved > begin + 479u * 4u || (saved - begin) % 4u) {
        fail("Unexpected original removal particle instruction\n");
    }
    return (uint32_t)((saved - begin) / 4u - 1u);
}

static int removal_particles_function1(void *lua);
static int removal_particles_function2(void *lua);
static int removal_particles_function3(void *lua);
static int removal_particles_new1(void *lua);
static int removal_particles_new2(void *lua);
static int removal_particles_old1(void *lua);
static int removal_particles_old2(void *lua);
static int removal_particles_property(void *lua);

static uint32_t removal_particles_function_id(void *lua, int index,
                                              int (*first)(void *),
                                              int (*second)(void *),
                                              int (*third)(void *)) {
    const uint32_t *closure = damage_lua.to_pointer(lua, index);
    if (!closure || !((const uint8_t *)closure)[6]) {
        fail("Expected an original C fixture closure\n");
    }
    if (closure[4] == (uint32_t)(uintptr_t)first) {
        return 1;
    }
    if (closure[4] == (uint32_t)(uintptr_t)second) {
        return 2;
    }
    if (third && closure[4] == (uint32_t)(uintptr_t)third) {
        return 3;
    }
    fail("Unexpected original removal particle function\n");
    return 0;
}

static void removal_particles_value(void *lua, int index, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, index);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, index) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t string = 0; string < 7; ++string) {
            if (damage_named(lua, index, removal_particles_strings[string])) {
                output[1] = string + 1u;
                return;
            }
        }
        fail("Unexpected original removal particle string\n");
    } else if (output[0] == 5) {
        damage_lua.get_field(lua, index, "_id");
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, -2);
        if (!((output[1] >= 201 && output[1] <= 204) || output[1] == 301 ||
              output[1] == 302)) {
            fail("Unexpected original removal particle table\n");
        }
    } else if (output[0] == 6) {
        output[1] = removal_particles_function_id(
            lua, index, removal_particles_function1, removal_particles_function2,
            removal_particles_function3);
    } else if (output[0]) {
        fail("Unexpected original removal particle value\n");
    }
}

static void removal_particles_scalar(void *lua, uint32_t kind, const char *string,
                                     uint32_t table) {
    if (!kind) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0);
    } else if (kind == 4) {
        damage_lua.push_string(lua, string);
    } else {
        damage_lua.get_field(lua, -10002, removal_particles_tables[table]);
    }
}

static void removal_particles_set(void *lua, const char *table, const char *field,
                                  uint32_t kind, const char *string,
                                  uint32_t value_table) {
    int top = damage_lua.get_top(lua);
    if (table) {
        damage_lua.get_field(lua, -10002, table);
    }
    removal_particles_scalar(lua, kind, string, value_table);
    damage_lua.set_field(lua, table ? top + 1 : -10002, field);
    damage_lua.set_top(lua, top);
}

static void removal_particles_list(void *lua, uint32_t kind) {
    if (!kind) {
        damage_lua.push_nil(lua);
    } else if (kind == 1) {
        damage_lua.push_boolean(lua, 0);
    } else if (kind == 2) {
        damage_lua.push_number(lua, 0);
    } else if (kind == 5) {
        damage_lua.push_string(lua, "list");
    } else {
        damage_lua.get_field(lua, -10002, removal_particles_lists[kind - 3u]);
    }
}

static void removal_particles_item(void *lua, uint32_t kind) {
    if (kind <= 2) {
        removal_particles_scalar(lua, kind == 2 ? 3u : kind, "", 3);
    } else {
        damage_lua.push_closure(lua,
                                kind == 3   ? removal_particles_function1
                                : kind == 4 ? removal_particles_function2
                                            : removal_particles_function3,
                                0);
    }
}

static uint32_t removal_particles_binding(void *lua, const char *name,
                                          int (*first)(void *), int (*second)(void *)) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, name);
    uint32_t binding = damage_lua.type(lua, -1) == 6
                           ? removal_particles_function_id(lua, -1, first, second, NULL)
                           : 0;
    damage_lua.set_top(lua, top);
    return binding;
}

static void removal_particles_snapshot(void *lua, uint32_t output[36]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
    const char *const fields[] = {"name", "killSilently", "removeFunctions"};
    for (uint32_t field = 0; field < 3; ++field) {
        damage_lua.get_field(lua, top + 1, fields[field]);
        removal_particles_value(lua, top + 2, output + field * 2u);
        damage_lua.set_top(lua, top + 1);
    }
    damage_lua.set_top(lua, top);
    for (uint32_t list = 0; list < 2; ++list) {
        damage_lua.get_field(lua, -10002, removal_particles_lists[list]);
        for (uint32_t slot = 0; slot < 4; ++slot) {
            removal_particles_raw_get(lua, top + 1, (int)slot + 1);
            removal_particles_value(lua, top + 2, output + 6 + list * 8u + slot * 2u);
            damage_lua.set_top(lua, top + 1);
        }
        damage_lua.set_top(lua, top);
    }
    damage_lua.get_field(lua, -10002, "_ab_particles_use_new");
    removal_particles_value(lua, top + 1, output + 22);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "_ab_particles_fresh");
    removal_particles_value(lua, top + 1, output + 24);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, removal_particles_tables[2]);
    damage_lua.get_field(lua, top + 1, "removal");
    removal_particles_value(lua, top + 2, output + 26);
    damage_lua.set_top(lua, top);
    output[28] = removal_particles_input[10] & 1u;
    damage_lua.get_field(lua, -10002, "_ab_particles_script");
    damage_lua.get_field(lua, top + 1, "get");
    if (damage_lua.type(lua, -1) == 6) {
        output[28] |= 2u;
    }
    damage_lua.set_top(lua, top);
    output[29] = removal_particles_binding(
        lua, "addParticlesNew", removal_particles_new1, removal_particles_new2);
    output[30] = removal_particles_binding(lua, "addParticles", removal_particles_old1,
                                           removal_particles_old2);
    output[28] |= (output[29] ? 8u : 0u) | (output[30] ? 4u : 0u);
    for (uint32_t kind = 0; kind < 5; ++kind) {
        output[31 + kind] = removal_particles_calls[kind];
    }
}

static uint32_t *removal_particles_record(void *lua, uint32_t kind, uint32_t pc,
                                          uint32_t variant, uint32_t argc) {
    if (removal_particles_pc(lua) != pc || damage_lua.get_top(lua) != (int)argc ||
        removal_particles_count >= 8) {
        fail("Unexpected original removal particle boundary\n");
    }
    uint32_t *event = removal_particles_events[removal_particles_count++];
    for (uint32_t word = 0; word < 56; ++word) {
        event[word] = 0;
    }
    event[0] = kind;
    event[1] = pc;
    event[2] = variant;
    event[3] = argc;
    removal_particles_snapshot(lua, event + 20);
    ++removal_particles_calls[kind - 1u];
    return event;
}

static void removal_particles_require_object(void *lua, int index) {
    uint32_t value[2];
    removal_particles_value(lua, index, value);
    if (value[0] != 5 || value[1] != 201) {
        fail("Original removal particles lost cached object\n");
    }
}

static void removal_particles_clear(void *lua, const char *global) {
    damage_lua.push_nil(lua);
    damage_lua.set_field(lua, -10002, global);
}

#if !defined(AB_ORACLE_BLOCK_REMOVAL_PASS_ONLY)
static int removal_particles_notify(void *lua) {
    uint32_t *event = removal_particles_record(lua, 1, 233, 0, 1);
    removal_particles_require_object(lua, 1);
    event[4] = 201;
    uint32_t mutation = removal_particles_input[11];
    if (mutation & 1u) {
        damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
        removal_particles_list(lua, 4);
        damage_lua.set_field(lua, 2, "removeFunctions");
        damage_lua.set_top(lua, 1);
    }
    if (mutation & 2u) {
        removal_particles_set(lua, removal_particles_tables[0], "killSilently", 1, "",
                              3);
    }
    if (mutation & 4u) {
        removal_particles_set(lua, NULL, "_ab_particles_use_new", 2, "", 3);
    }
    if (mutation & 8u) {
        removal_particles_set(lua, "_ab_particles_script", "get", 0, "", 3);
    }
    if (mutation & 16u) {
        removal_particles_clear(lua, "addParticlesNew");
    }
    if (mutation & 32u) {
        removal_particles_clear(lua, "addParticles");
    }
    if (mutation & 64u) {
        removal_particles_set(lua, removal_particles_tables[0], "name", 4, "renamed",
                              3);
    }
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}
#endif

static int removal_particles_function(void *lua, uint32_t variant) {
    uint32_t *event = removal_particles_record(lua, 2, 244, variant, 1);
    removal_particles_require_object(lua, 1);
    event[4] = 201;
    uint32_t mutation = removal_particles_input[12], next = removal_particles_calls[1];
    if (next < 4 && (mutation & 3u)) {
        damage_lua.get_field(lua, -10002, removal_particles_lists[0]);
        removal_particles_item(lua, mutation & 2u ? 0u : 4u);
        removal_particles_raw_set(lua, 2, (int)next + 1);
        damage_lua.set_top(lua, 1);
    }
    if (mutation & 4u) {
        damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
        removal_particles_list(lua, 4);
        damage_lua.set_field(lua, 2, "removeFunctions");
        damage_lua.set_top(lua, 1);
    }
    if (mutation & 8u) {
        removal_particles_set(lua, removal_particles_tables[0], "killSilently", 2, "",
                              3);
    }
    if (mutation & 16u) {
        removal_particles_set(lua, removal_particles_tables[0], "killSilently", 1, "",
                              3);
    }
    if (mutation & 32u) {
        removal_particles_clear(lua, "addParticlesNew");
    }
    if (mutation & 64u) {
        removal_particles_set(lua, NULL, "_ab_particles_use_new", 2, "", 3);
    }
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int removal_particles_function1(void *lua) {
    return removal_particles_function(lua, 1);
}

static int removal_particles_function2(void *lua) {
    return removal_particles_function(lua, 2);
}

static int removal_particles_function3(void *lua) {
    return removal_particles_function(lua, 3);
}

static int removal_particles_property(void *lua) {
    uint32_t pc = removal_particles_pc(lua);
    if (pc == 164 || pc == 173) {
        if (damage_lua.get_top(lua) != 2 ||
            !damage_named(lua, 2, pc == 164 ? "particles" : "particleAmount")) {
            fail("Unexpected original pre-particle property\n");
        }
        damage_lua.get_field(
            lua, -10002, pc == 164 ? "_ab_particles_cached" : "_ab_particles_amount");
        return 1;
    }
    int use = pc == 253;
    uint32_t *event = removal_particles_record(lua, 3, use ? 253u : 261u, 0, 2);
    if (!damage_named(lua, 2, use ? "useNewParticleTable" : "particles")) {
        fail("Unexpected original removal particle property\n");
    }
    damage_lua.get_field(lua, 1, "_object");
    removal_particles_require_object(lua, 3);
    damage_lua.set_top(lua, 2);
    event[4] = 501;
    event[5] = use ? 1u : 2u;
    uint32_t mutation = removal_particles_input[use ? 13 : 14];
    if (mutation & 1u) {
        damage_function(lua, -10002, "addParticlesNew", removal_particles_new2);
    }
    if (mutation & 2u) {
        if (use) {
            removal_particles_clear(lua, "_ab_particles_fresh");
        } else {
            removal_particles_set(lua, removal_particles_tables[2], "removal", 1, "",
                                  3);
        }
    }
    if ((use && (mutation & 4u)) || (!use && (mutation & 8u))) {
        removal_particles_set(lua, removal_particles_tables[0], "killSilently", 2, "",
                              3);
    }
    if (use && (mutation & 8u)) {
        removal_particles_set(lua, removal_particles_tables[0], "name", 4, "renamed",
                              3);
    }
    if ((use && (mutation & 16u)) || (!use && (mutation & 4u))) {
        removal_particles_clear(lua, "addParticlesNew");
    }
    damage_lua.get_field(lua, -10002,
                         use ? "_ab_particles_use_new" : "_ab_particles_fresh");
    return 1;
}

static int removal_particles_emit(void *lua, uint32_t kind, uint32_t variant) {
    uint32_t argc = kind == 4 ? 4u : 5u;
    uint32_t *event =
        removal_particles_record(lua, kind, kind == 4 ? 265u : 273u, variant, argc);
    if (!damage_named(lua, 1, "queued")) {
        fail("Original removal particle emit lost captured key\n");
    }
    event[4] = 1;
    for (uint32_t argument = 2; argument <= argc; ++argument) {
        removal_particles_value(lua, (int)argument, event + 4 + (argument - 1u) * 2u);
    }
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int removal_particles_new1(void *lua) {
    return removal_particles_emit(lua, 4, 1);
}

static int removal_particles_new2(void *lua) {
    return removal_particles_emit(lua, 4, 2);
}

static int removal_particles_old1(void *lua) {
    return removal_particles_emit(lua, 5, 1);
}

static int removal_particles_old2(void *lua) {
    return removal_particles_emit(lua, 5, 2);
}

static int removal_particles_noop(void *lua) {
    (void)lua;
    return 0;
}

static int removal_particles_nil(void *lua) {
    damage_lua.push_nil(lua);
    return 1;
}

static int removal_particles_definition(void *lua) {
    if (removal_particles_pc(lua) != 48 || damage_lua.get_top(lua) != 1 ||
        !damage_named(lua, 1, "queued")) {
        fail("Unexpected original pre-particle definition\n");
    }
    damage_lua.get_field(lua, -10002, "_ab_particles_definition");
    return 1;
}

static int removal_particles_zero(void *lua) {
    if (removal_particles_pc(lua) != 51) {
        fail("Unexpected original pre-particle score\n");
    }
    damage_lua.push_number(lua, 0);
    return 1;
}

static int removal_particles_random(void *lua) {
    if (removal_particles_pc(lua) != 182) {
        fail("Unexpected original pre-particle random\n");
    }
    damage_lua.push_number(lua, 17);
    return 1;
}

static int removal_particles_update(void *lua) {
    if (removal_particles_pc(lua) != 230) {
        fail("Unexpected original pre-notification update\n");
    }
    removal_particles_set(lua, removal_particles_tables[0], "killSilently",
                          removal_particles_input[1], "truthy", 3);
    if (!(removal_particles_input[10] & 2u)) {
        removal_particles_set(lua, "_ab_particles_script", "get", 0, "", 3);
    }
    return 0;
}

static int removal_particles_suffix(void *lua) {
    if (removal_particles_pc(lua) != 288 || damage_lua.get_top(lua) != 1 ||
        !damage_named(lua, 1, "queued") || removal_particles_cutoff++) {
        fail("Unexpected original post-particle cutoff\n");
    }
    return 0;
}

static void removal_particles_bind(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_particles_removeBlocks");
    damage_lua.get_field(lua, -10002, "_ab_particles_notification");
    const uint8_t *closure = damage_lua.to_pointer(lua, top + 1);
    const uint8_t *replacement = damage_lua.to_pointer(lua, top + 2);
    if (!closure || closure[6] || closure[7] != 1 || !replacement || !replacement[6] ||
        replacement[7]) {
        fail("Unexpected original removal notification closure layout\n");
    }
    const uint32_t *upvalue =
        (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[5];
    if (!upvalue || upvalue[2] != (uint32_t)(uintptr_t)(upvalue + 3) ||
        (((const uint8_t *)upvalue)[5] & 4u)) {
        fail("Expected original closed removal notification upvalue\n");
    }
    uint32_t *value = (uint32_t *)(uintptr_t)upvalue[2];
    if (value[1] != 0 &&
        (value[1] != 6 || value[0] != (uint32_t)(uintptr_t)replacement)) {
        fail("Unexpected original prior removal notification binding\n");
    }
    /* A rooted controlled notification host is an environment input. Stripped
     * upvalue names reject lua_setupvalue; bind the checked closed slot only.
     * No original code/prototype/register is changed. */
    value[0] = removal_particles_input[10] & 1u ? (uint32_t)(uintptr_t)replacement : 0;
    value[1] = removal_particles_input[10] & 1u ? 6u : 0u;
    damage_lua.set_top(lua, top);
}

static void removal_particles_prepare(void *lua) {
    for (uint32_t table = 0; table < 4; ++table) {
        damage_lua.create_table(lua, 0, 8);
        damage_lua.push_number(lua, (float)(201u + table));
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.set_field(lua, -10002, removal_particles_tables[table]);
    }
    removal_particles_set(lua, removal_particles_tables[0], "name", 4, "body", 3);
    removal_particles_set(lua, removal_particles_tables[0], "strength", 3, "", 3);
    removal_particles_set(lua, removal_particles_tables[2], "removal",
                          removal_particles_input[9], "descriptor", 3);
    for (uint32_t list = 0; list < 2; ++list) {
        damage_lua.create_table(lua, 4, 1);
        damage_lua.push_number(lua, (float)(301u + list));
        damage_lua.set_field(lua, 1, "_id");
        for (uint32_t slot = 0; slot < 4; ++slot) {
            removal_particles_item(
                lua, (removal_particles_input[3 + list] >> (slot * 3u)) & 7u);
            removal_particles_raw_set(lua, 1, (int)slot + 1);
        }
        damage_lua.set_field(lua, -10002, removal_particles_lists[list]);
    }
    damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
    removal_particles_list(lua, removal_particles_input[2]);
    damage_lua.set_field(lua, 1, "removeFunctions");
    damage_lua.push_string(lua, "raw");
    damage_lua.set_field(lua, 1, "definition");
    damage_lua.set_top(lua, 0);
    removal_particles_set(lua, NULL, "_ab_particles_use_new",
                          removal_particles_input[5], "truthy", 3);
    removal_particles_set(lua, NULL, "_ab_particles_fresh", removal_particles_input[8],
                          "new", 2);
    removal_particles_set(lua, NULL, "_ab_particles_cached", removal_particles_input[6],
                          "old", 3);
    removal_particles_set(lua, NULL, "_ab_particles_amount", removal_particles_input[7],
                          "truthy", 3);
    damage_lua.create_table(lua, 0, 3);
    damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
    damage_lua.set_field(lua, 1, "_object");
    damage_function(lua, 1, "onDestroy", removal_particles_noop);
    damage_function(lua, 1, "get", removal_particles_property);
    damage_lua.set_field(lua, -10002, "_ab_particles_script");
    damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
    damage_lua.get_field(lua, -10002, "_ab_particles_script");
    damage_lua.set_field(lua, 1, "gameObjectScript");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
    damage_lua.set_field(lua, 1, "queued");
    damage_lua.set_field(lua, -10002, "deadBlocks");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "deadBlocks");
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_field(lua, -10002, "objects");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 2, "score", 0);
    damage_lua.create_table(lua, 1, 0);
    damage_lua.set_field(lua, 2, "individualDestructionScores");
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.set_field(lua, -10002, "scoreTable");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, 1, "spriteScore");
    damage_lua.set_field(lua, -10002, "_ab_particles_definition");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "destroyedSound", removal_particles_nil);
    damage_lua.set_field(lua, -10002, "g_levelEffect");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "random", removal_particles_random);
    damage_lua.set_field(lua, -10002, "math");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 2, "raw");
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.set_field(lua, -10002, "blockTable");
    damage_lua.push_number(lua, 1);
    damage_lua.set_field(lua, -10002, "floatingScoreScaling");
    damage_function(lua, -10002, "getObjectDefinition", removal_particles_definition);
    damage_function(lua, -10002, "g_get_score_to_add_for_breaking",
                    removal_particles_zero);
    damage_function(lua, -10002, "addFloatingScore", removal_particles_noop);
    damage_function(lua, -10002, "updateDestroyedBlocksValues",
                    removal_particles_update);
    damage_function(lua, -10002, "removeObject", removal_particles_suffix);
    if (removal_particles_input[10] & 8u) {
        damage_function(lua, -10002, "addParticlesNew",
                        removal_particles_input[15] == 1 ? removal_particles_new1
                                                         : removal_particles_new2);
    } else {
        removal_particles_clear(lua, "addParticlesNew");
    }
    if (removal_particles_input[10] & 4u) {
        damage_function(lua, -10002, "addParticles",
                        removal_particles_input[16] == 1 ? removal_particles_old1
                                                         : removal_particles_old2);
    } else {
        removal_particles_clear(lua, "addParticles");
    }
    if (damage_lua.load(lua, removal_particles_chunk, sizeof(removal_particles_chunk),
                        "original-removeBlocks")) {
        fail("Cannot reload original removal particle prototype\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_particles_removeBlocks");
    removal_particles_bind(lua);
}

static int removal_particles_contains(const char *message, const char *wanted) {
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

#if defined(AB_ORACLE_BLOCK_REMOVAL_PARTICLES_ONLY)
static void probe_block_removal_particles(void) {
    initialize_damage_lua();
    removal_particles_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    removal_particles_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original removal particle state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    int file = system_call(
        5, (int)(uintptr_t)"/data/local/tmp/ab520/collision/remove-blocks.lua", NULL,
        0);
    if (file < 0 ||
        system_call(3, file, removal_particles_chunk,
                    sizeof(removal_particles_chunk)) !=
            sizeof(removal_particles_chunk) ||
        system_call(6, file, NULL, 0) != 0) {
        fail("Cannot load original removal particle prototype\n");
    }
    damage_function(lua, -10002, "_ab_particles_notification",
                    removal_particles_notify);
    semihost(4, "AB_ANDROID_BLOCK_REMOVAL_PARTICLES_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        block_removal_particles_case(sample, removal_particles_input);
        print_words(removal_particles_input, 17);
        removal_particles_count = removal_particles_cutoff = 0;
        for (uint32_t kind = 0; kind < 5; ++kind) {
            removal_particles_calls[kind] = 0;
        }
        removal_particles_prepare(lua);
        damage_lua.get_field(lua, -10002, "_ab_particles_removeBlocks");
        int status = damage_lua.call(lua, 0, 0, 0);
        const char *message = status ? damage_lua.to_string(lua, -1, NULL) : NULL;
        if (!status ||
            (removal_particles_cutoff &&
             (removal_particles_cutoff != 1 ||
              !removal_particles_contains(message, "index"))) ||
            (!removal_particles_cutoff &&
             !removal_particles_contains(message, "index") &&
             !removal_particles_contains(message, "call") &&
             !removal_particles_contains(message, "table expected"))) {
            fail(message ? message : "Original removal particles bypassed cutoff\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t failed = removal_particles_cutoff == 0;
        print_words(&failed, 1);
        uint32_t state[36];
        removal_particles_snapshot(lua, state);
        print_words(state, 36);
        print_words(&removal_particles_count, 1);
        for (uint32_t event = 0; event < removal_particles_count; ++event) {
            print_words(removal_particles_events[event], 56);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB block removal particles reference complete\n");
}
#endif

#endif
