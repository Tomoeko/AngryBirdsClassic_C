#ifndef AB_ORACLE_OBJECT_PROPERTY_ACCESS_H
#define AB_ORACLE_OBJECT_PROPERTY_ACCESS_H

#include "../../tests/object_property_access_cases.h"

static uint32_t property_input[15], property_events[8][7], property_calls;
static const char *const property_globals[] = {
    "_ab_physical",      "_ab_definition",    "_ab_material_old",    "_ab_material_new",
    "_ab_materials_old", "_ab_materials_new", "_ab_property_payload"};
static const uint32_t property_ids[] = {201, 202, 203, 204, 301, 302, 101};
static const char *const property_strings[] = {"value", "changed", "first", "second",
                                               "wood",  "bad",     "new"};

static const char *property_key(void) {
    return property_input[13] ? "target" : "newTarget";
}

static void property_value(void *lua, int index, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, index);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, index) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t string = 0; string < 7; ++string) {
            if (damage_named(lua, index, property_strings[string])) {
                output[1] = string + 1u;
                return;
            }
        }
        fail("Original property access returned an unknown string\n");
    } else if (output[0] == 5) {
        int top = damage_lua.get_top(lua);
        damage_lua.get_field(lua, index, "_id");
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, top);
    } else if (output[0] != 0) {
        fail("Unexpected original property value type\n");
    }
}

static void property_scalar(void *lua, uint32_t kind) {
    switch (kind) {
        case 0:
            damage_lua.push_nil(lua);
            break;
        case 1:
        case 2:
            damage_lua.push_boolean(lua, kind == 2);
            break;
        case 3: {
            FloatBits value = {.bits = property_input[11]};
            damage_lua.push_number(lua, value.number);
            break;
        }
        case 4:
            damage_lua.push_string(lua, "value");
            break;
        default:
            damage_lua.get_field(lua, -10002, property_globals[6]);
            break;
    }
}

static void property_record(void *lua, uint32_t kind, size_t table) {
    if (kind == 2) {
        damage_lua.get_field(lua, -10002, property_globals[table]);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0.0f);
    } else if (kind == 4) {
        damage_lua.push_boolean(lua, 1);
    } else if (kind == 5) {
        damage_lua.push_string(lua, "bad");
    } else {
        property_scalar(lua, kind);
    }
}

static int property_definition(void *lua) {
    if (damage_lua.get_top(lua) != 1 || property_calls >= 8) {
        fail("Original property definition query overflow/arity mismatch\n");
    }
    uint32_t *event = property_events[property_calls];
    event[0] = ++property_calls;
    property_value(lua, 1, event + 1);
    damage_lua.get_field(lua, -10002, "_ab_property_object");
    damage_lua.get_field(lua, 2, "physicsObject");
    property_value(lua, 3, event + 3);
    damage_lua.set_top(lua, 2);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, 3, "blockTable");
    damage_lua.get_field(lua, 4, "materials");
    property_value(lua, 5, event + 5);
    damage_lua.set_top(lua, 2);
    uint32_t phase = (property_calls - 1u) % 3u;
    if (phase == 0) {
        if (property_input[10] & 1u) {
            damage_lua.get_field(lua, -10002, property_globals[0]);
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 3, property_key());
            damage_lua.set_top(lua, 2);
        }
        if (property_input[10] & 2u) {
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, 2, "physicsObject");
        }
    } else if (phase == 1 && (property_input[10] & 4u)) {
        damage_lua.push_string(lua, "second");
        damage_lua.set_field(lua, 2, "name");
    } else if (phase == 2) {
        if (property_input[10] & 8u) {
            damage_lua.get_field(lua, -10002, "gamelua");
            damage_lua.get_field(lua, 3, "blockTable");
            damage_lua.get_field(lua, -10002, property_globals[5]);
            damage_lua.set_field(lua, 4, "materials");
            damage_lua.set_top(lua, 2);
        }
        if (property_input[10] & 16u) {
            damage_lua.get_field(lua, -10002, property_globals[2]);
            damage_lua.push_string(lua, "changed");
            damage_lua.set_field(lua, 3, property_key());
            damage_lua.set_top(lua, 2);
        }
    }
    property_record(lua, property_input[4 + phase], 1);
    return 1;
}

static void property_prepare(void *lua) {
    for (size_t index = 0; index < 7; ++index) {
        damage_lua.create_table(lua, 0, 3);
        damage_lua.push_number(lua, (float)property_ids[index]);
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.set_field(lua, -10002, property_globals[index]);
    }
    for (size_t index = 0; index < 4; ++index) {
        damage_lua.get_field(lua, -10002, property_globals[index]);
        if (index == 3) {
            damage_lua.push_string(lua, "new");
        } else {
            property_scalar(lua, property_input[index == 0 ? 3 : index == 1 ? 7 : 9]);
        }
        damage_lua.set_field(
            lua, 1, index == 0 && !property_input[13] ? "target" : property_key());
        if (index == 1) {
            damage_lua.push_string(lua, "wood");
            damage_lua.set_field(lua, 1, "materialName");
        }
        damage_lua.set_top(lua, 0);
    }
    damage_lua.get_field(lua, -10002, property_globals[4]);
    property_record(lua, property_input[8], 2);
    damage_lua.set_field(lua, 1, "wood");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, property_globals[5]);
    damage_lua.get_field(lua, -10002, property_globals[3]);
    damage_lua.set_field(lua, 1, "wood");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, 1, "name");
    property_record(lua, property_input[2], 0);
    damage_lua.set_field(lua, 1, "physicsObject");
    damage_lua.set_field(lua, -10002, "_ab_property_object");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "getObjectDefinition", property_definition);
    damage_lua.create_table(lua, 0, 1);
    if (property_input[10] & 32u) {
        damage_lua.push_nil(lua);
    } else {
        damage_lua.get_field(lua, -10002, property_globals[4]);
    }
    damage_lua.set_field(lua, 2, "materials");
    damage_lua.set_field(lua, 1, "blockTable");
    damage_lua.set_field(lua, -10002, "gamelua");
}

static void property_call(void *lua, int setter, uint32_t output[4]) {
    damage_lua.get_field(lua, -10002, "ObjectFunctions");
    damage_lua.get_field(lua, 1, setter ? "setObjectProperty" : "getObjectProperty");
    if (property_input[1]) {
        damage_lua.get_field(lua, -10002, "_ab_property_object");
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.push_string(lua, property_key());
    if (setter) {
        property_scalar(lua, property_input[12]);
    }
    int status = damage_lua.call(lua, setter ? 3 : 2, setter ? 0 : 1, 0);
    output[0] = 1;
    output[1] = status != 0;
    output[2] = output[3] = 0;
    if (!status && !setter) {
        property_value(lua, -1, output + 2);
    }
    if (status) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        const char *wanted = "attempt to index";
        int found = 0;
        for (size_t index = 0; message && message[index]; ++index) {
            size_t matched = 0;
            while (wanted[matched] && message[index + matched] &&
                   wanted[matched] == message[index + matched]) {
                ++matched;
            }
            if (!wanted[matched]) {
                found = 1;
                break;
            }
        }
        if (!found) {
            fail(message ? message : "Unexpected original property failure\n");
        }
    }
    damage_lua.set_top(lua, 0);
}

static void property_snapshot(void *lua) {
    uint32_t output[15];
    damage_lua.get_field(lua, -10002, "_ab_property_object");
    damage_lua.get_field(lua, 1, "name");
    property_value(lua, 2, output);
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "physicsObject");
    property_value(lua, 2, output + 2);
    damage_lua.set_top(lua, 0);
    for (size_t index = 0; index < 4; ++index) {
        damage_lua.get_field(lua, -10002, property_globals[index]);
        damage_lua.get_field(lua, 1, property_key());
        property_value(lua, 2, output + 4 + index * 2);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, 1, "blockTable");
    damage_lua.get_field(lua, 2, "materials");
    property_value(lua, 3, output + 12);
    damage_lua.set_top(lua, 0);
    output[14] = property_calls;
    print_words(output, 15);
    for (uint32_t event = 0; event < property_calls; ++event) {
        print_words(property_events[event], 7);
    }
}

static void probe_object_property_access(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original object property state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua,
                      "/data/local/tmp/ab520/collision/object-property-functions.lua");
    semihost(4, "AB_ANDROID_OBJECT_PROPERTY_ACCESS_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        object_property_access_case(sample, property_input);
        print_words(property_input, 15);
        property_calls = 0;
        property_prepare(lua);
        uint32_t results[3][4] = {{0}};
        uint32_t operation = property_input[14];
        if (operation == 0 || operation == 3) {
            property_call(lua, 0, results[0]);
        }
        if (operation != 0) {
            property_call(lua, 1, results[1]);
        }
        if (operation >= 2) {
            property_call(lua, 0, results[2]);
        }
        for (size_t index = 0; index < 3; ++index) {
            print_words(results[index], 4);
        }
        property_snapshot(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB object property access reference complete\n");
}

#endif
