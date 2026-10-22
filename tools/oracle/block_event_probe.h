#ifndef AB_ORACLE_BLOCK_EVENT_H
#define AB_ORACLE_BLOCK_EVENT_H

#include "selected_function.h"

static const char *const event_names[] = {"first", "second", "third"};
static const char *const event_definitions[] = {"def1", "def2", "def3"};
static uint32_t event_records[32][12], event_count, event_phase, event_world,
    event_version, event_handlers, event_mutation, event_arguments;
static const char *(*event_set_upvalue)(void *, int, int);
static const char *(*event_get_upvalue)(void *, int, int);
static int (*event_next)(void *, int);
#if defined(AB_ORACLE_PHYSICS_SCRIPT_ONLY)
static uint32_t event_selected;
#endif

static void event_call(void *lua, int arguments) {
    if (damage_lua.call(lua, arguments, 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original block event call failed\n");
    }
}

static uint32_t event_id(void *lua, int table) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, "id");
    if (damage_lua.type(lua, -1) != 3) {
        fail("Missing controlled original block identity\n");
    }
    uint32_t id = (uint32_t)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, top);
    return id;
}

static void event_membership(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 3);
    for (size_t index = 0; index < 3; ++index) {
        /* Alternate nil and false for missing world entries. */
        if (event_world & (1u << index)) {
            damage_lua.push_boolean(lua, 1);
        } else if (index & 1) {
            damage_lua.push_boolean(lua, 0);
        } else {
            damage_lua.push_nil(lua);
        }
        damage_lua.set_field(lua, top + 3, event_names[index]);
    }
    damage_lua.set_field(lua, top + 2, "world");
    damage_lua.set_field(lua, top + 1, "objects");
    damage_lua.set_top(lua, top);
}

static void event_catalog(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 3);
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.create_table(lua, 0, 1);
        FloatBits id = {.number = (float)(index + 1 + event_version * 3)};
        damage_number(lua, top + 4, "id", id.bits);
        damage_lua.set_field(lua, top + 3, event_definitions[index]);
    }
    damage_lua.set_field(lua, top + 2, "blocks");
    damage_lua.set_field(lua, top + 1, "blockTable");
    damage_lua.set_top(lua, top);
}

static int event_handler(void *lua);

static void event_handler_fields(void *lua) {
    int top = damage_lua.get_top(lua);
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, index ? "_ab_script2" : "_ab_script1");
        if (event_handlers & (1u << index)) {
            damage_lua.push_closure(lua, event_handler, 0);
        } else {
            damage_lua.push_boolean(lua, 0);
        }
        damage_lua.set_field(lua, top + 1, "evt");
        damage_lua.set_top(lua, top);
    }
}

static void event_mutate(void *lua) {
    event_world ^= event_mutation & 7;
    if (event_mutation & 8) {
        event_handlers ^= 2;
        event_handler_fields(lua);
    }
    if (event_mutation & 16) {
        event_version ^= 1;
        event_catalog(lua);
    }
#if defined(AB_ORACLE_PHYSICS_SCRIPT_ONLY)
    if (event_mutation & 32) {
        int top = damage_lua.get_top(lua);
        event_selected ^= 1;
        damage_lua.get_field(lua, -10002, "blocks");
        damage_lua.get_field(lua, top + 1, "events");
        damage_lua.push_string(lua, event_selected ? "skip" : "evt");
        damage_lua.set_field(lua, top + 2, "EID_UPDATE_BLOCK_PHYSICS_STEP");
        damage_lua.set_top(lua, top);
    }
#endif
    event_membership(lua);
}

static int event_handler(void *lua) {
    if (damage_lua.get_top(lua) != (int)event_arguments + 3 || event_count >= 32) {
        fail("Unexpected original block event arity\n");
    }
    uint32_t *record = event_records[event_count++];
    record[0] = event_phase;
    record[1] = event_id(lua, 2);
    record[2] = event_id(lua, 1);
    record[3] = event_id(lua, 3);
    record[4] = (uint32_t)damage_lua.get_top(lua);
    for (size_t index = 0; index < 6; ++index) {
        record[5 + index] = 0;
    }
    for (uint32_t index = 0; index < event_arguments; ++index) {
        int argument = (int)index + 4;
        record[5 + index * 2] = (uint32_t)damage_lua.type(lua, argument);
        if (index == 0) {
            FloatBits value = {.number = damage_lua.to_number(lua, argument)};
            record[6] = value.bits;
        } else if (index == 2) {
            if (!damage_named(lua, argument, "payload")) {
                fail("Unexpected original block event string argument\n");
            }
            record[10] = 1;
        }
    }
    record[11] = event_world;
    event_mutate(lua);
    return 0;
}

static void event_push_arguments(void *lua, uint32_t seconds) {
    if (event_arguments) {
        FloatBits value = {.bits = seconds};
        damage_lua.push_number(lua, value.number);
    }
    if (event_arguments >= 2) {
        damage_lua.push_nil(lua);
    }
    if (event_arguments >= 3) {
        damage_lua.push_string(lua, "payload");
    }
}

#if defined(AB_ORACLE_PHYSICS_SCRIPT_ONLY)
static int event_level_step(void *lua) {
    if (damage_lua.get_top(lua) != 1 || event_count >= 32) {
        fail("Unexpected original level physics hook arity\n");
    }
    uint32_t *record = event_records[event_count++];
    for (size_t index = 0; index < 12; ++index) {
        record[index] = 0;
    }
    FloatBits seconds = {.number = damage_lua.to_number(lua, 1)};
    record[0] = event_phase;
    record[4] = 1;
    record[5] = 3;
    record[6] = seconds.bits;
    record[11] = event_world;
    event_mutate(lua);
    return 0;
}

static void event_level_configuration(void *lua, uint32_t kind) {
    damage_lua.create_table(lua, 0, 1);
    if (kind >= 2) {
        damage_lua.create_table(lua, 0, 1);
        if (kind == 3) {
            damage_lua.push_closure(lua, event_level_step, 0);
        } else {
            damage_lua.push_boolean(lua, 0);
        }
        damage_lua.set_field(lua, 2, "onUpdatePhysicsStep");
    } else if (kind == 1) {
        damage_lua.push_boolean(lua, 0);
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, 1, "level");
    damage_lua.set_field(lua, -10002, "g_level_scripts");
    damage_lua.get_field(lua, -10002, "blocks");
    damage_lua.get_field(lua, 1, "events");
    damage_lua.push_string(lua, "evt");
    damage_lua.set_field(lua, 2, "EID_UPDATE_BLOCK_PHYSICS_STEP");
    damage_lua.set_top(lua, 0);
}
#endif

static void probe_block_event(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original block event state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    event_set_upvalue = (const char *(*)(void *, int, int))find_function(
        "_Z14lua_setupvalueP9lua_Stateii");
    event_get_upvalue = (const char *(*)(void *, int, int))find_function(
        "_Z14lua_getupvalueP9lua_Stateii");
    event_next = (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    load_selected_function(lua, "/data/local/tmp/ab520/block-event/object.lua",
                           "_ab_object_event");
    load_selected_function(lua, "/data/local/tmp/ab520/block-event/global.lua",
                           "_ab_global_event");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_object_event");
    damage_lua.set_field(lua, 1, "triggerEvent");
    damage_lua.set_field(lua, -10002, "BlockScriptManager");
#if defined(AB_ORACLE_PHYSICS_SCRIPT_ONLY)
    load_selected_function(lua, "/data/local/tmp/ab520/block-event/physics.lua",
                           "_ab_physics_step");
    damage_lua.get_field(lua, -10002, "BlockScriptManager");
    damage_lua.get_field(lua, -10002, "_ab_global_event");
    damage_lua.set_field(lua, 1, "triggerGlobalEvent");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 2);
    damage_lua.get_field(lua, -10002, "BlockScriptManager");
    damage_lua.set_field(lua, 1, "BlockScriptManager");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, 1, "events");
    damage_lua.set_field(lua, -10002, "blocks");
    damage_lua.push_string(lua, "level");
    damage_lua.set_field(lua, -10002, "levelName");
#endif
    damage_lua.create_table(lua, 0, 2);
    damage_lua.set_field(lua, -10002, "gamelua");
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.create_table(lua, 0, 2);
        FloatBits id = {.number = (float)index + 1};
        damage_number(lua, 1, "id", id.bits);
        damage_lua.set_field(lua, -10002, index ? "_ab_script2" : "_ab_script1");
    }
    static const uint8_t permutations[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                                               {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    uint32_t random = UINT32_C(0x520b10c3);
#if defined(AB_ORACLE_PHYSICS_SCRIPT_ONLY)
    semihost(4, "AB_ANDROID_PHYSICS_SCRIPT_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_BLOCK_EVENT_V1 4096\n");
#endif
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[] = {random_float(&random, 112, 30, 1),
                            (uint32_t)(item & 3),
                            (random_word(&random) >> 8) & 63,
                            (uint32_t)((item / 4) & 7),
                            (uint32_t)((item / 32) & 7),
                            (uint32_t)((item / 128) & 31),
                            (uint32_t)(item % 6),
                            (uint32_t)((item / 7) % 4),
                            0};
#if defined(AB_ORACLE_PHYSICS_SCRIPT_ONLY)
        input[5] = (uint32_t)((item / 64) & 63);
        input[7] = 1;
        input[8] = (uint32_t)((item / 11) % 4);
        event_selected = 0;
        event_level_configuration(lua, input[8]);
#endif
        event_world = input[4];
        event_handlers = input[1];
        event_mutation = input[5];
        event_arguments = input[7];
        event_version = event_count = 0;
        event_handler_fields(lua);
        event_catalog(lua);
        event_membership(lua);
        for (size_t index = 0; index < 3; ++index) {
            damage_lua.create_table(lua, 0, 3);
            FloatBits id = {.number = (float)index + 1};
            damage_number(lua, 1, "id", id.bits);
            damage_lua.push_string(lua, event_definitions[index]);
            damage_lua.set_field(lua, 1, "definition");
            if (input[3] & (1u << index)) {
                damage_lua.create_table(lua, 0, 0);
                unsigned slot = 1;
                for (size_t script = 0; script < 2; ++script) {
                    if (input[2] & (1u << (index * 2 + script))) {
                        damage_lua.get_field(lua, -10002,
                                             script ? "_ab_script2" : "_ab_script1");
                        void (*raw_set)(void *, int, int) =
                            (void (*)(void *, int, int))find_function(
                                "_Z11lua_rawsetiP9lua_Stateii");
                        raw_set(lua, 2, (int)slot++);
                    }
                }
                damage_lua.set_field(lua, 1, "blockScripts");
            }
            damage_lua.set_field(lua, -10002, event_names[index]);
        }
        damage_lua.get_field(lua, -10002, "_ab_global_event");
        damage_lua.create_table(lua, 0, 0);
        for (size_t index = 0; index < 3; ++index) {
            unsigned object = permutations[input[6]][index];
            damage_lua.get_field(lua, -10002, event_names[object]);
            damage_lua.set_field(lua, 2, event_names[object]);
        }
        if (!event_set_upvalue(lua, 1, 1)) {
            fail("Cannot bind original global block event table\n");
        }
        damage_lua.set_top(lua, 0);
        for (event_phase = 0; event_phase < 2; ++event_phase) {
#if defined(AB_ORACLE_PHYSICS_SCRIPT_ONLY)
            damage_lua.get_field(lua, -10002, "_ab_physics_step");
            event_push_arguments(lua, input[0]);
            event_call(lua, 1);
#else
            damage_lua.get_field(lua, -10002, "_ab_global_event");
            damage_lua.push_string(lua, "evt");
            event_push_arguments(lua, input[0]);
            event_call(lua, (int)event_arguments + 1);
#endif
            damage_lua.set_top(lua, 0);
        }
        event_phase = 2;
        for (size_t index = 0; index < 3; ++index) {
            damage_lua.get_field(lua, -10002, "_ab_object_event");
            damage_lua.get_field(lua, -10002, event_names[index]);
            damage_lua.push_string(lua, "evt");
            event_push_arguments(lua, input[0]);
            event_call(lua, (int)event_arguments + 2);
            damage_lua.set_top(lua, 0);
        }
        uint32_t tracked = 0;
        damage_lua.get_field(lua, -10002, "_ab_global_event");
        if (!event_get_upvalue(lua, 1, 1)) {
            fail("Cannot read original active block event table\n");
        }
        damage_lua.push_nil(lua);
        while (event_next(lua, 2)) {
            uint32_t id = event_id(lua, 4);
            tracked |= 1u << (id - 1);
            damage_lua.set_top(lua, 3);
        }
        damage_lua.set_top(lua, 0);
        uint32_t output[] = {event_world, event_version, event_handlers, tracked,
                             event_count};
#if defined(AB_ORACLE_PHYSICS_SCRIPT_ONLY)
        print_words(input, 9);
        print_words(output, 5);
        print_words(&event_selected, 1);
#else
        print_words(input, 8);
        print_words(output, 5);
#endif
        for (size_t index = 0; index < event_count; ++index) {
            print_words(event_records[index], 12);
        }
    }
    damage_lua.close_state(lua);
#if defined(AB_ORACLE_PHYSICS_SCRIPT_ONLY)
    semihost(4, "AB physics script reference complete\n");
#else
    semihost(4, "AB block event reference complete\n");
#endif
}

#endif
