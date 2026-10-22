#ifndef AB_ORACLE_GAME_CONTACT_H
#define AB_ORACLE_GAME_CONTACT_H

#if defined(AB_ORACLE_GAME_CONTACT_GLUE)
#include "contact_glue_probe.h"
#endif

static uint32_t game_contact_input[5], game_contact_arguments[4];
static uint32_t game_contact_calls;

static int game_contact_callback(void *lua) {
#if defined(AB_ORACLE_GAME_CONTACT_END)
    if (damage_lua.get_top(lua) != 2 || !damage_named(lua, 1, "first") ||
        !damage_named(lua, 2, "second") || ++game_contact_calls != 1) {
        fail("Unexpected original end-contact callback\n");
    }
#else
    if (damage_lua.get_top(lua) != 7 || !damage_named(lua, 1, "first") ||
        !damage_named(lua, 2, "second") || damage_lua.type(lua, 7) != 5 ||
        ++game_contact_calls != 1) {
        fail("Unexpected original GameLua collision callback\n");
    }
    damage_lua.get_field(lua, 7, "shouldDisableContact");
    if (damage_lua.type(lua, -1) != 1 || damage_lua.to_boolean(lua, -1)) {
        fail("Original contact did not initialize disable state to false\n");
    }
    damage_lua.set_top(lua, 7);
    for (int argument = 3; argument <= 6; ++argument) {
        if (damage_lua.type(lua, argument) != 3) {
            fail("Original collision geometry argument is not numeric\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        game_contact_arguments[(size_t)argument - 3] = value.bits;
    }
    damage_lua.push_boolean(lua, (game_contact_input[0] & 4) != 0);
    damage_lua.set_field(lua, 7, "shouldDisableContact");
#if defined(AB_ORACLE_GAME_CONTACT_GLUE)
    contact_glue_response(lua, game_contact_input);
#else
    if (game_contact_input[0] & 2) {
        damage_lua.create_table(lua, 0, 3);
        damage_lua.push_string(lua, "first");
        damage_lua.set_field(lua, -2, "birdName");
        damage_number(lua, -2, "afterCollisionVelocityX", game_contact_input[3]);
        damage_number(lua, -2, "afterCollisionVelocityY", game_contact_input[4]);
        damage_lua.set_field(lua, 7, "newCollisionPathParameters");
    }
#endif
#endif
    return 0;
}

static void probe_game_contact(void) {
    typedef void *(*Allocate)(unsigned);
    typedef void (*Construct)(void *);
    typedef void (*ConstructWithState)(void *, void *);
    typedef void (*ObjectReference)(void *);
    typedef void (*BeginContact)(void *, void *);
    Allocate allocate = (Allocate)find_function("_Znwj");
    Construct construct_state = (Construct)find_function("_ZN3lua8LuaStateC1Ev");
    ConstructWithState construct_object =
        (ConstructWithState)find_function("_ZN3lua9LuaObjectC1EPNS_8LuaStateE");
    ConstructWithState construct_table =
        (ConstructWithState)find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    Construct destroy_table = (Construct)find_function("_ZN3lua8LuaTableD1Ev");
    ObjectReference claim = (ObjectReference)find_function("_ZN4lang6Object5claimEv");
    ObjectReference release =
        (ObjectReference)find_function("_ZN4lang6Object7releaseEv");
    ObjectReference get_reference =
        (ObjectReference)find_function("_ZNK3lua6detail7RefBase6getRefEv");
#if defined(AB_ORACLE_GAME_CONTACT_END)
    BeginContact begin_contact =
        (BeginContact)find_function("_ZN7GameLua15basicEndContactEP9b2Contact");
#else
    BeginContact begin_contact =
        (BeginContact)find_function("_ZN7GameLua17basicBeginContactEP9b2Contact");
#endif
    initialize_damage_lua();
    uint32_t *wrapper = allocate(20);
    uint32_t *game = allocate(0x1860);
    if (!wrapper || !game) {
        fail("Cannot allocate isolated native collision context\n");
    }
    for (size_t field = 0; field < 0x1860 / 4; ++field) {
        game[field] = 0;
    }
    construct_state(wrapper);
    claim(wrapper);
    construct_object(game, wrapper);
    claim(game);
    construct_table((uint8_t *)game + 0x1810, wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    damage_lua.set_top(lua, 0);
    get_reference(game);
#if defined(AB_ORACLE_GAME_CONTACT_END)
    damage_function(lua, -2, "onEndContact", game_contact_callback);
#else
    damage_function(lua, -2, "onCollision", game_contact_callback);
#endif
    damage_lua.set_top(lua, 0);

    static uint32_t objects[2][50], bodies[2][38], nodes[2][6];
    static uint32_t fixtures[2][11], shapes[2][5], contact[37];
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } names[] = {{5, 5, UINT32_MAX, "first"}, {6, 6, UINT32_MAX, "second"}};
    uint32_t header = (uint32_t)(uintptr_t)((uint8_t *)game + 0x17a8);
    for (size_t index = 0; index < 2; ++index) {
        objects[index][11] = (uint32_t)(uintptr_t)bodies[index];
        objects[index][32] = (uint32_t)(uintptr_t)names[index].bytes;
        nodes[index][0] = index ? 0 : 1;
        nodes[index][1] = index ? (uint32_t)(uintptr_t)nodes[0] : header;
        nodes[index][4] = (uint32_t)(uintptr_t)names[index].bytes;
        nodes[index][5] = (uint32_t)(uintptr_t)objects[index];
        bodies[index][0] = 2;
        bodies[index][1] = 38;
        bodies[index][6] = UINT32_C(0x3f800000);
        fixtures[index][2] = (uint32_t)(uintptr_t)bodies[index];
        fixtures[index][3] = (uint32_t)(uintptr_t)shapes[index];
        shapes[index][2] = UINT32_C(0x3f000000);
    }
    bodies[1][3] = UINT32_C(0x40000000);
    nodes[0][3] = (uint32_t)(uintptr_t)nodes[1];
    game[0x17ac / 4] = (uint32_t)(uintptr_t)nodes[0];
    game[0x17b0 / 4] = (uint32_t)(uintptr_t)nodes[0];
    game[0x17b4 / 4] = (uint32_t)(uintptr_t)nodes[1];
    game[0x17b8 / 4] = 2;
    contact[12] = (uint32_t)(uintptr_t)fixtures[0];
    contact[13] = (uint32_t)(uintptr_t)fixtures[1];
    contact[31] = 1;
#if defined(AB_ORACLE_GAME_CONTACT_GLUE)
    uint32_t glue_pairs[4][2];
    semihost(4, "AB_ANDROID_CONTACT_GLUE_V1 4096\n");
#else
    uint32_t state = UINT32_C(0x520c0111);
#if defined(AB_ORACLE_GAME_CONTACT_END)
    semihost(4, "AB_ANDROID_GAME_CONTACT_END_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_GAME_CONTACT_V1 4096\n");
#endif
#endif
    for (size_t item = 0; item < 4096; ++item) {
#if defined(AB_ORACLE_GAME_CONTACT_GLUE)
        contact_glue_case((uint32_t)item, game_contact_input);
        uint32_t flags = game_contact_input[0];
        game[0x38 / 4] = (uint32_t)(uintptr_t)glue_pairs;
        game[0x3c / 4] = (uint32_t)(uintptr_t)(glue_pairs + game_contact_input[2]);
        game[0x40 / 4] = (uint32_t)(uintptr_t)(glue_pairs + 4);
        for (size_t index = 0; index < 4; ++index) {
            glue_pairs[index][0] = (uint32_t)(uintptr_t)bodies[index % 2];
            glue_pairs[index][1] = (uint32_t)(uintptr_t)bodies[(index + 1) % 2];
        }
#else
        uint32_t flags = item < 512
                             ? (uint32_t)item & 255
                             : 242 | ((uint32_t)item & 1) | (((uint32_t)item & 2) << 1);
        game_contact_input[0] = flags;
        for (size_t field = 1; field < 5; ++field) {
            game_contact_input[field] = random_float(&state, 1, 254, 1);
        }
        if (item % 7 == 0) {
            game_contact_input[3] = game_contact_input[2];
            game_contact_input[4] = game_contact_input[1] ^ UINT32_C(0x80000000);
        }
        if (item % 13 == 0) {
            game_contact_input[3] = item & 1 ? 1 : UINT32_C(0x80000000);
            game_contact_input[4] = item & 2 ? UINT32_C(0x80000001) : 0;
        }
        if (item % 17 == 0) {
            game_contact_input[1] = UINT32_C(0x7f7fffff);
            game_contact_input[2] = UINT32_C(0xff7fffff);
            game_contact_input[3] = UINT32_C(0xff7fffff);
            game_contact_input[4] = UINT32_C(0x7f7fffff);
        }
#endif
        objects[0][30] = game_contact_input[1];
        objects[0][31] = game_contact_input[2];
        objects[0][40] = flags & 1;
        ((uint8_t *)game)[0x185d] = (flags & 8) != 0;
        fixtures[0][10] = flags & 16 ? (uint32_t)(uintptr_t)objects[0] : 0;
        fixtures[1][10] = flags & 32 ? (uint32_t)(uintptr_t)objects[1] : 0;
        get_reference((uint8_t *)game + 0x1810);
        for (size_t index = 0; index < 2; ++index) {
            if (flags & (64u << index)) {
                damage_lua.create_table(lua, 0, 0);
            } else {
                damage_lua.push_nil(lua);
            }
            damage_lua.set_field(lua, 1, index ? "second" : "first");
        }
        damage_lua.set_top(lua, 0);
        game_contact_calls = 0;
        for (size_t field = 0; field < 4; ++field) {
            game_contact_arguments[field] = 0;
        }
        contact[1] = 7;
        print_words(game_contact_input, 5);
        begin_contact(game, contact);
#if defined(AB_ORACLE_GAME_CONTACT_GLUE)
        print_contact_glue(game, glue_pairs, bodies, contact, game_contact_calls);
#else
        uint32_t output[] = {objects[0][40] & 255, objects[0][30], objects[0][31],
                             contact[1], game_contact_calls};
        print_words(output, 5);
        print_words(game_contact_arguments, 4);
#endif
    }
    destroy_table((uint8_t *)game + 0x1810);
    release(game);
    release(wrapper);
#if defined(AB_ORACLE_GAME_CONTACT_GLUE)
    semihost(4, "AB contact glue reference complete\n");
#else
#if defined(AB_ORACLE_GAME_CONTACT_END)
    semihost(4, "AB game contact end reference complete\n");
#else
    semihost(4, "AB game contact reference complete\n");
#endif
#endif
}

#endif
