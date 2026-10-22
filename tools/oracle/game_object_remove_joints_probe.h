#ifndef AB_ORACLE_GAME_OBJECT_REMOVE_JOINTS_H
#define AB_ORACLE_GAME_OBJECT_REMOVE_JOINTS_H

#include "../../tests/game_object_remove_joints_cases.h"
#include "game_joint_probe.h"

static uint32_t removal_joint_identifier(const char *name) {
    for (uint32_t index = 0; index < 4; ++index) {
        const char *expected = removal_joint_names[index];
        size_t byte = 0;
        while (name[byte] && name[byte] == expected[byte]) {
            ++byte;
        }
        if (name[byte] == expected[byte]) {
            return index + 1u;
        }
    }
    fail("Unexpected original object-removal joint name\n");
    return 0;
}

static void removal_joint_snapshot(uint32_t *game, void *lua,
                                   const uint32_t objects[3]) {
    void (*reference)(void *) =
        (void (*)(void *))find_function("_ZNK3lua6detail7RefBase6getRefEv");
    void *(*get_body)(void *, const char **) =
        (void *(*)(void *, const char **))find_function("_ZN7GameLua7getBodyERKSs");
    uint32_t *world = (uint32_t *)(uintptr_t)game[0x1784 / 4];
    uint32_t output[25] = {game[0x17b8 / 4],
                           world[0x19230 / 4],
                           world[0x19234 / 4],
                           (game[0x17e4 / 4] - game[0x17e0 / 4]) / 20,
                           (game[0x17f0 / 4] - game[0x17ec / 4]) / 20,
                           0,
                           (world[0x191d4 / 4] & 2u) != 0};
    if (output[3] > 4 || output[4] > 4) {
        fail("Original object-removal joint vector overflow\n");
    }
    damage_lua.set_top(lua, 0);
    reference((uint8_t *)game + 0x17f8);
    damage_lua.get_field(lua, -1, "joints");
    for (uint32_t index = 0; index < 4; ++index) {
        damage_lua.get_field(lua, -1, removal_joint_names[index]);
        output[5] |= damage_lua.type(lua, -1) == 5 ? 1u << index : 0u;
        damage_lua.set_top(lua, 2);
    }
    damage_lua.set_top(lua, 0);
    for (uint32_t index = 0; index < 3; ++index) {
        struct {
            uint32_t length, capacity, references;
            char bytes[8];
        } name = {.references = UINT32_MAX};
        const char *text = removal_object_names[index];
        while (text[name.length]) {
            name.bytes[name.length] = text[name.length];
            ++name.length;
        }
        name.capacity = name.length;
        const char *key = name.bytes;
        if (get_body(game, &key)) {
            const uint32_t *object = (const uint32_t *)(uintptr_t)objects[index];
            output[7 + index * 3] = 1;
            output[8 + index * 3] = ((const uint8_t *)object)[0xa6];
            output[9 + index * 3] = object[0xa8 / 4];
        }
    }
    const uint32_t *records = (const uint32_t *)(uintptr_t)game[0x17e0 / 4];
    for (uint32_t index = 0; index < output[3]; ++index) {
        output[16 + index] =
            removal_joint_identifier((const char *)(uintptr_t)records[index * 5]);
    }
    records = (const uint32_t *)(uintptr_t)game[0x17ec / 4];
    for (uint32_t index = 0; index < output[4]; ++index) {
        output[20 + index] =
            removal_joint_identifier((const char *)(uintptr_t)records[index * 5]);
    }
    const uint32_t *joint = (const uint32_t *)(uintptr_t)world[0x1922c / 4];
    uint32_t count = 0;
    while (joint) {
        if (!joint[16] || joint[16] > 4 || ++count > 4) {
            fail("Unexpected original removal engine-joint identity\n");
        }
        output[24] |= 1u << (joint[16] - 1u);
        joint = (const uint32_t *)(uintptr_t)joint[3];
    }
    if (count != output[2]) {
        fail("Original removal engine-joint count disagrees with list\n");
    }
    print_words(output, 25);
}

static void probe_game_object_remove_joints(void) {
    typedef void (*Construct)(void *);
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    Construct state_construct = (Construct)find_function("_ZN3lua8LuaStateC1Ev");
    void (*table_construct)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    Construct table_destroy = (Construct)find_function("_ZN3lua8LuaTableD1Ev");
    Construct claim = (Construct)find_function("_ZN4lang6Object5claimEv");
    Construct release = (Construct)find_function("_ZN4lang6Object7releaseEv");
    Construct reference = (Construct)find_function("_ZNK3lua6detail7RefBase6getRefEv");
    void (*world_construct)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN7b2WorldC1ERK6b2Vec2");
    Construct world_destroy = (Construct)find_function("_ZN7b2WorldD1Ev");
    void *(*create_body)(void *, const void *) =
        (void *(*)(void *, const void *))find_function(
            "_ZN7b2World10CreateBodyEPK9b2BodyDef");
    void (*destroy_body)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN7b2World11DestroyBodyEP6b2Body");
    void (*object_construct)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN10GameObjectC1EP7b2World");
    void **(*insert)(void *, const char **) =
        (void **(*)(void *, const char **))find_function(
            "_ZNSt3mapISsP10GameObjectSt4lessISsESaISt4pairIKSsS1_EEEixERS5_");
    void (*set_rolling)(void *, const char **, const char **) =
        (void (*)(void *, const char **, const char **))find_function(
            "_ZN7GameLua15setRollingSoundESsSs");
    void *(*get_body)(void *, const char **) =
        (void *(*)(void *, const char **))find_function("_ZN7GameLua7getBodyERKSs");
    void (*remove_object)(void *, const char **) =
        (void (*)(void *, const char **))find_function("_ZN7GameLua12removeObjectESs");
    int (*create_joint)(void *, void *) = (int (*)(void *, void *))find_function(
        "_ZN7GameLua14createJointLuaEPN3lua8LuaStateE");
    Construct destroy_record = (Construct)find_function("_ZN7GameLua9jointDataD1Ev");
    initialize_damage_lua();
    uint32_t *wrapper = allocate(20);
    if (!wrapper) {
        fail("Cannot allocate original removal-joint Lua state\n");
    }
    state_construct(wrapper);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    static uint32_t game[0x1a00 / 4], records[4][5], glue[4][5];
    static uint64_t world_storage[16384];
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } names[] = {{5, 5, UINT32_MAX, "first"},
                 {6, 6, UINT32_MAX, "second"},
                 {5, 5, UINT32_MAX, "third"},
                 {7, 7, UINT32_MAX, "missing"}};
    const uint32_t gravity[2] = {0, 0};
    const uint32_t definition[13] = {
        2, 0, 0, 0, 0, 0, 0, 0, 0, UINT32_C(0x101), 1, 0, UINT32_C(0x3f800000)};
    world_construct(world_storage, gravity);
    uint32_t *world = (uint32_t *)world_storage;
    game[4] = (uint32_t)(uintptr_t)wrapper;
    game[0x1784 / 4] = (uint32_t)(uintptr_t)world;
    table_construct((uint8_t *)game + 0x17f8, wrapper);
    const uint32_t maps[] = {0x1790, 0x17a8, 0x17c0};
    for (uint32_t map = 0; map < 3; ++map) {
        uint32_t *header = game + maps[map] / 4;
        header[2] = header[3] = (uint32_t)(uintptr_t)header;
    }
    game[0x17e0 / 4] = game[0x17e4 / 4] = (uint32_t)(uintptr_t)records;
    game[0x17e8 / 4] = (uint32_t)(uintptr_t)(records + 4);
    game[0x17ec / 4] = game[0x17f0 / 4] = (uint32_t)(uintptr_t)glue;
    game[0x17f4 / 4] = (uint32_t)(uintptr_t)(glue + 4);
    semihost(4, "AB_ANDROID_GAME_OBJECT_REMOVE_JOINTS_V1 512\n");
    for (uint32_t sample = 0; sample < 512; ++sample) {
        uint32_t header[4], rows[4][4], objects[3];
        game_object_remove_joints_case(sample, header, rows);
        print_words(header, 4);
        for (size_t index = 0; index < 4; ++index) {
            print_words(rows[index], 4);
        }
        for (uint32_t index = 0; index < 3; ++index) {
            const char *key = names[index].bytes;
            uint32_t *object = allocate(200);
            if (!object) {
                fail("Cannot allocate original removal-joint object\n");
            }
            object_construct(object, world);
            object[0] = 0;
            object[11] = (uint32_t)(uintptr_t)create_body(world, definition);
            if (!object[11]) {
                fail("Cannot create original removal-joint body\n");
            }
            *insert(game + 0x17a4 / 4, &key) = object;
            set_rolling(game, &key, &key);
            uint32_t empty = object[32];
            object[32] = object[23];
            object[23] = empty;
            ((uint8_t *)object)[0xa6] = (uint8_t)((sample >> index) & 1u);
            object[0xa8 / 4] = UINT32_C(0x3e000000) + index * UINT32_C(0x800000);
            objects[index] = (uint32_t)(uintptr_t)object;
        }
        damage_lua.set_top(lua, 0);
        reference((uint8_t *)game + 0x17f8);
        damage_lua.create_table(lua, 0, 4);
        damage_lua.set_field(lua, -2, "joints");
        for (uint32_t index = 0; index < 4; ++index) {
            uint32_t input[28] = {0};
            input[1] = rows[index][0];
            game_joint_lua_input(lua, input);
            damage_lua.push_string(lua, removal_joint_names[index]);
            damage_lua.set_field(lua, -2, "name");
            damage_lua.push_string(lua, removal_object_names[rows[index][1]]);
            damage_lua.set_field(lua, -2, "end1");
            damage_lua.push_string(lua, removal_object_names[rows[index][2]]);
            damage_lua.set_field(lua, -2, "end2");
            damage_number(lua, 10, "destroyTimer", rows[index][3]);
            world[0x191d4 / 4] |= header[3] & (1u << index) ? 2u : 0u;
            if (create_joint(game, wrapper) != 0) {
                fail("Cannot create original object-removal joint\n");
            }
            world[0x191d4 / 4] &= ~UINT32_C(2);
            if (input[1] != 5) {
                const uint32_t *end = (const uint32_t *)(uintptr_t)game[0x17e4 / 4];
                uint32_t *joint = (uint32_t *)(uintptr_t)end[-2];
                if (joint) {
                    joint[16] = index + 1u;
                }
            }
        }
        world[0x191d4 / 4] |= header[2] ? 2u : 0u;
        removal_joint_snapshot(game, lua, objects);
        for (uint32_t pass = 0; pass < 3; ++pass) {
            uint32_t requested = pass < 2 ? header[1] : (header[1] + 1u) % 4u;
            const char *key = names[requested].bytes;
            remove_object(game, &key);
            removal_joint_snapshot(game, lua, objects);
        }
        world[0x191d4 / 4] &= ~UINT32_C(2);
        uint32_t count = (game[0x17e4 / 4] - game[0x17e0 / 4]) / 20;
        uint32_t glue_count = (game[0x17f0 / 4] - game[0x17ec / 4]) / 20;
        for (uint32_t index = 0; index < count; ++index) {
            destroy_record(records[index]);
        }
        for (uint32_t index = 0; index < glue_count; ++index) {
            destroy_record(glue[index]);
        }
        game[0x17e4 / 4] = (uint32_t)(uintptr_t)records;
        game[0x17f0 / 4] = (uint32_t)(uintptr_t)glue;
        for (uint32_t index = 0; index < 3; ++index) {
            const char *key = names[index].bytes;
            if (get_body(game, &key)) {
                remove_object(game, &key);
            }
        }
        while (world[0x19228 / 4]) {
            destroy_body(world, (void *)(uintptr_t)world[0x19228 / 4]);
        }
        damage_lua.set_top(lua, 0);
    }
    table_destroy((uint8_t *)game + 0x17f8);
    world_destroy(world);
    release(wrapper);
    semihost(4, "AB game object removal joints reference complete\n");
}

#endif
