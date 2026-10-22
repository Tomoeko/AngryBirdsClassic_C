#ifndef AB_ORACLE_BLOCK_REMOVAL_DETACH_H
#define AB_ORACLE_BLOCK_REMOVAL_DETACH_H

/* Reuse the independently checked inert prefix fixture and original frame
 * guard, without copying or modifying any original instruction. */
#include "block_removal_particles_probe.h"
#include "../../tests/block_removal_detach_cases.h"

static uint32_t removal_detach_input[11], removal_detach_result[8];
static uint32_t removal_detach_event[36], removal_detach_state[20],
    removal_detach_count;
static uintptr_t removal_detach_string_ids[7], removal_detach_table_ids[5];
static const char *const removal_detach_strings[] = {
    "body", "renamed", "raw", "spawn", "death", "truthy", "changed"};
static const char *const removal_detach_tables[] = {
    "_ab_detach_object", "_ab_detach_raw", "_ab_detach_catalog", "_ab_detach_blocks",
    "_ab_detach_value"};

static void removal_detach_scalar(void *lua, uint32_t kind, const char *string,
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
        damage_lua.get_field(lua, -10002, removal_detach_tables[table]);
    }
}

static void removal_detach_value(void *lua, int index, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, index);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, index) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t string = 0; string < 7; ++string) {
            if (damage_named(lua, index, removal_detach_strings[string])) {
                output[1] = string + 1u;
                return;
            }
        }
        fail("Unexpected original removal detach string\n");
    } else if (output[0] == 5) {
        const void *table = damage_lua.to_pointer(lua, index);
        for (uint32_t identity = 0; identity < 5; ++identity) {
            if (removal_detach_table_ids[identity] == (uintptr_t)table) {
                output[1] = identity + 201u;
                return;
            }
        }
        fail("Unexpected original removal detach table\n");
    } else if (output[0]) {
        fail("Unexpected original removal detach value\n");
    }
}

static void removal_detach_register(const uint32_t value[2], uint32_t output[2]) {
    output[0] = value[1];
    output[1] = 0;
    if (value[1] == 1 || value[1] == 3) {
        output[1] = value[0];
    } else if (value[1] == 4 || value[1] == 5) {
        const uintptr_t *identities =
            value[1] == 4 ? removal_detach_string_ids : removal_detach_table_ids;
        uint32_t count = value[1] == 4 ? 7u : 5u;
        for (uint32_t index = 0; index < count; ++index) {
            if (identities[index] == value[0]) {
                output[1] = index + (value[1] == 4 ? 1u : 201u);
                return;
            }
        }
        fail("Unexpected original removal detach register identity\n");
    } else if (value[1]) {
        fail("Unexpected original removal detach register tag\n");
    }
}

static void removal_detach_set(void *lua, const char *table, const char *field,
                               uint32_t kind, const char *string,
                               uint32_t value_table) {
    int top = damage_lua.get_top(lua);
    if (table) {
        damage_lua.get_field(lua, -10002, table);
    }
    removal_detach_scalar(lua, kind, string, value_table);
    damage_lua.set_field(lua, table ? top + 1 : -10002, field);
    damage_lua.set_top(lua, top);
}

static void removal_detach_snapshot(void *lua, uint32_t output[20]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, removal_detach_tables[0]);
    const char *const fields[] = {"name", "definition", "x", "y"};
    for (uint32_t field = 0; field < 4; ++field) {
        damage_lua.get_field(lua, top + 1, fields[field]);
        removal_detach_value(lua, top + 2, output + field * 2u);
        damage_lua.set_top(lua, top + 1);
    }
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, removal_detach_tables[1]);
    damage_lua.get_field(lua, top + 1, "spawn");
    removal_detach_value(lua, top + 2, output + 8);
    damage_lua.set_top(lua, top + 1);
    damage_lua.get_field(lua, top + 1, "deathSpawn");
    removal_detach_value(lua, top + 2, output + 10);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "blockTable");
    removal_detach_value(lua, top + 1, output + 12);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, removal_detach_tables[3]);
    damage_lua.get_field(lua, top + 1, "blocks");
    removal_detach_value(lua, top + 2, output + 14);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, removal_detach_tables[2]);
    damage_lua.get_field(lua, top + 1, "raw");
    removal_detach_value(lua, top + 2, output + 16);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "removeObject");
    output[18] = damage_lua.type(lua, -1) == 6;
    output[19] = removal_detach_count;
    damage_lua.set_top(lua, top);
}

static int removal_detach_remove(void *lua) {
    if (removal_particles_pc(lua) != 288 || damage_lua.get_top(lua) != 1 ||
        !damage_named(lua, 1, "queued") || removal_detach_count) {
        fail("Unexpected original detach removeObject boundary\n");
    }
    uint32_t *event = removal_detach_event;
    event[0] = 1;
    event[1] = 288;
    event[2] = 0;
    event[3] = 1;
    event[4] = 1;
    const uint32_t *state = lua;
    const uint32_t *caller = (const uint32_t *)(uintptr_t)(state[5] - 24u);
    const uint32_t *registers = (const uint32_t *)(uintptr_t)caller[0];
    if (!registers || registers + 32 > (const uint32_t *)(uintptr_t)state[3]) {
        fail("Unexpected original detach cached-register bounds\n");
    }
    for (uint32_t slot = 0; slot < 4; ++slot) {
        removal_detach_register(registers + (12u + slot) * 2u,
                                removal_detach_result + slot * 2u);
        event[5 + slot * 2u] = removal_detach_result[slot * 2u];
        event[6 + slot * 2u] = removal_detach_result[slot * 2u + 1u];
    }
    removal_detach_snapshot(lua, event + 16);
    ++removal_detach_count;
    uint32_t mutation = removal_detach_input[10];
    if (mutation & 1u) {
        removal_detach_set(lua, removal_detach_tables[0], "name", 4, "renamed", 4);
    }
    if (mutation & 2u) {
        removal_detach_set(lua, removal_detach_tables[0], "x", 2, "", 4);
        removal_detach_set(lua, removal_detach_tables[0], "y", 1, "", 4);
    }
    if (mutation & 4u) {
        removal_detach_set(lua, removal_detach_tables[1], "spawn", 4, "changed", 4);
        removal_detach_set(lua, removal_detach_tables[1], "deathSpawn", 0, "", 4);
    }
    if (mutation & 8u) {
        removal_particles_clear(lua, "blockTable");
    }
    if (mutation & 16u) {
        removal_particles_clear(lua, "removeObject");
    }
    removal_detach_snapshot(lua, removal_detach_state);
    /* These are post-phase cutoff inputs, excluded from the captured state.
     * Non-nil spawn immediately indexes blockTable; otherwise nil levelGoals
     * stops after an inert zero-iteration/false deathSpawn continuation. */
    removal_particles_clear(lua, "blockTable");
    removal_particles_clear(lua, "levelGoals");
    return 0;
}

static void removal_detach_prepare(void *lua) {
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
    removal_detach_table_ids[0] = (uintptr_t)damage_lua.to_pointer(lua, 1);
    damage_lua.set_field(lua, -10002, removal_detach_tables[0]);
    for (uint32_t table = 1; table < 5; ++table) {
        damage_lua.create_table(lua, 0, 2);
        removal_detach_table_ids[table] = (uintptr_t)damage_lua.to_pointer(lua, 1);
        damage_lua.set_field(lua, -10002, removal_detach_tables[table]);
    }
    removal_detach_set(lua, removal_detach_tables[0], "definition",
                       removal_detach_input[3], "raw", 4);
    removal_detach_set(lua, removal_detach_tables[0], "x", removal_detach_input[7],
                       "truthy", 4);
    removal_detach_set(lua, removal_detach_tables[0], "y", removal_detach_input[8],
                       "truthy", 4);
    removal_detach_set(lua, removal_detach_tables[1], "spawn", removal_detach_input[5],
                       "spawn", 4);
    removal_detach_set(lua, removal_detach_tables[1], "deathSpawn",
                       removal_detach_input[6], "death", 4);
    removal_detach_set(lua, removal_detach_tables[2], "raw", removal_detach_input[4],
                       "truthy", 1);
    removal_detach_set(lua, removal_detach_tables[3], "blocks", removal_detach_input[2],
                       "truthy", 2);
    removal_detach_set(lua, NULL, "blockTable", removal_detach_input[1], "truthy", 3);
    if (removal_detach_input[9]) {
        damage_function(lua, -10002, "removeObject", removal_detach_remove);
    } else {
        removal_particles_clear(lua, "removeObject");
    }
}

static void probe_block_removal_detach(void) {
    initialize_damage_lua();
    removal_particles_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    removal_particles_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original removal detach state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 7);
    for (uint32_t string = 0; string < 7; ++string) {
        damage_lua.push_string(lua, removal_detach_strings[string]);
        const uint32_t *state = lua;
        const uint32_t *top = (const uint32_t *)(uintptr_t)state[2];
        if (top[-1] != 4 || !damage_named(lua, 2, removal_detach_strings[string])) {
            fail("Unexpected original detach pinned string TValue\n");
        }
        removal_detach_string_ids[string] = top[-2];
        damage_lua.set_field(lua, 1, removal_detach_strings[string]);
    }
    damage_lua.set_field(lua, -10002, "_ab_detach_pinned_strings");
    int file = system_call(
        5, (int)(uintptr_t)"/data/local/tmp/ab520/collision/remove-blocks.lua", NULL,
        0);
    if (file < 0 ||
        system_call(3, file, removal_particles_chunk,
                    sizeof(removal_particles_chunk)) !=
            sizeof(removal_particles_chunk) ||
        system_call(6, file, NULL, 0) != 0) {
        fail("Cannot read original removeBlocks for detach\n");
    }
    damage_function(lua, -10002, "_ab_particles_notification",
                    removal_particles_notify);
    semihost(4, "AB_ANDROID_BLOCK_REMOVAL_DETACH_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        block_removal_detach_case(sample, removal_detach_input);
        print_words(removal_detach_input, 11);
        removal_detach_count = 0;
        for (uint32_t word = 0; word < 36; ++word) {
            removal_detach_event[word] = 0;
        }
        removal_detach_prepare(lua);
        damage_lua.get_field(lua, -10002, "_ab_particles_removeBlocks");
        int status = damage_lua.call(lua, 0, 0, 0);
        const char *message = status ? damage_lua.to_string(lua, -1, NULL) : NULL;
        if (!status ||
            (!removal_particles_contains(message, "index") &&
             !removal_particles_contains(message, "call") &&
             !(removal_detach_count && removal_particles_contains(message, "for")))) {
            fail(message ? message : "Original detach missed post-phase cutoff\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t result[9] = {removal_detach_count == 0, 0, 0, 0, 0, 0, 0, 0, 0};
        if (removal_detach_count) {
            for (uint32_t word = 0; word < 8; ++word) {
                result[word + 1u] = removal_detach_result[word];
            }
        } else {
            removal_detach_snapshot(lua, removal_detach_state);
        }
        print_words(result, 9);
        print_words(removal_detach_state, 20);
        print_words(&removal_detach_count, 1);
        if (removal_detach_count) {
            print_words(removal_detach_event, 36);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB block removal detach reference complete\n");
}

#endif
