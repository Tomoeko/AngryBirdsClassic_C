#ifndef AB_ORACLE_BLOCK_REMOVAL_SPAWN_H
#define AB_ORACLE_BLOCK_REMOVAL_SPAWN_H

#include "block_removal_particles_probe.h"
#include "../../tests/block_removal_spawn_cases.h"

static uint32_t removal_spawn_input[15], removal_spawn_events[20][8];
static uint32_t removal_spawn_count, removal_spawn_creates, removal_spawn_rotates,
    removal_spawn_randoms, removal_spawn_cutoff;
static int (*removal_spawn_metatable)(void *, int);

static int removal_spawn_rotate1(void *lua);
static int removal_spawn_rotate2(void *lua);

static uint32_t removal_spawn_definition_id(void *lua, int index) {
    if (damage_lua.type(lua, index) == 0) {
        return 0;
    }
    if (damage_lua.type(lua, index) == 4 && damage_named(lua, index, "target")) {
        return 1;
    }
    if (damage_lua.type(lua, index) == 4 && damage_named(lua, index, "alternate")) {
        return 2;
    }
    fail("Unexpected original removal spawn definition\n");
    return 0;
}

static uint32_t removal_spawn_number(void *lua, int index) {
    if (damage_lua.type(lua, index) != 3) {
        fail("Unexpected original removal spawn number\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, index)};
    return value.bits;
}

static uint32_t *removal_spawn_record(void *lua, uint32_t kind, uint32_t pc,
                                      uint32_t variant, int argc) {
    if (removal_particles_pc(lua) != pc || damage_lua.get_top(lua) != argc ||
        removal_spawn_count >= 20) {
        fail("Unexpected original removal spawn boundary\n");
    }
    uint32_t *event = removal_spawn_events[removal_spawn_count++];
    for (uint32_t word = 0; word < 8; ++word) {
        event[word] = 0;
    }
    event[0] = kind;
    event[1] = pc;
    event[2] = variant;
    return event;
}

static void removal_spawn_global_number(void *lua, const char *field, uint32_t word) {
    FloatBits value = {.bits = word};
    damage_lua.push_number(lua, value.number);
    damage_lua.set_field(lua, -10002, field);
}

static void removal_spawn_set_number(void *lua, const char *table, const char *field,
                                     uint32_t word) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, table);
    damage_number(lua, top + 1, field, word);
    damage_lua.set_top(lua, top);
}

static int removal_spawn_random(void *lua) {
    uint32_t pc = removal_particles_pc(lua);
    if (pc == 182) {
        if (damage_lua.get_top(lua) != 2 || damage_lua.to_number(lua, 1) != 12 ||
            damage_lua.to_number(lua, 2) != 20) {
            fail("Unexpected original pre-spawn random\n");
        }
        damage_lua.push_number(lua, 17);
        return 1;
    }
    int choice = pc == 424;
    uint32_t *event =
        removal_spawn_record(lua, 3, choice ? 424u : 443u, 0, choice ? 2 : 0);
    event[3] = choice ? 2u : 0u;
    if (choice) {
        event[4] = removal_spawn_number(lua, 1);
        event[5] = removal_spawn_number(lua, 2);
        if (damage_lua.to_number(lua, 1) != 1 ||
            damage_lua.to_number(lua, 2) != (float)removal_spawn_input[14]) {
            fail("Unexpected original spawn choice bounds\n");
        }
    }
    ++removal_spawn_randoms;
    uint32_t flags = removal_spawn_input[1];
    if (choice && (flags & 8u)) {
        damage_lua.get_field(lua, -10002, "_ab_spawn_death");
        damage_lua.get_field(lua, -10002, "_ab_spawn_types2");
        damage_lua.set_field(lua, 3, "types");
        damage_lua.set_top(lua, 2);
    }
    if (!choice) {
        if (flags & 8u) {
            damage_function(lua, -10002, "setRotation", removal_spawn_rotate2);
            removal_spawn_global_number(lua, "scaleFactor", 0x3f000000);
            removal_spawn_set_number(lua, "math", "pi", 0x3f800000);
        }
        if (flags & 64u) {
            removal_particles_clear(lua, "createObject");
        }
    }
    FloatBits output = {.bits = removal_spawn_input[choice ? 10 : 11]};
    damage_lua.push_number(lua, output.number);
    return 1;
}

static int removal_spawn_create(void *lua) {
    uint32_t pc = removal_particles_pc(lua);
    if (pc != 400 && pc != 437) {
        fail("Unexpected original createObject instruction\n");
    }
    uint32_t *event = removal_spawn_record(lua, 1, pc, 0, 5);
    damage_lua.get_field(lua, -10002, "_ab_spawn_blocks");
    if (damage_lua.type(lua, 1) != 5 ||
        damage_lua.to_pointer(lua, 1) != damage_lua.to_pointer(lua, 6)) {
        fail("Original spawn createObject lost current block table\n");
    }
    damage_lua.set_top(lua, 5);
    event[3] = removal_spawn_definition_id(lua, 2);
    unsigned length = 0;
    if (damage_lua.type(lua, 3) != 4) {
        fail("Original spawn generated name is not a string\n");
    }
    const char *name = damage_lua.to_string(lua, 3, &length);
    event[4] = UINT32_C(2166136261);
    event[5] = length;
    for (unsigned index = 0; index < length; ++index) {
        event[4] = (event[4] ^ (uint8_t)name[index]) * UINT32_C(16777619);
    }
    event[6] = removal_spawn_number(lua, 4);
    event[7] = removal_spawn_number(lua, 5);
    ++removal_spawn_creates;
    uint32_t flags = removal_spawn_input[1];
    if ((flags & 4u) && removal_spawn_creates == 1) {
        removal_particles_set(lua, removal_particles_tables[0], "name", 4, "renamed",
                              3);
        removal_spawn_set_number(lua, removal_particles_tables[0], "width", 0x41b00000);
        removal_spawn_set_number(lua, removal_particles_tables[0], "angle", 0x3fc90fdb);
        removal_spawn_set_number(lua, removal_particles_tables[0], "x", 0x41f00000);
        removal_spawn_set_number(lua, removal_particles_tables[0], "y", 0x42200000);
        removal_particles_set(lua, "_ab_spawn_grid", "definition", 4, "alternate", 3);
    }
    if (flags & 16u) {
        removal_particles_clear(lua, "setRotation");
    }
    if (flags & 32u) {
        removal_particles_set(lua, "math", "random", 0, "", 3);
    }
    damage_lua.push_number(lua, (float)(removal_spawn_creates - 1u) + 100);
    return 1;
}

static int removal_spawn_rotate(void *lua, uint32_t variant) {
    uint32_t pc = removal_particles_pc(lua);
    if (pc != 409 && pc != 449) {
        fail("Unexpected original spawn setRotation instruction\n");
    }
    uint32_t *event = removal_spawn_record(lua, 2, pc, variant, 2);
    event[3] = removal_spawn_number(lua, 1);
    event[4] = removal_spawn_number(lua, 2);
    ++removal_spawn_rotates;
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int removal_spawn_rotate1(void *lua) {
    return removal_spawn_rotate(lua, 1);
}

static int removal_spawn_rotate2(void *lua) {
    return removal_spawn_rotate(lua, 2);
}

static int removal_spawn_begin(void *lua) {
    if (removal_particles_pc(lua) != 288 || damage_lua.get_top(lua) != 1 ||
        !damage_named(lua, 1, "queued")) {
        fail("Unexpected original spawn entry boundary\n");
    }
    removal_spawn_set_number(lua, removal_particles_tables[0], "x",
                             removal_spawn_input[6]);
    removal_spawn_set_number(lua, removal_particles_tables[0], "y",
                             removal_spawn_input[7]);
    return 0;
}

static int removal_spawn_end(void *lua) {
    if (removal_particles_pc(lua) != 452 || damage_lua.get_top(lua) != 3 ||
        damage_lua.type(lua, 1) != 5 || !damage_named(lua, 2, "queued") ||
        damage_lua.type(lua, 3) != 0 || removal_spawn_cutoff++) {
        fail("Unexpected original post-spawn map-write cutoff\n");
    }
    removal_particles_clear(lua, "objects");
    return 0;
}

static void removal_spawn_snapshot(void *lua, uint32_t output[16]) {
    damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
    damage_lua.get_field(lua, 1, "name");
    output[0] = damage_named(lua, 2, "body") ? 1u : 2u;
    damage_lua.set_top(lua, 1);
    const char *const fields[] = {"width", "height", "angle", "x", "y"};
    for (uint32_t field = 0; field < 5; ++field) {
        damage_lua.get_field(lua, 1, fields[field]);
        output[field + 1u] = removal_spawn_number(lua, 2);
        damage_lua.set_top(lua, 1);
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_spawn_grid");
    damage_lua.get_field(lua, 1, "definition");
    output[6] = removal_spawn_definition_id(lua, 2);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_spawn_death");
    damage_lua.get_field(lua, 1, "types");
    const void *current_types = damage_lua.to_pointer(lua, 2);
    damage_lua.get_field(lua, -10002, "_ab_spawn_types1");
    output[7] = damage_lua.to_pointer(lua, 3) == current_types ? 1u : 2u;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "scaleFactor");
    output[8] = removal_spawn_number(lua, 1);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "math");
    damage_lua.get_field(lua, 1, "pi");
    output[9] = removal_spawn_number(lua, 2);
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "random");
    output[12] = damage_lua.type(lua, 2) == 6;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "createObject");
    output[10] = damage_lua.type(lua, 1) == 6;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "setRotation");
    output[11] = damage_lua.type(lua, 1) == 6
                     ? removal_particles_function_id(lua, 1, removal_spawn_rotate1,
                                                     removal_spawn_rotate2, NULL)
                     : 0;
    damage_lua.set_top(lua, 0);
    output[13] = removal_spawn_creates;
    output[14] = removal_spawn_rotates;
    output[15] = removal_spawn_randoms;
}

static void removal_spawn_prepare(void *lua) {
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
    damage_lua.get_field(lua, -10002, "_ab_spawn_math");
    damage_lua.set_field(lua, -10002, "math");
    damage_function(lua, -10002, "createObject", removal_spawn_create);
    damage_function(lua, -10002, "setRotation", removal_spawn_rotate1);
    damage_function(lua, -10002, "removeObject", removal_spawn_begin);
    damage_lua.get_field(lua, -10002, "math");
    damage_function(lua, 1, "random", removal_spawn_random);
    damage_number(lua, 1, "pi", removal_spawn_input[13]);
    damage_lua.set_top(lua, 0);
    removal_spawn_global_number(lua, "scaleFactor", removal_spawn_input[12]);
    const char *const fields[] = {"width", "height", "angle"};
    const uint32_t inputs[] = {2, 3, 5};
    for (uint32_t field = 0; field < 3; ++field) {
        removal_spawn_set_number(lua, removal_particles_tables[0], fields[field],
                                 removal_spawn_input[inputs[field]]);
    }
    removal_spawn_set_number(lua, removal_particles_tables[0], "x", 0x40e00000);
    removal_spawn_set_number(lua, removal_particles_tables[0], "y", 0xc0400000);
    const char *const types[] = {"_ab_spawn_types1", "_ab_spawn_types2"};
    for (uint32_t type = 0; type < 2; ++type) {
        damage_lua.create_table(lua, (int)removal_spawn_input[14], 0);
        for (uint32_t slot = 0; slot < removal_spawn_input[14]; ++slot) {
            damage_lua.push_string(lua, (type + slot) % 2u ? "alternate" : "target");
            removal_particles_raw_set(lua, 1, (int)slot + 1);
        }
        damage_lua.set_field(lua, -10002, types[type]);
    }
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_string(lua, "target");
    damage_lua.set_field(lua, 1, "definition");
    damage_number(lua, 1, "amount", removal_spawn_input[8]);
    damage_lua.set_field(lua, -10002, "_ab_spawn_grid");
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 1, "amount", removal_spawn_input[9]);
    damage_lua.get_field(lua, -10002, types[0]);
    damage_lua.set_field(lua, 1, "types");
    damage_lua.set_field(lua, -10002, "_ab_spawn_death");
    damage_lua.get_field(lua, -10002, "blockTable");
    damage_lua.get_field(lua, 1, "blocks");
    damage_lua.create_table(lua, 0, 1);
    damage_number(lua, 3, "width", removal_spawn_input[4]);
    damage_lua.set_field(lua, 2, "target");
    damage_lua.get_field(lua, 2, "target");
    damage_lua.set_field(lua, 2, "alternate");
    damage_lua.get_field(lua, 2, "raw");
    uint32_t flags = removal_spawn_input[1];
    const char *const names[] = {"spawn", "deathSpawn"};
    const char *const descriptors[] = {"_ab_spawn_grid", "_ab_spawn_death"};
    for (uint32_t descriptor = 0; descriptor < 2; ++descriptor) {
        if (flags & (256u << descriptor)) {
            damage_lua.push_boolean(lua, 0);
        } else if (flags & (1u << descriptor)) {
            damage_lua.get_field(lua, -10002, descriptors[descriptor]);
        } else {
            damage_lua.push_nil(lua);
        }
        damage_lua.set_field(lua, 3, names[descriptor]);
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "blockTable");
    damage_lua.set_field(lua, -10002, "_ab_spawn_blocks");
    if (flags & 128u) {
        removal_particles_clear(lua, "createObject");
    }
    damage_lua.create_table(lua, 0, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "__newindex", removal_spawn_end);
    (void)removal_spawn_metatable(lua, 1);
    damage_lua.set_field(lua, -10002, "levelGoals");
}

static void probe_block_removal_spawn(void) {
    initialize_damage_lua();
    removal_particles_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    removal_particles_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    removal_spawn_metatable =
        (int (*)(void *, int))find_function("_Z16lua_setmetatableP9lua_Statei");
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original removal spawn state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "math");
    damage_lua.set_field(lua, -10002, "_ab_spawn_math");
    int file = system_call(
        5, (int)(uintptr_t)"/data/local/tmp/ab520/collision/remove-blocks.lua", NULL,
        0);
    if (file < 0 ||
        system_call(3, file, removal_particles_chunk,
                    sizeof(removal_particles_chunk)) !=
            sizeof(removal_particles_chunk) ||
        system_call(6, file, NULL, 0) != 0) {
        fail("Cannot read original removeBlocks for spawns\n");
    }
    damage_function(lua, -10002, "_ab_particles_notification",
                    removal_particles_notify);
    semihost(4, "AB_ANDROID_BLOCK_REMOVAL_SPAWN_V1 2048\n");
    for (uint32_t sample = 0; sample < 2048; ++sample) {
        block_removal_spawn_case(sample, removal_spawn_input);
        print_words(removal_spawn_input, 15);
        removal_spawn_count = removal_spawn_creates = removal_spawn_rotates =
            removal_spawn_randoms = removal_spawn_cutoff = 0;
        removal_spawn_prepare(lua);
        damage_lua.get_field(lua, -10002, "_ab_particles_removeBlocks");
        int status = damage_lua.call(lua, 0, 0, 0);
        const char *message = status ? damage_lua.to_string(lua, -1, NULL) : NULL;
        if (!status ||
            (removal_spawn_cutoff && (removal_spawn_cutoff != 1 ||
                                      !removal_particles_contains(message, "index"))) ||
            (!removal_spawn_cutoff && !removal_particles_contains(message, "index") &&
             !removal_particles_contains(message, "call"))) {
            fail(message ? message : "Original removal spawn missed cutoff\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t failed = removal_spawn_cutoff == 0;
        print_words(&failed, 1);
        uint32_t state[16];
        removal_spawn_snapshot(lua, state);
        print_words(state, 16);
        print_words(&removal_spawn_count, 1);
        for (uint32_t event = 0; event < removal_spawn_count; ++event) {
            print_words(removal_spawn_events[event], 8);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB block removal spawn reference complete\n");
}

#endif
