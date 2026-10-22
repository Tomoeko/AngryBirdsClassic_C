#ifndef AB_ORACLE_OBJECT_MISSING_H
#define AB_ORACLE_OBJECT_MISSING_H

#include "../../tests/object_missing_cases.h"

static uint32_t missing_calls, missing_message_hash;
static void (*missing_native_log)(void *, const void *);

static int missing_log(void *lua) {
    unsigned length = 0;
    const char *message = damage_lua.to_string(lua, 1, &length);
    if (damage_lua.get_top(lua) != 1 || !message || length >= 512) {
        fail("Unexpected original missing-definition diagnostic\n");
    }
    static struct {
        uint32_t length, capacity, references;
        char bytes[512];
    } name;
    name.length = length;
    name.capacity = 511;
    name.references = 1000;
    missing_message_hash = UINT32_C(2166136261);
    for (size_t index = 0; index < length; ++index) {
        name.bytes[index] = message[index];
        missing_message_hash =
            (missing_message_hash ^ (uint8_t)message[index]) * UINT32_C(16777619);
    }
    name.bytes[length] = 0;
    uint32_t reference = (uint32_t)(uintptr_t)name.bytes;
    missing_native_log(NULL, &reference);
    ++missing_calls;
    return 0;
}

static void probe_object_missing(void) {
    const char *path = "/data/local/tmp/ab520/object-create-unknown.lua";
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    static char bytes[8874];
    size_t offset = 0;
    if (file < 0) {
        fail("Cannot open private original ObjectCreation prototype\n");
    }
    while (offset < sizeof(bytes)) {
        int count = system_call(3, file, bytes + offset, sizeof(bytes) - offset);
        if (count <= 0 || (size_t)count > sizeof(bytes) - offset) {
            fail("Incomplete private original ObjectCreation prototype\n");
        }
        offset += (size_t)count;
    }
    char extra;
    if (system_call(3, file, &extra, 1) != 0) {
        fail("Original ObjectCreation prototype exceeds its pinned extent\n");
    }
    (void)system_call(6, file, NULL, 0);
    initialize_damage_lua();
    missing_native_log =
        (void (*)(void *, const void *))find_function("_ZN7GameLua10printErrorESs");
    void (*push_value)(void *, int) =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot create isolated original missing-definition Lua state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, -2, "printError", missing_log);
    damage_lua.set_field(lua, -10002, "gamelua");
    if (damage_lua.load(lua, bytes, sizeof(bytes), "ObjectCreation-missing") != 0) {
        fail("Cannot load original ObjectCreation prototype\n");
    }
    semihost(4, "AB_ANDROID_OBJECT_MISSING_V1 64\n");
    for (uint32_t sample = 0; sample < 64; ++sample) {
        uint32_t input[3];
        object_missing_case(sample, input);
        print_words(input, 3);
        missing_calls = 0;
        missing_message_hash = 0;
        push_value(lua, 1);
        damage_lua.create_table(lua, 0, 1);
        damage_lua.create_table(lua, 0, 1);
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, -2, "known_definition");
        damage_lua.set_field(lua, -2, "blocks");
        damage_lua.push_string(lua, missing_definition_names[input[1]]);
        damage_lua.push_string(lua, "instance");
        FloatBits coordinate = {.bits = input[2] & 4u ? UINT32_C(0x7fc00000)
                                                      : UINT32_C(0x3f800000)};
        damage_lua.push_number(lua, coordinate.number);
        damage_lua.push_number(lua, coordinate.number);
        if (input[2] == 0) {
            damage_lua.push_nil(lua);
        } else if (input[2] < 3) {
            damage_lua.push_boolean(lua, input[2] == 2);
        } else if (input[2] == 3) {
            damage_lua.push_number(lua, 0);
        } else if (input[2] == 4) {
            damage_lua.push_string(lua, "unused");
        } else {
            damage_lua.create_table(lua, 0, 0);
        }
        int status = damage_lua.call(lua, 6, 1, 0);
        unsigned length = 0;
        const char *result = damage_lua.to_string(lua, -1, &length);
        uint32_t output[] = {(uint32_t)status, (uint32_t)damage_lua.type(lua, -1),
                             length, missing_calls, missing_message_hash};
        print_words(output, 5);
        if (status || !result || length || missing_calls != 1 || output[1] != 4) {
            fail("Original missing-definition branch did not return an empty string\n");
        }
        damage_lua.set_top(lua, 1);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB object missing reference complete\n");
}

#endif
