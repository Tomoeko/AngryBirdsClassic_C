#ifndef AB_ORACLE_GAME_JOINT_PARAMETERS_H
#define AB_ORACLE_GAME_JOINT_PARAMETERS_H

#include "../../tests/game_joint_parameter_cases.h"

static const char *const parameter_names[] = {"motor",     "motorSpeed",  "maxTorque",
                                              "limit",     "lowerLimit",  "upperLimit",
                                              "frequency", "dampingRatio"};

static void parameter_state(uint32_t *joint, uint32_t type, uint32_t sample,
                            bool seed) {
    uint32_t output[5] = {0};
    size_t impulse = type == 1 ? 25u : type == 2 ? 26u : type == 3 ? 21u : 26u;
    size_t count = type == 1 ? 1u : type == 2 ? 3u : 4u;
    const uint32_t values[] = {UINT32_C(0x3e000000), UINT32_C(0xbe800000),
                               UINT32_C(0x3f000000), UINT32_C(0xbf400000)};
    for (size_t field = 0; field < count; ++field) {
        if (seed) {
            joint[impulse + field] = values[field];
        }
        output[field] = joint[impulse + field];
    }
    if (type >= 3) {
        size_t limit = type == 3 ? 56u : 35u;
        if (seed) {
            joint[limit] = sample % 4;
        }
        output[4] = joint[limit];
    }
    if (!seed) {
        print_words(output, 5);
    }
}

static bool parameter_word_mutable(uint32_t type, size_t field) {
    if (type == 1) {
        return field == 17 || field == 18;
    }
    if (type == 2) {
        return false;
    }
    return type == 3 ? field == 23 || (field >= 25 && field <= 28) || field == 30 ||
                           field == 31
                     : field == 28 || (field >= 30 && field <= 34);
}

static void probe_joint_parameters(uint32_t *game, void *wrapper, void *lua,
                                   uint32_t *first, const uint32_t bodies[2],
                                   uint32_t records[2][5], const uint32_t input[28]) {
    int (*create)(void *, void *) = (int (*)(void *, void *))find_function(
        "_ZN7GameLua14createJointLuaEPN3lua8LuaStateE");
    int (*set)(void *, void *) = (int (*)(void *, void *))find_function(
        "_ZN7GameLua18setJointParametersEPN3lua8LuaStateE");
    void (*reference)(void *) =
        (void (*)(void *))find_function("_ZNK3lua6detail7RefBase6getRefEv");
    uint32_t *world = (uint32_t *)(uintptr_t)game[0x1784 / 4];
    uint32_t *second = NULL;
    uint32_t second_type = 0;
    if (input[0] & 512u) {
        uint32_t duplicate[28];
        for (size_t field = 0; field < 28; ++field) {
            duplicate[field] = input[field];
        }
        second_type = input[0] & 1024u ? input[1] % 4 + 1 : input[1];
        duplicate[1] = second_type;
        game_joint_lua_input(lua, duplicate);
        if (create(game, wrapper) != 0 || world[0x19234 / 4] != 2) {
            fail("Cannot create native duplicate parameter joint\n");
        }
        second = (uint32_t *)(uintptr_t)world[0x1922c / 4];
    }
    print_words(&second_type, 1);
    const uint32_t empty[20] = {0};
    if (second) {
        print_game_joint_definition(second, second_type);
    } else {
        print_words(empty, 20);
    }
    for (uint32_t pass = 0; pass < 2; ++pass) {
        uint32_t types[8], values[8], before[64], before_second[64];
        uint32_t before_bodies[2][38];
        game_joint_parameter_case(input[0], pass, types, values);
        print_words(types, 8);
        print_words(values, 8);
        parameter_state(first, input[1], input[0], true);
        if (second) {
            parameter_state(second, second_type, input[0], true);
        }
        const size_t sizes[] = {44, 52, 57, 64};
        for (size_t field = 0; field < sizes[input[1] - 1]; ++field) {
            before[field] = first[field];
        }
        if (second) {
            for (size_t field = 0; field < sizes[second_type - 1]; ++field) {
                before_second[field] = second[field];
            }
        }
        for (size_t index = 0; index < 2; ++index) {
            uint32_t *body = (uint32_t *)(uintptr_t)bodies[index];
            body[1] =
                (body[1] & ~UINT32_C(2)) | (((input[0] >> (4 + index)) & 1u) << 1);
            body[36] = index ? UINT32_C(0x3e000000) : UINT32_C(0x3e800000);
            for (size_t field = 0; field < 38; ++field) {
                before_bodies[index][field] = body[field];
            }
        }
        world[0x191d4 / 4] |= input[0] & 256u ? 2u : 0u;
        damage_lua.set_top(lua, 0);
        damage_lua.create_table(lua, 0, 9);
        damage_lua.push_string(lua, input[0] & 2048u ? "absent" : "joint");
        damage_lua.set_field(lua, -2, "name");
        for (size_t field = 0; field < 8; ++field) {
            if (types[field] == 1) {
                damage_lua.push_boolean(lua, (int)values[field]);
            } else if (types[field] == 3) {
                FloatBits value = {.bits = values[field]};
                damage_lua.push_number(lua, value.number);
            } else if (types[field] == 4) {
                damage_lua.push_string(lua, "invalid");
            } else {
                continue;
            }
            damage_lua.set_field(lua, -2, parameter_names[field]);
        }
        if (set(game, wrapper) != 0) {
            fail("Original joint parameter update returned values\n");
        }
        world[0x191d4 / 4] &= ~UINT32_C(2);
        print_game_joint_definition(first, input[1]);
        if (second) {
            print_game_joint_definition(second, second_type);
        } else {
            print_words(empty, 20);
        }
        for (size_t index = 0; index < 2; ++index) {
            const uint32_t *body = (const uint32_t *)(uintptr_t)bodies[index];
            const uint32_t state[] = {body[1] & UINT32_C(0xffff), body[36]};
            print_words(state, 2);
        }
        parameter_state(first, input[1], input[0], false);
        if (second) {
            parameter_state(second, second_type, input[0], false);
        } else {
            print_words(empty, 5);
        }
        print_game_joint_metadata(lua, (uint8_t *)game + 0x17f8, reference);
        for (size_t index = 0; index < 2; ++index) {
            const uint32_t *body = (const uint32_t *)(uintptr_t)bodies[index];
            for (size_t field = 0; field < 38; ++field) {
                if (field != 1 && field != 36 &&
                    body[field] != before_bodies[index][field]) {
                    fail("Original parameter update changed a preserved body field\n");
                }
            }
            if ((body[1] & ~UINT32_C(2)) != (before_bodies[index][1] & ~UINT32_C(2))) {
                fail("Original parameter update changed a non-awake body flag\n");
            }
        }
        for (size_t field = 0; field < sizes[input[1] - 1]; ++field) {
            if (!parameter_word_mutable(input[1], field) &&
                first[field] != before[field]) {
                fail("Original parameter update changed a preserved joint field\n");
            }
        }
        if (second) {
            for (size_t field = 0; field < sizes[second_type - 1]; ++field) {
                if (second[field] != before_second[field]) {
                    fail("Original parameter update changed a later duplicate\n");
                }
            }
        }
    }
    if (second) {
        void (*destroy)(void *, void *) = (void (*)(void *, void *))find_function(
            "_ZN7b2World12DestroyJointEP7b2Joint");
        void (*destroy_record)(void *) =
            (void (*)(void *))find_function("_ZN7GameLua9jointDataD1Ev");
        destroy(world, second);
        destroy_record(records[1]);
        game[0x17e4 / 4] = (uint32_t)(uintptr_t)(records[0] + 5);
    }
}

#endif
