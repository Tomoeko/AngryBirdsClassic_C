#ifndef AB_ORACLE_CREATE_BOX_H
#define AB_ORACLE_CREATE_BOX_H

#include "reference_create_box.h"

static void probe_create_box(void) {
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*state_construct)(void *) =
        (void (*)(void *))find_function("_ZN3lua8LuaStateC1Ev");
    void (*table_construct)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    void (*table_destroy)(void *) =
        (void (*)(void *))find_function("_ZN3lua8LuaTableD1Ev");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    void (*release)(void *) =
        (void (*)(void *))find_function("_ZN4lang6Object7releaseEv");
    void (*reference)(void *) =
        (void (*)(void *))find_function("_ZNK3lua6detail7RefBase6getRefEv");
    void (*world_construct)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN7b2WorldC1ERK6b2Vec2");
    void (*world_destroy)(void *) = (void (*)(void *))find_function("_ZN7b2WorldD1Ev");
    void (*create_box)(void *, const char **, const char **, float, float, float, float,
                       float, float, float, unsigned, unsigned, float) =
        (void (*)(void *, const char **, const char **, float, float, float, float,
                  float, float, float, unsigned, unsigned,
                  float))find_function("_ZN7GameLua12createBoxLuaESsSsfffffffbbf");
    void **(*object_entry)(void *, const char **) =
        (void **(*)(void *, const char **))find_function(
            "_ZNSt3mapISsP10GameObjectSt4lessISsESaISt4pairIKSsS1_EEEixERS5_");
    void (*remove_object)(void *, const char **) =
        (void (*)(void *, const char **))find_function("_ZN7GameLua12removeObjectESs");
    initialize_damage_lua();
    uint32_t *wrapper = allocate(20);
    if (!wrapper)
        fail("Cannot allocate original box Lua state\n");
    state_construct(wrapper);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    static uint32_t game[0x1a00 / 4], birds[2];
    static uint64_t world[16384];
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } strings[] = {{3, 3, UINT32_MAX, "box"},
                   {6, 6, UINT32_MAX, "unused"},
                   {0, 0, UINT32_MAX, ""}};
    const char *name = strings[0].bytes;
    const uint32_t gravity[2] = {0};
    world_construct(world, gravity);
    game[0x1784 / 4] = (uint32_t)(uintptr_t)world;
    const uint32_t maps[] = {0x1790, 0x17a8, 0x17c0};
    for (unsigned index = 0; index < 3; ++index) {
        uint32_t *header = game + maps[index] / 4;
        header[2] = header[3] = (uint32_t)(uintptr_t)header;
    }
    game[0x17d4 / 4] = game[0x17d8 / 4] = (uint32_t)(uintptr_t)birds;
    game[0x17dc / 4] = (uint32_t)(uintptr_t)(birds + 2);
    table_construct((uint8_t *)game + 0x17f8, wrapper);
    table_construct((uint8_t *)game + 0x1810, wrapper);
    reference((uint8_t *)game + 0x17f8);
    reference((uint8_t *)game + 0x1810);
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_top(lua, 0);
    semihost(4, "AB_ANDROID_CREATE_BOX_V1 256\n");
    for (unsigned sample = 0; sample < CREATE_BOX_CASES; ++sample) {
        uint32_t input[11];
        reference_create_box(sample, input);
        FloatBits value[8];
        for (unsigned index = 0; index < 8; ++index)
            value[index].bits = input[index];
        const char *sprite = strings[1u + input[10]].bytes;
        create_box(game, &name, &sprite, value[0].number, value[1].number,
                   value[2].number, value[3].number, value[4].number, value[5].number,
                   value[6].number, input[8], input[9], value[7].number);
        print_words(input, 11);
        uint32_t *object = *object_entry(game + 0x17a4 / 4, &name);
        const uint32_t *body = object ? (const void *)(uintptr_t)object[11] : NULL;
        if (!body || game[0x17b8 / 4] != 1 || body[26] != 1)
            fail("Original box lost its body/map entry\n");
        uint32_t fields[38] = {0};
        reference((uint8_t *)game + 0x1810);
        damage_lua.get_field(lua, 1, "box");
        if (damage_lua.type(lua, 2) != 5)
            fail("Original box did not publish its world table\n");
        for (unsigned index = 0; index < 19; ++index) {
            damage_lua.get_field(lua, 2, create_box_fields[index]);
            uint32_t kind = (uint32_t)damage_lua.type(lua, 3);
            fields[index * 2] = kind;
            if (kind == 3) {
                FloatBits number = {.number = damage_lua.to_number(lua, 3)};
                fields[index * 2 + 1] = number.bits;
            } else if (kind == 4) {
                if (damage_named(lua, 3, "box"))
                    fields[index * 2 + 1] = 1;
                else if (damage_named(lua, 3, ""))
                    fields[index * 2 + 1] = 2;
                else
                    fail("Unknown original box property string\n");
            } else if (kind != 0) {
                fail("Unexpected original box raw property type\n");
            }
            damage_lua.set_top(lua, 2);
        }
        print_words(fields, 38);
        damage_lua.set_top(lua, 0);
        uint32_t state[43] = {0};
        for (unsigned index = 0; index < 9; ++index) {
            static const unsigned offsets[] = {0x44, 0x48, 0x4c, 0x50, 0x54,
                                               0x58, 0x90, 0x94, 0x98};
            state[index] = object[offsets[index] / 4];
        }
        const uint32_t *fixture = (const void *)(uintptr_t)body[25];
        const uint32_t *shape = (const void *)(uintptr_t)fixture[3];
        state[9] = body[0];
        for (unsigned index = 0; index < 4; ++index)
            state[10 + index] = body[29 + index];
        state[14] = fixture[4];
        state[15] = fixture[5];
        state[16] = fixture[0];
        state[17] = shape[2];
        state[18] = shape[3];
        state[19] = shape[4];
        state[20] = object[0];
        state[21] = ((const uint8_t *)object)[0xa2];
        state[22] = ((const uint8_t *)object)[0x29];
        state[23] = (game[0x17d8 / 4] - game[0x17d4 / 4]) / 4;
        state[24] = body[3];
        state[25] = body[4];
        state[26] = shape[37];
        for (unsigned index = 0; index < 8; ++index) {
            state[27 + index] = shape[5 + index];
            state[35 + index] = shape[21 + index];
        }
        print_words(state, 43);
        remove_object(game, &name);
        game[0x17d8 / 4] = game[0x17d4 / 4];
        if (game[0x17b8 / 4] || ((const uint32_t *)world)[0x19230 / 4])
            fail("Original box fixture retained a body/map entry\n");
    }
    table_destroy((uint8_t *)game + 0x1810);
    table_destroy((uint8_t *)game + 0x17f8);
    world_destroy(world);
    release(wrapper);
    semihost(4, "AB create box reference complete\n");
}

#endif
