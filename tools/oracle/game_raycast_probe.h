#ifndef AB_ORACLE_GAME_RAYCAST_H
#define AB_ORACLE_GAME_RAYCAST_H

#include "../../tests/game_raycast_cases.h"

static void game_ray_native_result(void *lua, int count,
                                   uint32_t output[GAME_RAY_OUTPUT_WORDS]) {
    if (damage_lua.get_top(lua) != 4 + count || damage_lua.type(lua, 5) != 1 ||
        (count != 1 && count != 6)) {
        fail("Unexpected original game raycast result arity/type\n");
    }
    output[0] = (uint32_t)count;
    output[1] = damage_lua.to_boolean(lua, 5) != 0;
    if (output[1]) {
        if (count != 6) {
            fail("Original game raycast omitted hit fields\n");
        }
        if (damage_lua.type(lua, 6) == 4) {
            unsigned length = 0;
            const char *label = damage_lua.to_string(lua, 6, &length);
            if (!label || length > 16) {
                fail("Invalid original game raycast name\n");
            }
            output[2] = 1;
            output[3] = length;
            game_raycast_pack(label, length, output + 4);
        } else if (damage_lua.type(lua, 6) != 0) {
            fail("Unexpected original game raycast name type\n");
        }
        for (int index = 0; index < 4; ++index) {
            if (damage_lua.type(lua, 7 + index) != 3) {
                fail("Unexpected original game raycast numeric type\n");
            }
            FloatBits number = {.number = damage_lua.to_number(lua, 7 + index)};
            output[8 + index] = number.bits;
        }
    } else if (count != 1) {
        fail("Original game raycast supplied missing-hit fields\n");
    }
}

#if defined(AB_ORACLE_BIRD_IMPACT_WORLD_ONLY)
#include "bird_impact_world_probe.h"
#endif

/* Original allocators, GameObjects, owned strings, map nodes, fixtures and Lua
 * wrapper execute unchanged. Only the enclosing GameLua fields are controlled
 * inputs. No renderable, joint, teleporter or live script object is attached. */
static void probe_game_raycast(void) {
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*state_construct)(void *) =
        (void (*)(void *))find_function("_ZN3lua8LuaStateC1Ev");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    void (*release)(void *) =
        (void (*)(void *))find_function("_ZN4lang6Object7releaseEv");
    void (*construct_world)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN7b2WorldC1ERK6b2Vec2");
    void (*destroy_world)(void *) = (void (*)(void *))find_function("_ZN7b2WorldD1Ev");
    void *(*create_body)(void *, const void *) =
        (void *(*)(void *, const void *))find_function(
            "_ZN7b2World10CreateBodyEPK9b2BodyDef");
    void *(*create_fixture)(void *, const void *) =
        (void *(*)(void *, const void *))find_function(
            "_ZN6b2Body13CreateFixtureEPK12b2FixtureDef");
    void (*set_box)(void *, float, float) = (void (*)(
        void *, float, float))find_function("_ZN14b2PolygonShape8SetAsBoxEff");
    void (*object_construct)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN10GameObjectC1EP7b2World");
    void **(*insert)(void *, const char **) =
        (void **(*)(void *, const char **))find_function(
            "_ZNSt3mapISsP10GameObjectSt4lessISsESaISt4pairIKSsS1_EEEixERS5_");
    void (*set_rolling)(void *, const char **, const char **) =
        (void (*)(void *, const char **, const char **))find_function(
            "_ZN7GameLua15setRollingSoundESsSs");
    void (*remove_object)(void *, const char **) =
        (void (*)(void *, const char **))find_function("_ZN7GameLua12removeObjectESs");
    int (*raycast)(void *, void *) =
        (int (*)(void *, void *))find_function("_ZN7GameLua7raycastEPN3lua8LuaStateE");
    const uint32_t *circle_vtable =
        (const uint32_t *)find_symbol("_ZTV13b2CircleShape", 1);
    const uint32_t *polygon_vtable =
        (const uint32_t *)find_symbol("_ZTV14b2PolygonShape", 1);
    initialize_damage_lua();
    uint32_t *wrapper = allocate(20);
    if (!wrapper) {
        fail("Cannot allocate original game raycast Lua wrapper\n");
    }
    state_construct(wrapper);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    static uint32_t game[0x1a00 / 4];
    static uint64_t storage[16384];
    uint32_t *world = (uint32_t *)storage;
#if defined(AB_ORACLE_BIRD_IMPACT_WORLD_ONLY)
    load_prediction_graph(lua);
    impact_world_native_game = game;
    impact_world_native_wrapper = wrapper;
    impact_world_native_cast = raycast;
#if defined(AB_ORACLE_BIRD_IMPACT_WORLD_NUMERIC)
    semihost(4, "AB_ANDROID_BIRD_IMPACT_WORLD_NUMERIC_V1 32 64 12 4\n");
#else
    semihost(4, "AB_ANDROID_BIRD_IMPACT_WORLD_V1 32 64 12 4\n");
#endif
#elif defined(AB_ORACLE_GAME_RAYCAST_NUMERIC)
    semihost(4, "AB_ANDROID_GAME_RAYCAST_NUMERIC_V1 32 64 12\n");
#else
    semihost(4, "AB_ANDROID_GAME_RAYCAST_V1 32 64 12\n");
#endif
    for (uint32_t scene = 0; scene < 32; ++scene) {
        for (size_t index = 0; index < sizeof(game) / sizeof(game[0]); ++index) {
            game[index] = 0;
        }
        const uint32_t maps[] = {0x1790, 0x17a8, 0x17c0};
        for (uint32_t index = 0; index < 3; ++index) {
            uint32_t *header = game + maps[index] / 4;
            header[2] = header[3] = (uint32_t)(uintptr_t)header;
        }
        const uint32_t gravity[2] = {0};
        construct_world(world, gravity);
        game[0x1784 / 4] = (uint32_t)(uintptr_t)world;
        for (uint32_t index = 0; index < WORLD_RAY_BODIES; ++index) {
            uint32_t row[WORLD_RAY_BODY_WORDS];
            world_raycast_body(scene, index, row);
            print_words(row, WORLD_RAY_BODY_WORDS);
            uint32_t *object = allocate(200);
            if (!object) {
                fail("Cannot allocate original game raycast object\n");
            }
            object_construct(object, world);
            object[0] = 0;
            uint32_t definition[13] = {0};
            definition[0] = row[1];
            definition[1] = row[2];
            definition[2] = row[3];
            definition[3] = row[4];
            definition[9] = UINT32_C(0x101);
            definition[10] = row[5];
            definition[11] = (uint32_t)(uintptr_t)object;
            definition[12] = UINT32_C(0x3f800000);
            void *body = create_body(world, definition);
            if (!body) {
                fail("Cannot create original game raycast body\n");
            }
            object[11] = (uint32_t)(uintptr_t)body;
            struct {
                uint32_t length, capacity, references;
                char bytes[16];
            } key = {5, 5, UINT32_MAX, {0}}, name = {0, 0, UINT32_MAX, {0}};
            for (uint32_t offset = 0; offset < 5; ++offset) {
                key.bytes[offset] = game_raycast_key(index)[offset];
            }
            GameRayName text = game_raycast_name(index);
            name.length = name.capacity = text.size;
            for (uint32_t offset = 0; offset < text.size; ++offset) {
                name.bytes[offset] = text.data[offset];
            }
            const char *identifier = key.bytes, *label = name.bytes;
            *insert(game + 0x17a4 / 4, &identifier) = object;
            set_rolling(game, &identifier, &label);
            uint32_t empty = object[32];
            object[32] = object[23];
            object[23] = empty;
            uint32_t shape[38] = {0};
            shape[0] =
                (uint32_t)(uintptr_t)((row[6] ? polygon_vtable : circle_vtable) + 2);
            shape[1] = row[6];
            shape[2] = row[7];
            if (row[6]) {
                FloatBits width = {.bits = row[8]}, height = {.bits = row[9]};
                set_box(shape, width.number, height.number);
            } else {
                shape[3] = row[8];
                shape[4] = row[9];
            }
            uint32_t fixture_definition[] = {
                (uint32_t)(uintptr_t)shape,
                (scene + index) % 5u ? (uint32_t)(uintptr_t)object : 0,
                UINT32_C(0x3e4ccccd),
                0,
                0,
                row[10] | row[11] << 16,
                row[12]};
            if (!create_fixture(body, fixture_definition)) {
                fail("Cannot create original game raycast fixture\n");
            }
        }
        if (scene & 16u) {
            world[0x191d4 / 4] |= 2u;
        }
        for (uint32_t query = 0; query < 64; ++query) {
            uint32_t input[WORLD_RAY_INPUT_WORDS];
            game_raycast_case(scene, query, input);
            input[6] = UINT32_C(0x3f800000);
            input[7] = 0;
            print_words(input, WORLD_RAY_INPUT_WORDS);
            damage_lua.set_top(lua, 0);
            for (uint32_t index = 2; index < 6; ++index) {
                FloatBits number = {.bits = input[index]};
                damage_lua.push_number(lua, number.number);
            }
            int count = raycast(game, wrapper);
            uint32_t output[GAME_RAY_OUTPUT_WORDS] = {0};
            game_ray_native_result(lua, count, output);
            print_words(output, GAME_RAY_OUTPUT_WORDS);
        }
#if defined(AB_ORACLE_BIRD_IMPACT_WORLD_ONLY)
        probe_bird_impact_world_queries(lua, scene);
#endif
        damage_lua.set_top(lua, 0);
        world[0x191d4 / 4] &= ~2u;
        for (uint32_t index = 0; index < WORLD_RAY_BODIES; ++index) {
            struct {
                uint32_t length, capacity, references;
                char bytes[8];
            } key = {5, 5, UINT32_MAX, {0}};
            for (uint32_t offset = 0; offset < 5; ++offset) {
                key.bytes[offset] = game_raycast_key(index)[offset];
            }
            const char *identifier = key.bytes;
            remove_object(game, &identifier);
        }
        uint32_t remaining[] = {game[0x17b8 / 4], world[0x19230 / 4]};
        print_words(remaining, 2);
        if (remaining[0] || remaining[1]) {
            fail("Original game raycast fixture cleanup failed\n");
        }
        destroy_world(world);
    }
    release(wrapper);
#if defined(AB_ORACLE_BIRD_IMPACT_WORLD_NUMERIC)
    semihost(4, "AB bird impact world numeric reference complete\n");
#elif defined(AB_ORACLE_BIRD_IMPACT_WORLD_ONLY)
    semihost(4, "AB bird impact world reference complete\n");
#elif defined(AB_ORACLE_GAME_RAYCAST_NUMERIC)
    semihost(4, "AB game raycast numeric reference complete\n");
#else
    semihost(4, "AB game raycast reference complete\n");
#endif
}

#endif
