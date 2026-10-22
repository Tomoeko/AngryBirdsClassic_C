#ifndef AB_ORACLE_GAME_JOINT_BREAK_H
#define AB_ORACLE_GAME_JOINT_BREAK_H

#include "../../tests/game_joint_break_cases.h"

static uint32_t joint_break_requests[7], joint_break_count;

static int joint_break_request(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 4 ||
        joint_break_count >= 7) {
        fail("Invalid original deferred joint destruction callback\n");
    }
    for (uint32_t index = 0; index < 7; ++index) {
        if (damage_named(lua, 1, joint_break_names[index])) {
            joint_break_requests[joint_break_count++] = index;
            return 0;
        }
    }
    fail("Unknown original deferred joint name\n");
    return 0;
}

static void joint_break_value(void *lua, uint32_t type, uint32_t bits,
                              const char *key) {
    if (type == 0) {
        damage_lua.push_nil(lua);
    } else if (type == 1) {
        damage_lua.push_boolean(lua, bits != 0);
    } else if (type == 3) {
        FloatBits value = {.bits = bits};
        damage_lua.push_number(lua, value.number);
    } else {
        damage_lua.push_string(lua, "invalid");
    }
    damage_lua.set_field(lua, -2, key);
}

static void probe_game_joint_break(void) {
    typedef void (*Construct)(void *);
    typedef void (*WithState)(void *, void *);
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    Construct state_construct = (Construct)find_function("_ZN3lua8LuaStateC1Ev");
    Construct claim = (Construct)find_function("_ZN4lang6Object5claimEv");
    Construct release = (Construct)find_function("_ZN4lang6Object7releaseEv");
    Construct reference = (Construct)find_function("_ZNK3lua6detail7RefBase6getRefEv");
    WithState object_construct =
        (WithState)find_function("_ZN3lua9LuaObjectC1EPNS_8LuaStateE");
    WithState table_construct =
        (WithState)find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    Construct table_destroy = (Construct)find_function("_ZN3lua8LuaTableD1Ev");
    void (*dispatch)(void *, const void *, float) =
        (void (*)(void *, const void *, float))find_function(
            "_ZN7GameLua22destroyBreakableJointsESsf");
    initialize_damage_lua();
    uint32_t *wrapper = allocate(20), *game = allocate(0x1860);
    if (!wrapper || !game) {
        fail("Cannot allocate original joint breakage context\n");
    }
    for (size_t index = 0; index < 0x1860 / 4; ++index) {
        game[index] = 0;
    }
    state_construct(wrapper);
    claim(wrapper);
    object_construct(game, wrapper);
    claim(game);
    table_construct((uint8_t *)game + 0x17f8, wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    reference(game);
    damage_function(lua, -2, "destroyJointDeferred", joint_break_request);
    damage_lua.set_top(lua, 0);
    struct NativeName {
        uint32_t length, capacity, references;
        char bytes[12];
    } names[7], objects[8];
    for (size_t group = 0; group < 2; ++group) {
        size_t count = group ? 8 : 7;
        for (size_t index = 0; index < count; ++index) {
            struct NativeName *name = group ? objects + index : names + index;
            const char *text =
                group ? joint_break_objects[index] : joint_break_names[index];
            uint32_t length = 0;
            do {
                name->bytes[length] = text[length];
            } while (text[length++]);
            name->length = name->capacity = length - 1;
            name->references = UINT32_MAX;
        }
    }
    uint32_t native_records[6][5] = {{0}};
    semihost(4, "AB_ANDROID_GAME_JOINT_BREAK_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t header[5], records[7][7], before[6][5];
        game_joint_break_case(sample, header, records);
        print_words(header, 5);
        for (size_t index = 0; index < 7; ++index) {
            print_words(records[index], 7);
        }
        reference((uint8_t *)game + 0x17f8);
        damage_lua.create_table(lua, 0, 7);
        for (size_t index = 0; index < 7; ++index) {
            if (index < header[3]) {
                native_records[index][0] =
                    (uint32_t)(uintptr_t)names[records[index][0]].bytes;
                native_records[index][1] =
                    (uint32_t)(uintptr_t)objects[records[index][1]].bytes;
                native_records[index][2] =
                    (uint32_t)(uintptr_t)objects[records[index][2]].bytes;
                native_records[index][3] = native_records[index][4] = 0;
            } else if (index != 6 || !header[4]) {
                continue;
            }
            damage_lua.create_table(lua, 0, 2);
            joint_break_value(lua, records[index][3], records[index][4], "breakable");
            joint_break_value(lua, records[index][5], records[index][6], "breakForce");
            damage_lua.set_field(lua, -2, joint_break_names[records[index][0]]);
        }
        damage_lua.set_field(lua, -2, "joints");
        damage_lua.set_top(lua, 0);
        for (size_t index = 0; index < 6; ++index) {
            for (size_t field = 0; field < 5; ++field) {
                before[index][field] = native_records[index][field];
            }
        }
        game[0x17e0 / 4] = (uint32_t)(uintptr_t)native_records;
        game[0x17e4 / 4] = (uint32_t)(uintptr_t)(native_records + header[3]);
        game[0x17e8 / 4] = (uint32_t)(uintptr_t)(native_records + 6);
        joint_break_count = 0;
        for (size_t index = 0; index < 7; ++index) {
            joint_break_requests[index] = UINT32_MAX;
        }
        const char *object = objects[header[1]].bytes;
        FloatBits force = {.bits = header[2]};
        dispatch(game, &object, force.number);
        print_words(&joint_break_count, 1);
        print_words(joint_break_requests, 7);
        for (size_t index = 0; index < 6; ++index) {
            for (size_t field = 0; field < 5; ++field) {
                if (before[index][field] != native_records[index][field]) {
                    fail("Original breakage unexpectedly changed its record vector\n");
                }
            }
        }
        if (damage_lua.get_top(lua) != 0 ||
            game[0x17e4 / 4] != (uint32_t)(uintptr_t)(native_records + header[3])) {
            fail("Original breakage changed vector size or leaked Lua stack\n");
        }
    }
    table_destroy((uint8_t *)game + 0x17f8);
    release(game);
    release(wrapper);
    semihost(4, "AB game joint break reference complete\n");
}

#endif
