#ifndef AB_ORACLE_GAME_JOINT_NAMED_H
#define AB_ORACLE_GAME_JOINT_NAMED_H

#include "../../tests/game_joint_named_cases.h"
#include "game_joint_probe.h"

static uint32_t native_named_id(const char *text) {
    for (uint32_t index = 0; index < 5; ++index) {
        const char *name = joint_named_names[index];
        size_t byte = 0;
        while (text[byte] && name[byte] && text[byte] == name[byte]) {
            ++byte;
        }
        if (text[byte] == name[byte]) {
            return index;
        }
    }
    fail("Unexpected original named joint identifier\n");
    return UINT32_MAX;
}

static void print_named_joints(uint32_t *game, void *lua, const uint32_t bodies[2]) {
    void (*reference)(void *) =
        (void (*)(void *))find_function("_ZNK3lua6detail7RefBase6getRefEv");
    uint32_t *world = (uint32_t *)(uintptr_t)game[0x1784 / 4];
    uint32_t count = (game[0x17e4 / 4] - game[0x17e0 / 4]) / 20;
    uint32_t glue = (game[0x17f0 / 4] - game[0x17ec / 4]) / 20;
    if (count > 4 || glue > 4 || world[0x19234 / 4] > 4) {
        fail("Original named joint counts exceeded controlled capacity\n");
    }
    damage_lua.set_top(lua, 0);
    reference((uint8_t *)game + 0x17f8);
    damage_lua.get_field(lua, -1, "joints");
    uint32_t mask = 0;
    for (uint32_t index = 0; index < 4; ++index) {
        damage_lua.get_field(lua, -1, joint_named_names[index]);
        int type = damage_lua.type(lua, -1);
        if (type != 0 && type != 5) {
            fail("Unexpected original named joint metadata type\n");
        }
        mask |= type == 5 ? 1u << index : 0;
        damage_lua.set_top(lua, 2);
    }
    damage_lua.set_top(lua, 0);
    const uint32_t header[] = {world[0x19234 / 4], count, glue, mask,
                               (world[0x191d4 / 4] & 2u) != 0};
    print_words(header, 5);
    uint32_t output[16];
    for (size_t index = 0; index < 16; ++index) {
        output[index] = UINT32_MAX;
    }
    const uint32_t *joint = (const uint32_t *)(uintptr_t)world[0x1922c / 4];
    size_t index = 0;
    while (joint) {
        if (index >= 4) {
            fail("Original named joint list exceeded controlled capacity\n");
        }
        output[index++] = joint[16];
        joint = (const uint32_t *)(uintptr_t)joint[3];
    }
    if (index != header[0]) {
        fail("Original named joint count disagrees with list\n");
    }
    const uint32_t *records = (const uint32_t *)(uintptr_t)game[0x17e0 / 4];
    for (index = 0; index < count; ++index) {
        output[4 + index * 2] =
            native_named_id((const char *)(uintptr_t)records[index * 5]);
        joint = (const uint32_t *)(uintptr_t)records[index * 5 + 3];
        output[5 + index * 2] = joint ? joint[16] : 0;
    }
    records = (const uint32_t *)(uintptr_t)game[0x17ec / 4];
    for (index = 0; index < glue; ++index) {
        output[12 + index] =
            native_named_id((const char *)(uintptr_t)records[index * 5]);
    }
    print_words(output, 16);
    uint32_t body_output[4];
    for (index = 0; index < 2; ++index) {
        const uint32_t *body = (const uint32_t *)(uintptr_t)bodies[index];
        body_output[index * 2] = (body[1] & 2u) != 0;
        body_output[index * 2 + 1] = body[36];
    }
    print_words(body_output, 4);
}

static void probe_game_joint_named(void) {
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
    void (*destroy_joint)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN7b2World12DestroyJointEP7b2Joint");
    void (*destroy_named)(void *, const void *) = (void (*)(
        void *, const void *))find_function("_ZN7GameLua15destroyJointLuaESs");
    int (*create_joint)(void *, void *) = (int (*)(void *, void *))find_function(
        "_ZN7GameLua14createJointLuaEPN3lua8LuaStateE");
    Construct destroy_record = (Construct)find_function("_ZN7GameLua9jointDataD1Ev");
    initialize_damage_lua();
    uint32_t *wrapper = allocate(20);
    if (!wrapper) {
        fail("Cannot allocate original named-joint Lua wrapper\n");
    }
    state_construct(wrapper);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    static uint32_t game[0x1860 / 4], objects[2][50], nodes[2][6];
    static uint32_t records[4][5], glue[4][5];
    static uint64_t world_storage[16384];
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } names[] = {{5, 5, UINT32_MAX, "first"}, {6, 6, UINT32_MAX, "second"}};
    const uint32_t gravity[2] = {0, 0};
    world_construct(world_storage, gravity);
    uint32_t *world = (uint32_t *)world_storage;
    game[4] = (uint32_t)(uintptr_t)wrapper;
    game[0x1784 / 4] = (uint32_t)(uintptr_t)world_storage;
    table_construct((uint8_t *)game + 0x17f8, wrapper);
    for (size_t index = 0; index < 2; ++index) {
        nodes[index][0] = index ? 0 : 1;
        nodes[index][1] = (uint32_t)(uintptr_t)(index ? nodes[0] : game + 0x17a8 / 4);
        nodes[index][3] = index ? 0 : (uint32_t)(uintptr_t)nodes[1];
        nodes[index][4] = (uint32_t)(uintptr_t)names[index].bytes;
        nodes[index][5] = (uint32_t)(uintptr_t)objects[index];
    }
    game[0x17ac / 4] = game[0x17b0 / 4] = (uint32_t)(uintptr_t)nodes[0];
    game[0x17b4 / 4] = (uint32_t)(uintptr_t)nodes[1];
    game[0x17b8 / 4] = 2;
    game[0x17e0 / 4] = game[0x17e4 / 4] = (uint32_t)(uintptr_t)records;
    game[0x17e8 / 4] = (uint32_t)(uintptr_t)(records + 4);
    game[0x17ec / 4] = game[0x17f0 / 4] = (uint32_t)(uintptr_t)glue;
    game[0x17f4 / 4] = (uint32_t)(uintptr_t)(glue + 4);
    semihost(4, "AB_ANDROID_GAME_JOINT_NAMED_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t header[7], rows[4][2], bodies[2];
        game_joint_named_case(sample, header, rows);
        print_words(header, 7);
        for (size_t index = 0; index < 4; ++index) {
            print_words(rows[index], 2);
        }
        for (size_t index = 0; index < 2; ++index) {
            const uint32_t definition[13] = {
                2, 0, 0, 0, 0, 0, 0, 0, 0, UINT32_C(0x101), 1, 0, UINT32_C(0x3f800000)};
            bodies[index] = (uint32_t)(uintptr_t)create_body(world, definition);
            objects[index][11] = bodies[index];
        }
        reference((uint8_t *)game + 0x17f8);
        damage_lua.create_table(lua, 0, 4);
        damage_lua.set_field(lua, -2, "joints");
        for (size_t index = 0; index < 4; ++index) {
            uint32_t input[28] = {0};
            input[1] = rows[index][0];
            game_joint_lua_input(lua, input);
            damage_lua.push_string(lua, joint_named_names[rows[index][1]]);
            damage_lua.set_field(lua, -2, "name");
            world[0x191d4 / 4] |= header[2] & (1u << index) ? 2u : 0u;
            if (create_joint(game, wrapper) != 0) {
                fail("Cannot create original named-joint case\n");
            }
            world[0x191d4 / 4] &= ~UINT32_C(2);
            if (input[1] != 5) {
                const uint32_t *last = (const uint32_t *)(uintptr_t)game[0x17e4 / 4];
                uint32_t *joint = (uint32_t *)(uintptr_t)last[-2];
                if (joint) {
                    joint[16] = (uint32_t)index + 1;
                }
            }
        }
        for (size_t index = 0; index < 2; ++index) {
            uint32_t *body = (uint32_t *)(uintptr_t)bodies[index];
            body[1] = (body[1] & ~UINT32_C(2)) | (((sample >> (4 + index)) & 1u) << 1);
            body[36] = index ? UINT32_C(0x3e000000) : UINT32_C(0x3e800000);
        }
        world[0x191d4 / 4] |= header[1] ? 2u : 0u;
        print_named_joints(game, lua, bodies);
        for (size_t request = 0; request < 4; ++request) {
            struct {
                uint32_t length, capacity, references;
                char bytes[8];
            } name = {.references = UINT32_MAX};
            const char *text = joint_named_names[header[3 + request]];
            do {
                name.bytes[name.length] = text[name.length];
            } while (text[name.length++]);
            name.capacity = --name.length;
            const char *value = name.bytes;
            destroy_named(game, &value);
            print_named_joints(game, lua, bodies);
        }
        world[0x191d4 / 4] &= ~UINT32_C(2);
        while (world[0x1922c / 4]) {
            destroy_joint(world, (void *)(uintptr_t)world[0x1922c / 4]);
        }
        size_t count = (game[0x17e4 / 4] - game[0x17e0 / 4]) / 20;
        size_t glue_count = (game[0x17f0 / 4] - game[0x17ec / 4]) / 20;
        for (size_t index = 0; index < count; ++index) {
            destroy_record(records[index]);
        }
        for (size_t index = 0; index < glue_count; ++index) {
            destroy_record(glue[index]);
        }
        game[0x17e4 / 4] = (uint32_t)(uintptr_t)records;
        game[0x17f0 / 4] = (uint32_t)(uintptr_t)glue;
        for (size_t index = 0; index < 2; ++index) {
            destroy_body(world, (void *)(uintptr_t)bodies[index]);
        }
        damage_lua.set_top(lua, 0);
    }
    table_destroy((uint8_t *)game + 0x17f8);
    world_destroy(world_storage);
    release(wrapper);
    semihost(4, "AB game joint named reference complete\n");
}

#endif
