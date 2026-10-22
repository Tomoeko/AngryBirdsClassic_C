#ifndef AB_ORACLE_CONTACT_GLUE_H
#define AB_ORACLE_CONTACT_GLUE_H

#include "../../tests/contact_glue_cases.h"

static void contact_glue_response(void *lua, const uint32_t input[5]) {
    if (input[1] == 0) {
        damage_lua.push_nil(lua);
    } else if (input[1] <= 2) {
        damage_lua.push_boolean(lua, input[1] == 2);
    } else if (input[1] == 3) {
        damage_lua.push_number(lua, 0);
    } else if (input[1] == 4) {
        damage_lua.push_string(lua, "invalid");
    } else {
        void (*set_index)(void *, int, int) =
            (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
        damage_lua.create_table(lua, 2, 0);
        for (int index = 1; index <= 2; ++index) {
            damage_lua.push_string(lua, contact_glue_names[input[2 + (size_t)index]]);
            set_index(lua, -2, index);
        }
    }
    damage_lua.set_field(lua, 7, "glueContactPair");
}

static void print_contact_glue(const uint32_t *game, uint32_t pairs[4][2],
                               uint32_t bodies[2][38], const uint32_t *contact,
                               uint32_t calls) {
    uint32_t end = game[0x3c / 4];
    uint32_t begin = (uint32_t)(uintptr_t)pairs;
    if (end < begin || (end - begin) % 8 || end - begin > sizeof(uint32_t[4][2])) {
        fail("Unexpected original contact glue queue extent\n");
    }
    uint32_t count = (end - begin) / 8;
    uint32_t header[] = {contact[1], calls, count};
    uint32_t output[8];
    for (size_t index = 0; index < 8; ++index) {
        output[index] = UINT32_MAX;
    }
    for (size_t index = 0; index < count; ++index) {
        for (size_t body = 0; body < 2; ++body) {
            uint32_t pointer = pairs[index][body];
            output[index * 2 + body] = pointer == (uint32_t)(uintptr_t)bodies[0]   ? 1u
                                       : pointer == (uint32_t)(uintptr_t)bodies[1] ? 2u
                                       : pointer == 0                              ? 0u
                                                      : UINT32_MAX;
            if (output[index * 2 + body] == UINT32_MAX) {
                fail("Unexpected original contact glue body pointer\n");
            }
        }
    }
    print_words(header, 3);
    print_words(output, 8);
}

#endif
