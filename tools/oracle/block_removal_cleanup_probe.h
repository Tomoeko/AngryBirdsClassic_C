#ifndef AB_ORACLE_BLOCK_REMOVAL_CLEANUP_H
#define AB_ORACLE_BLOCK_REMOVAL_CLEANUP_H

#include "block_removal_particles_probe.h"
#include "../../tests/block_removal_cleanup_cases.h"

static uint32_t removal_cleanup_input[15], removal_cleanup_events[2][49];
static uint32_t removal_cleanup_count, removal_cleanup_calls[2],
    removal_cleanup_started;
static const char *const removal_cleanup_strings[] = {"body", "renamed", "truthy"};
static const char *const removal_cleanup_tables[] = {
    "_ab_cleanup_object", "_ab_cleanup_container", "_ab_cleanup_world",
    "_ab_cleanup_dynamic", "_ab_cleanup_value"};

static void removal_cleanup_scalar(void *lua, uint32_t kind, uint32_t table) {
    if (!kind) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0);
    } else if (kind == 4) {
        damage_lua.push_string(lua, "truthy");
    } else {
        damage_lua.get_field(lua, -10002, removal_cleanup_tables[table]);
    }
}

static void removal_cleanup_value(void *lua, int index, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, index);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, index) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t string = 0; string < 3; ++string) {
            if (damage_named(lua, index, removal_cleanup_strings[string])) {
                output[1] = string + 1u;
                return;
            }
        }
        fail("Unexpected original removal cleanup string\n");
    } else if (output[0] == 5) {
        damage_lua.get_field(lua, index, "_id");
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, -2);
        if (output[1] < 201 || output[1] > 205) {
            fail("Unexpected original removal cleanup table\n");
        }
    } else if (output[0]) {
        fail("Unexpected original removal cleanup value\n");
    }
}

static void removal_cleanup_set(void *lua, const char *table, const char *field,
                                uint32_t kind, uint32_t value_table) {
    int top = damage_lua.get_top(lua);
    if (table) {
        damage_lua.get_field(lua, -10002, table);
    }
    removal_cleanup_scalar(lua, kind, value_table);
    damage_lua.set_field(lua, table ? top + 1 : -10002, field);
    damage_lua.set_top(lua, top);
}

static void removal_cleanup_text(void *lua, const char *table, const char *field,
                                 const char *value) {
    int top = damage_lua.get_top(lua);
    if (table) {
        damage_lua.get_field(lua, -10002, table);
    }
    damage_lua.push_string(lua, value);
    damage_lua.set_field(lua, table ? top + 1 : -10002, field);
    damage_lua.set_top(lua, top);
}

static void removal_cleanup_snapshot(void *lua, uint32_t output[29]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, removal_cleanup_tables[0]);
    const char *const fields[] = {"name", "levelGoal", "isGiftBox", "x", "y"};
    for (uint32_t field = 0; field < 5; ++field) {
        damage_lua.get_field(lua, top + 1, fields[field]);
        removal_cleanup_value(lua, top + 2, output + field * 2u);
        damage_lua.set_top(lua, top + 1);
    }
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "objects");
    removal_cleanup_value(lua, top + 1, output + 10);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, removal_cleanup_tables[1]);
    for (uint32_t field = 0; field < 2; ++field) {
        damage_lua.get_field(lua, top + 1, field ? "dynamic" : "world");
        removal_cleanup_value(lua, top + 2, output + 12 + field * 2u);
        damage_lua.set_top(lua, top + 1);
    }
    damage_lua.set_top(lua, top);
    for (uint32_t map = 0; map < 2; ++map) {
        damage_lua.get_field(lua, -10002, removal_cleanup_tables[map + 2u]);
        damage_lua.get_field(lua, top + 1, "queued");
        removal_cleanup_value(lua, top + 2, output + 16 + map * 2u);
        damage_lua.set_top(lua, top);
    }
    const char *const maps[] = {"levelGoals", "deadBlocks"};
    const char *const backing[] = {"_ab_cleanup_goals", "_ab_cleanup_dead"};
    for (uint32_t map = 0; map < 2; ++map) {
        damage_lua.get_field(lua, -10002, maps[map]);
        output[20 + map] = damage_lua.type(lua, -1) == 5;
        damage_lua.set_top(lua, top);
        damage_lua.get_field(lua, -10002, backing[map]);
        damage_lua.get_field(lua, top + 1, "queued");
        output[22 + map] = damage_lua.type(lua, -1) != 0;
        damage_lua.set_top(lua, top);
    }
    damage_lua.get_field(lua, -10002, "levelName");
    removal_cleanup_value(lua, top + 1, output + 24);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "removePig");
    output[26] = damage_lua.type(lua, -1) == 6;
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "GiftBoxManager");
    damage_lua.get_field(lua, top + 1, "collectGiftBox");
    output[26] |= damage_lua.type(lua, -1) == 6 ? 2u : 0u;
    damage_lua.set_top(lua, top);
    output[27] = removal_cleanup_calls[0];
    output[28] = removal_cleanup_calls[1];
}

static uint32_t *removal_cleanup_record(void *lua, uint32_t kind, uint32_t pc,
                                        uint32_t argc) {
    if (removal_particles_pc(lua) != pc || damage_lua.get_top(lua) != (int)argc ||
        removal_cleanup_count >= 2) {
        fail("Unexpected original removal cleanup boundary\n");
    }
    uint32_t *event = removal_cleanup_events[removal_cleanup_count++];
    for (uint32_t word = 0; word < 49; ++word) {
        event[word] = 0;
    }
    event[0] = kind;
    event[1] = pc;
    event[2] = 0;
    event[3] = argc;
    removal_cleanup_snapshot(lua, event + 20);
    ++removal_cleanup_calls[kind - 1u];
    return event;
}

static int removal_cleanup_pig(void *lua) {
    uint32_t *event = removal_cleanup_record(lua, 1, 466, 1);
    if (!damage_named(lua, 1, "queued")) {
        fail("Original removePig lost captured map key\n");
    }
    event[4] = 1;
    uint32_t mutation = removal_cleanup_input[12];
    if (mutation & 1u) {
        removal_cleanup_set(lua, removal_cleanup_tables[0], "isGiftBox", 0, 4);
    }
    if (mutation & 2u) {
        removal_cleanup_set(lua, removal_cleanup_tables[0], "isGiftBox", 2, 4);
    }
    if (mutation & 4u) {
        removal_cleanup_text(lua, NULL, "levelName", "renamed");
    }
    if (mutation & 8u) {
        removal_cleanup_set(lua, removal_cleanup_tables[0], "x", 1, 4);
        removal_cleanup_set(lua, removal_cleanup_tables[0], "y", 5, 4);
    }
    if (mutation & 16u) {
        removal_cleanup_set(lua, "GiftBoxManager", "collectGiftBox", 0, 4);
    }
    if (mutation & 32u) {
        removal_particles_clear(lua, "objects");
    }
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int removal_cleanup_gift(void *lua) {
    uint32_t *event = removal_cleanup_record(lua, 2, 475, 4);
    damage_lua.get_field(lua, 1, "_id");
    if (damage_lua.to_number(lua, -1) != 501) {
        fail("Original gift collection lost manager receiver\n");
    }
    damage_lua.set_top(lua, 4);
    event[4] = 501;
    for (uint32_t argument = 2; argument <= 4; ++argument) {
        removal_cleanup_value(lua, (int)argument, event + 5 + (argument - 2u) * 2u);
    }
    uint32_t mutation = removal_cleanup_input[14];
    if (mutation & 1u) {
        removal_cleanup_text(lua, removal_cleanup_tables[0], "name", "renamed");
    }
    if (mutation & 2u) {
        removal_cleanup_set(lua, removal_cleanup_tables[0], "levelGoal", 1, 4);
    }
    if (mutation & 4u) {
        removal_particles_clear(lua, "levelName");
    }
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int removal_cleanup_begin(void *lua) {
    if (removal_particles_pc(lua) != 288 || damage_lua.get_top(lua) != 1 ||
        !damage_named(lua, 1, "queued") || removal_cleanup_started++) {
        fail("Unexpected original cleanup entry boundary\n");
    }
    const char *const fields[] = {"levelGoal", "isGiftBox", "x", "y"};
    for (uint32_t field = 0; field < 4; ++field) {
        removal_cleanup_set(lua, removal_cleanup_tables[0], fields[field],
                            removal_cleanup_input[field + 6u], 4);
    }
    removal_cleanup_set(lua, NULL, "objects", removal_cleanup_input[1], 1);
    const char *const globals[] = {"levelGoals", "deadBlocks"};
    const char *const tables[] = {"_ab_cleanup_goals", "_ab_cleanup_dead"};
    for (uint32_t map = 0; map < 2; ++map) {
        if (removal_cleanup_input[map + 4u]) {
            damage_lua.get_field(lua, -10002, tables[map]);
        } else {
            damage_lua.push_nil(lua);
        }
        damage_lua.set_field(lua, -10002, globals[map]);
    }
    return 0;
}

static void removal_cleanup_prepare(void *lua) {
    for (uint32_t word = 0; word < 17; ++word) {
        removal_particles_input[word] = 0;
    }
    removal_particles_input[1] = 2;
    removal_particles_input[10] = 15;
    removal_particles_count = 0;
    for (uint32_t kind = 0; kind < 5; ++kind) {
        removal_particles_calls[kind] = 0;
    }
    removal_particles_prepare(lua);
    damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
    damage_lua.set_field(lua, -10002, removal_cleanup_tables[0]);
    for (uint32_t table = 1; table < 5; ++table) {
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_number(lua, (float)(201u + table));
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.set_field(lua, -10002, removal_cleanup_tables[table]);
    }
    removal_cleanup_set(lua, removal_cleanup_tables[1], "world",
                        removal_cleanup_input[2], 2);
    removal_cleanup_set(lua, removal_cleanup_tables[1], "dynamic",
                        removal_cleanup_input[3], 3);
    uint32_t present = removal_cleanup_input[13];
    for (uint32_t map = 0; map < 2; ++map) {
        removal_cleanup_set(lua, removal_cleanup_tables[map + 2u], "queued",
                            present & (1u << map) ? 5u : 0u, 0);
    }
    const char *const maps[] = {"_ab_cleanup_goals", "_ab_cleanup_dead"};
    for (uint32_t map = 0; map < 2; ++map) {
        damage_lua.create_table(lua, 0, 1);
        if (present & (4u << map)) {
            damage_lua.push_boolean(lua, 1);
            damage_lua.set_field(lua, 1, "queued");
        }
        damage_lua.set_field(lua, -10002, maps[map]);
    }
    removal_cleanup_set(lua, NULL, "levelName", removal_cleanup_input[10], 4);
    if (removal_cleanup_input[11] & 1u) {
        damage_function(lua, -10002, "removePig", removal_cleanup_pig);
    } else {
        removal_particles_clear(lua, "removePig");
    }
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_number(lua, 501);
    damage_lua.set_field(lua, 1, "_id");
    if (removal_cleanup_input[11] & 2u) {
        damage_function(lua, 1, "collectGiftBox", removal_cleanup_gift);
    }
    damage_lua.set_field(lua, -10002, "GiftBoxManager");
    damage_function(lua, -10002, "removeObject", removal_cleanup_begin);
}

static void probe_block_removal_cleanup(void) {
    initialize_damage_lua();
    removal_particles_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    removal_particles_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original removal cleanup state\n");
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
        fail("Cannot read original removeBlocks for cleanup\n");
    }
    damage_function(lua, -10002, "_ab_particles_notification",
                    removal_particles_notify);
    semihost(4, "AB_ANDROID_BLOCK_REMOVAL_CLEANUP_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        block_removal_cleanup_case(sample, removal_cleanup_input);
        print_words(removal_cleanup_input, 15);
        removal_cleanup_count = removal_cleanup_started = 0;
        removal_cleanup_calls[0] = removal_cleanup_calls[1] = 0;
        removal_cleanup_prepare(lua);
        damage_lua.get_field(lua, -10002, "_ab_particles_removeBlocks");
        int status = damage_lua.call(lua, 0, 0, 0);
        const char *message = status ? damage_lua.to_string(lua, -1, NULL) : NULL;
        if (removal_cleanup_started != 1 ||
            (status && !removal_particles_contains(message, "index") &&
             !removal_particles_contains(message, "call"))) {
            fail(message ? message : "Original removal cleanup missed its entry\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t failed = status != 0;
        print_words(&failed, 1);
        uint32_t state[29];
        removal_cleanup_snapshot(lua, state);
        print_words(state, 29);
        print_words(&removal_cleanup_count, 1);
        for (uint32_t event = 0; event < removal_cleanup_count; ++event) {
            print_words(removal_cleanup_events[event], 49);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB block removal cleanup reference complete\n");
}

#endif
