#ifndef AB_ORACLE_UI_VALUE_TEXT_H
#define AB_ORACLE_UI_VALUE_TEXT_H

#include "ui_value_text_cases.h"

#if defined(AB_ORACLE_UI_NATIVE_VALUE_TEXT)
static int ui_value_text_empty_function(void *lua) {
    (void)lua;
    return 0;
}
#endif

static void probe_ui_value_text(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original value text state\n");
    }
    void (*push_bytes)(void *, const char *, unsigned) =
        (void (*)(void *, const char *, unsigned))find_function(
            "_Z15lua_pushlstringP9lua_StatePKcj");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    static const uint32_t special[] = {
        0,          0x80000000, 1,          0x80000001, 0x007fffff, 0x807fffff,
        0x00800000, 0x80800000, 0x7f7fffff, 0xff7fffff, 0x7f800000, 0xff800000,
        0x7fc12345, 0xffc12345, 0x7f812345, 0xff812345, 0x3f800000, 0xbf800000,
        0x3dcccccd, 0x4b800000, 0x4b7fffff};
    uint32_t random = 0x52057a19;
#if defined(AB_ORACLE_UI_NATIVE_VALUE_TEXT)
    semihost(4, "AB_ANDROID_UI_NATIVE_VALUE_TEXT_V1 65536\n");
#else
    semihost(4, "AB_ANDROID_UI_VALUE_TEXT_V1 65536\n");
#endif
    for (size_t test = 0; test < 65536; ++test) {
        uint32_t output[11] = {2, random_word(&random)};
        if (test < sizeof(special) / sizeof(*special)) {
            output[1] = special[test];
        } else if (test % 128 == 0) {
            output[0] = (uint32_t)(test / 128) %
#if defined(AB_ORACLE_UI_NATIVE_VALUE_TEXT)
                        6;
#else
                        4;
#endif
            output[1] = output[0] == 0   ? 0
                        : output[0] == 1 ? (uint32_t)(test / 512) % 2
                        : output[0] == 3 ? (uint32_t)(test / 512) % 8
                                         : output[1];
        }
#if !defined(AB_ORACLE_UI_NATIVE_VALUE_TEXT)
        damage_lua.get_field(lua, -10002, "tostring");
#endif
        if (output[0] == 0) {
            damage_lua.push_nil(lua);
        } else if (output[0] == 1) {
            damage_lua.push_boolean(lua, (int)output[1]);
        } else if (output[0] == 2) {
            FloatBits value = {.bits = output[1]};
            damage_lua.push_number(lua, value.number);
        } else if (output[0] == 3) {
            push_bytes(lua, ui_value_text_cases[output[1]],
                       ui_value_text_lengths[output[1]]);
#if defined(AB_ORACLE_UI_NATIVE_VALUE_TEXT)
        } else if (output[0] == 4) {
            damage_lua.create_table(lua, 0, 0);
        } else {
            damage_lua.push_closure(lua, ui_value_text_empty_function, 0);
#endif
        }
#if !defined(AB_ORACLE_UI_NATIVE_VALUE_TEXT)
        if (damage_lua.call(lua, 1, 1, 0) || damage_lua.type(lua, 1) != 4) {
            fail("Original plain tostring conversion failed\n");
        }
#endif
        unsigned size = 0;
        const char *bytes = damage_lua.to_string(lua, 1, &size);
#if defined(AB_ORACLE_UI_NATIVE_VALUE_TEXT)
        size = 0;
        if (bytes) {
            while (bytes[size]) {
                if (++size > 32)
                    fail("Native value C string exceeds bounded probe\n");
            }
        }
#endif
        if (size > 32 || (!bytes && size)) {
            fail("Original plain tostring output exceeds fixture bounds\n");
        }
        output[2] = size;
        for (unsigned index = 0; index < size; ++index) {
            output[3 + index / 4] |= (uint32_t)(uint8_t)bytes[index]
                                     << ((index % 4) * 8);
        }
        print_words(output, 11);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
#if defined(AB_ORACLE_UI_NATIVE_VALUE_TEXT)
    semihost(4, "AB UI native value text reference complete\n");
#else
    semihost(4, "AB UI value text reference complete\n");
#endif
}

#endif
