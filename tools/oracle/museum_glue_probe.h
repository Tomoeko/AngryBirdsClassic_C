#ifndef AB_ORACLE_MUSEUM_GLUE_H
#define AB_ORACLE_MUSEUM_GLUE_H

#include "../../tests/museum_glue_cases.h"

static uint32_t museum_glue_input[6], museum_glue_events[3][4], museum_glue_count;
static const char *const museum_glue_names[] = {"first", "second", "plunger"};

static uint32_t museum_glue_name_id(void *lua, int index) {
    static const char *const names[] = {"first",   "second",          "plunger",
                                        "renamed", "renamed_plunger", "prior"};
    for (uint32_t name = 0; name < 6; ++name) {
        if (damage_named(lua, index, names[name])) {
            return name + 1;
        }
    }
    fail("Unexpected museum glue string\n");
    return 0;
}

static void museum_glue_event(void *lua, uint32_t operation) {
    if (museum_glue_count >= 3) {
        fail("Museum glue boundary event overflow\n");
    }
    uint32_t *event = museum_glue_events[museum_glue_count++];
    event[0] = operation;
    event[1] = museum_glue_name_id(lua, 1);
    event[2] = event[3] = 0;
}

static int museum_glue_definition(void *lua) {
    museum_glue_event(lua, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_number(lua, (float)museum_glue_input[2]);
    damage_lua.set_field(lua, -2, "collider");
    return 1;
}

static int museum_glue_object(void *lua) {
    museum_glue_event(lua, 2);
    static const uint32_t indices[] = {2, 0, 1};
    damage_lua.get_field(lua, -10002, museum_glue_names[indices[museum_glue_input[3]]]);
    return 1;
}

static int museum_glue_velocity(void *lua) {
    museum_glue_event(lua, 3);
    for (int argument = 2; argument <= 3; ++argument) {
        if (damage_lua.type(lua, argument) != 3) {
            fail("Museum glue velocity is not numeric\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        museum_glue_events[museum_glue_count - 1][argument] = value.bits;
    }
    if (museum_glue_input[5] & 1u) {
        damage_lua.get_field(lua, -10002, "second");
        damage_lua.push_string(lua, "renamed");
        damage_lua.set_field(lua, -2, "name");
        damage_lua.set_top(lua, 3);
    }
    if (museum_glue_input[5] & 2u) {
        static const uint32_t indices[] = {2, 0, 1};
        damage_lua.get_field(lua, -10002,
                             museum_glue_names[indices[museum_glue_input[3]]]);
        damage_lua.push_string(lua, "renamed_plunger");
        damage_lua.set_field(lua, -2, "name");
    }
    return 0;
}

static void museum_glue_value(void *lua, uint32_t kind) {
    switch (kind) {
        case 0:
            damage_lua.push_nil(lua);
            break;
        case 1:
            damage_lua.push_boolean(lua, 0);
            break;
        case 2:
            damage_lua.push_boolean(lua, 1);
            break;
        case 3:
            damage_lua.push_number(lua, 0);
            break;
        case 4:
            damage_lua.push_string(lua, "prior");
            break;
        default:
            damage_lua.create_table(lua, 0, 0);
            break;
    }
}

static void museum_glue_snapshot_value(void *lua, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = (uint32_t)(damage_lua.to_boolean(lua, -1) != 0);
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        output[1] = museum_glue_name_id(lua, -1);
    }
}

#if defined(AB_ORACLE_MUSEUM_GLUE_ROUTE)
#include "museum_glue_route_probe.h"
#endif

static void probe_museum_glue(void) {
#if defined(AB_ORACLE_MUSEUM_GLUE_ROUTE)
    probe_museum_glue_route();
    return;
#endif
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate museum glue oracle state\n");
    }
    int file = system_call(
        5, (int)(uintptr_t)"/data/local/tmp/ab520/collision/museum-glue.lua", NULL, 0);
    char chunk[508];
    if (file < 0 || system_call(3, file, chunk, sizeof(chunk)) != sizeof(chunk)) {
        fail("Cannot read extracted museum glue prototype\n");
    }
    system_call(6, file, NULL, 0);
    if (damage_lua.load(lua, chunk, sizeof(chunk), "museum-glue-reference")) {
        fail("Cannot load original museum glue prototype\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_glue_helper");
    semihost(4, "AB_ANDROID_MUSEUM_GLUE_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        museum_glue_case(sample, museum_glue_input);
        print_words(museum_glue_input, 6);
        museum_glue_count = 0;
        static const uint32_t plunger_indices[] = {2, 0, 1};
        for (uint32_t index = 0; index < 3; ++index) {
            damage_lua.create_table(lua, 0, 5);
            damage_lua.push_string(lua, museum_glue_names[index]);
            damage_lua.set_field(lua, -2, "name");
            if (!index) {
                damage_lua.push_string(
                    lua, museum_glue_names[plunger_indices[museum_glue_input[3]]]);
                damage_lua.set_field(lua, -2, "plungerName");
            }
            museum_glue_value(lua, index == 1 ? museum_glue_input[1] : 0);
            damage_lua.set_field(lua, -2, "grabber");
            museum_glue_value(lua, museum_glue_input[4]);
            damage_lua.set_field(lua, -2, "grabbedEgg");
            museum_glue_value(lua, (museum_glue_input[4] + index) % 4u);
            damage_lua.set_field(lua, -2, "gluedToBlock");
            damage_lua.set_field(lua, -10002, museum_glue_names[index]);
        }
        damage_lua.create_table(lua, 0, 4);
        damage_function(lua, -2, "getObjectDefinition", museum_glue_definition);
        damage_function(lua, -2, "getObject", museum_glue_object);
        damage_function(lua, -2, "setVelocity", museum_glue_velocity);
        damage_lua.create_table(lua, 0, 1);
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_number(lua, 3);
        damage_lua.set_field(lua, -2, "bird");
        damage_lua.push_number(lua, 6);
        damage_lua.set_field(lua, -2, "egg");
        damage_lua.set_field(lua, -2, "collider_types");
        damage_lua.set_field(lua, -2, "blockTable");
        damage_lua.set_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, -10002, "_ab_glue_helper");
        damage_lua.get_field(lua, -10002, "first");
        damage_lua.get_field(lua, -10002, "second");
        if (damage_lua.call(lua, 2, 2, 0)) {
            fail("Original museum glue helper failed\n");
        }
        uint32_t result[5] = {(uint32_t)damage_lua.type(lua, 1),
                              (uint32_t)(damage_lua.to_boolean(lua, 1) != 0),
                              (uint32_t)damage_lua.type(lua, 2), 0, 0};
        if (result[2] == 5) {
            void (*get_index)(void *, int, int) = (void (*)(
                void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
            get_index(lua, 2, 1);
            result[3] = museum_glue_name_id(lua, -1);
            damage_lua.set_top(lua, 2);
            get_index(lua, 2, 2);
            result[4] = museum_glue_name_id(lua, -1);
        }
        print_words(result, 5);
        damage_lua.set_top(lua, 0);
        for (uint32_t index = 0; index < 3; ++index) {
            damage_lua.get_field(lua, -10002, museum_glue_names[index]);
            damage_lua.get_field(lua, 1, "name");
            uint32_t values[7] = {museum_glue_name_id(lua, -1)};
            damage_lua.set_top(lua, 1);
            static const char *const fields[] = {"grabber", "grabbedEgg",
                                                 "gluedToBlock"};
            for (uint32_t field = 0; field < 3; ++field) {
                damage_lua.get_field(lua, 1, fields[field]);
                museum_glue_snapshot_value(lua, values + 1 + field * 2);
                damage_lua.set_top(lua, 1);
            }
            print_words(values, 7);
            damage_lua.set_top(lua, 0);
        }
        print_words(&museum_glue_count, 1);
        for (uint32_t event = 0; event < museum_glue_count; ++event) {
            print_words(museum_glue_events[event], 4);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB museum glue reference complete\n");
}

#endif
