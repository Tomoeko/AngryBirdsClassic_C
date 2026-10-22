#ifndef AB_ORACLE_GAME_JOINT_H
#define AB_ORACLE_GAME_JOINT_H

#if !defined(AB_ORACLE_GAME_JOINT_NAMED_ONLY) &&                                       \
    !defined(AB_ORACLE_GAME_OBJECT_REMOVE_JOINTS_ONLY)
#include "../../tests/game_joint_cases.h"
#endif
#if defined(AB_ORACLE_GAME_JOINT_ZEROS)
#include "../../tests/game_joint_zero_cases.h"
#endif
#if defined(AB_ORACLE_GAME_JOINT_LIMITS)
#include "../../tests/game_joint_limit_cases.h"
#endif

static const char *const game_joint_number_fields[] = {
    "coordType",  "frequency",  "dampingRatio", "motorSpeed", "maxTorque",
    "lowerLimit", "upperLimit", "worldAxisX",   "worldAxisY", "breakForce"};
static const uint8_t game_joint_number_bits[] = {0, 1, 2, 6, 7, 8, 9, 11, 12, 14};
static const uint8_t game_joint_number_input[] = {3,  14, 15, 16, 17,
                                                  18, 19, 20, 21, 22};
static const char *const game_joint_boolean_fields[] = {
    "collideConnected", "motor", "limit", "backAndForth", "breakable"};
static const uint8_t game_joint_boolean_bits[] = {3, 4, 5, 10, 13};

static void game_joint_lua_input(void *lua, const uint32_t input[28]) {
    damage_lua.set_top(lua, 0);
    for (size_t index = 0; index < 9; ++index) {
        FloatBits value = {.bits = index < 5 ? 0 : input[24 + index - 5]};
        damage_lua.push_number(lua, value.number);
    }
    damage_lua.create_table(lua, 0, 24);
    damage_lua.push_string(lua, "joint");
    damage_lua.set_field(lua, -2, "name");
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, -2, "end1");
    damage_lua.push_string(lua, "second");
    damage_lua.set_field(lua, -2, "end2");
    damage_lua.push_number(lua, (float)input[1]);
    damage_lua.set_field(lua, -2, "type");
    const char *const anchors[] = {"x1", "y1", "x2", "y2"};
    for (size_t index = 0; index < 4; ++index) {
        damage_number(lua, -2, anchors[index], input[10 + index]);
    }
    for (size_t index = 0; index < 10; ++index) {
        if (input[2] & (1u << game_joint_number_bits[index])) {
            damage_number(lua, -2, game_joint_number_fields[index],
                          input[game_joint_number_input[index]]);
        }
    }
    for (size_t index = 0; index < 5; ++index) {
        if (input[2] & (1u << game_joint_boolean_bits[index])) {
            damage_lua.push_boolean(lua, (int)((input[23] >> index) & 1u));
            damage_lua.set_field(lua, -2, game_joint_boolean_fields[index]);
        }
    }
#if defined(AB_ORACLE_GAME_GLUE)
    if (input[2] & 2u) {
        damage_number(lua, -2, "destroyTimer", input[14]);
    }
#endif
}

#if !defined(AB_ORACLE_GAME_JOINT_NAMED_ONLY) &&                                       \
    !defined(AB_ORACLE_GAME_OBJECT_REMOVE_JOINTS_ONLY)
static void print_game_joint_definition(const uint32_t *joint, uint32_t type) {
    uint32_t output[20] = {type - 1, ((const uint8_t *)joint)[0x3d]};
    size_t anchors = type <= 2 ? 20 : 17;
    for (size_t field = 0; field < 4; ++field) {
        output[2 + field] = joint[anchors + field];
    }
    if (type <= 2) {
        output[6] = joint[type == 1 ? 26 : 24];
        output[7] = joint[17];
        output[8] = joint[18];
    } else {
        output[6] = joint[type == 3 ? 29 : 25];
        output[9] = joint[30];
        output[10] = joint[31];
        output[11] = joint[type == 3 ? 27 : 33];
        output[12] = joint[type == 3 ? 26 : 32];
        output[13] = ((const uint8_t *)joint)[type == 3 ? 0x64 : 0x89];
        output[14] = ((const uint8_t *)joint)[type == 3 ? 0x70 : 0x88];
        if (type == 4) {
            for (size_t field = 0; field < 4; ++field) {
                output[15 + field] = joint[21 + field];
            }
        }
    }
    print_words(output, 20);
}

static void print_game_joint_metadata(void *lua, void *root,
                                      void (*reference)(void *)) {
    const char *const fields[] = {
        "type",         "coordType",    "x1",
        "y1",           "x2",           "y2",
        "frequency",    "dampingRatio", "collideConnected",
        "motor",        "motorSpeed",   "maxTorque",
        "limit",        "lowerLimit",   "upperLimit",
        "backAndForth", "worldAxisX",   "worldAxisY",
        "breakable",    "breakForce",
#if defined(AB_ORACLE_GAME_GLUE)
        "destroyTimer",
#endif
    };
    damage_lua.set_top(lua, 0);
    reference(root);
    damage_lua.get_field(lua, -1, "joints");
    damage_lua.get_field(lua, -1, "joint");
    for (size_t index = 0; index < sizeof(fields) / sizeof(*fields); ++index) {
        damage_lua.get_field(lua, -1, fields[index]);
        uint32_t type = (uint32_t)damage_lua.type(lua, -1);
        FloatBits value = {.number = type == 1 ? 0.0f : damage_lua.to_number(lua, -1)};
        uint32_t output[] = {type, type == 1 ? (uint32_t)damage_lua.to_boolean(lua, -1)
                                             : value.bits};
        print_words(output, 2);
        damage_lua.set_top(lua, 3);
    }
    damage_lua.set_top(lua, 0);
}

#if defined(AB_ORACLE_GAME_GLUE_POLICY)
#include "game_glue_policy_probe.h"
#endif
#if defined(AB_ORACLE_GAME_JOINT_LIMITS)
#include "game_joint_limits_probe.h"
#endif
#if defined(AB_ORACLE_GAME_JOINT_PARAMETERS)
#include "game_joint_parameters_probe.h"
#endif

static void probe_game_joint(void) {
    typedef void (*Construct)(void *);
    typedef void (*TableConstruct)(void *, void *);
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    Construct state_construct = (Construct)find_function("_ZN3lua8LuaStateC1Ev");
    TableConstruct table_construct =
        (TableConstruct)find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
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
    int (*create_joint)(void *, void *) = (int (*)(void *, void *))find_function(
        "_ZN7GameLua14createJointLuaEPN3lua8LuaStateE");
    Construct destroy_record = (Construct)find_function("_ZN7GameLua9jointDataD1Ev");
#if defined(AB_ORACLE_GAME_GLUE)
    Construct create_glue = (Construct)find_function("_ZN7GameLua16createGlueJointsEv");
#endif
    initialize_damage_lua();
    uint32_t *wrapper = allocate(20);
    if (!wrapper) {
        fail("Cannot allocate native joint Lua wrapper\n");
    }
    state_construct(wrapper);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    static uint32_t game[0x1860 / 4], objects[2][50], nodes[2][6], records[2][5];
    uint32_t *record = records[0];
#if defined(AB_ORACLE_GAME_GLUE)
    record = records[1];
#endif
    static uint64_t world_storage[16384];
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } names[] = {{5, 5, UINT32_MAX, "first"}, {6, 6, UINT32_MAX, "second"}};
    const uint32_t gravity[2] = {0, 0};
    world_construct(world_storage, gravity);
    game[4] = (uint32_t)(uintptr_t)wrapper;
    game[0x1784 / 4] = (uint32_t)(uintptr_t)world_storage;
    table_construct((uint8_t *)game + 0x17f8, wrapper);
    for (size_t index = 0; index < 2; ++index) {
        nodes[index][0] = index ? 0 : 1;
        nodes[index][1] =
            (uint32_t)(uintptr_t)(index ? nodes[0]
                                        : (uint32_t *)((uint8_t *)game + 0x17a8));
        nodes[index][3] = index ? 0 : (uint32_t)(uintptr_t)nodes[1];
        nodes[index][4] = (uint32_t)(uintptr_t)names[index].bytes;
        nodes[index][5] = (uint32_t)(uintptr_t)objects[index];
    }
    game[0x17ac / 4] = game[0x17b0 / 4] = (uint32_t)(uintptr_t)nodes[0];
    game[0x17b4 / 4] = (uint32_t)(uintptr_t)nodes[1];
    game[0x17b8 / 4] = 2;
    game[0x17e0 / 4] = game[0x17e4 / 4] = (uint32_t)(uintptr_t)records[0];
    game[0x17e8 / 4] = (uint32_t)(uintptr_t)(records[0] + 5);
#if defined(AB_ORACLE_GAME_JOINT_PARAMETERS)
    game[0x17e8 / 4] = (uint32_t)(uintptr_t)(records[1] + 5);
#endif
#if defined(AB_ORACLE_GAME_GLUE)
    game[0x17ec / 4] = game[0x17f0 / 4] = (uint32_t)(uintptr_t)record;
    game[0x17f4 / 4] = (uint32_t)(uintptr_t)(record + 5);
#if defined(AB_ORACLE_GAME_GLUE_POLICY)
    semihost(4, "AB_ANDROID_GAME_GLUE_POLICY_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_GAME_GLUE_V1 4096\n");
#endif
#else
#if defined(AB_ORACLE_GAME_JOINT_ZEROS)
    semihost(4, "AB_ANDROID_GAME_JOINT_ZEROS_V1 4096\n");
#elif defined(AB_ORACLE_GAME_JOINT_LIMITS)
    semihost(4, "AB_ANDROID_GAME_JOINT_LIMITS_V1 4096\n");
#elif defined(AB_ORACLE_GAME_JOINT_PARAMETERS)
    semihost(4, "AB_ANDROID_GAME_JOINT_PARAMETERS_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_GAME_JOINT_V1 4096\n");
#endif
#endif
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[28], bodies[2];
#if defined(AB_ORACLE_GAME_JOINT_ZEROS)
        game_joint_zero_case(sample, input);
#elif defined(AB_ORACLE_GAME_JOINT_LIMITS)
        game_joint_limit_case(sample, input);
#else
        game_joint_case(sample, input);
#endif
#if defined(AB_ORACLE_GAME_GLUE)
        input[1] = 5;
#endif
        print_words(input, 28);
        for (size_t index = 0; index < 2; ++index) {
            uint32_t definition[13] = {2, input[4 + index * 3], input[5 + index * 3],
                                       input[6 + index * 3]};
            definition[9] = UINT32_C(0x101);
            definition[10] = 1;
            definition[12] = UINT32_C(0x3f800000);
            uint32_t *body = create_body(world_storage, definition);
            bodies[index] = (uint32_t)(uintptr_t)body;
            objects[index][11] = bodies[index];
            const uint32_t frame[] = {body[3], body[4], body[5], body[6], body[14]};
            print_words(frame, 5);
        }
        reference((uint8_t *)game + 0x17f8);
        damage_lua.create_table(lua, 0, 1);
        damage_lua.set_field(lua, -2, "joints");
        game_joint_lua_input(lua, input);
        int status = create_joint(game, wrapper);
        const uint32_t *world = (const uint32_t *)world_storage;
#if defined(AB_ORACLE_GAME_GLUE)
        if (status != 0 || world[0x19234 / 4] != 0 ||
            game[0x17e4 / 4] != (uint32_t)(uintptr_t)records[0] ||
            game[0x17f0 / 4] != (uint32_t)(uintptr_t)(record + 5)) {
            fail("Original deferred glue registration changed controlled counts\n");
        }
        size_t pair_count = 1;
        uint32_t pair[] = {bodies[0], bodies[1], bodies[0], bodies[1]};
#if defined(AB_ORACLE_GAME_GLUE_POLICY)
        seed_glue_policy(game, wrapper, lua, bodies, input);
        if (sample & 64u) {
            ((uint32_t *)world_storage)[0x191d4 / 4] |= 2u;
        }
        print_glue_policy(game, lua, bodies);
        pair_count += (sample >> 2) & 1u;
        if (sample & 8u) {
            pair[2] = bodies[1];
            pair[3] = bodies[0];
        }
#endif
        game[0x38 / 4] = (uint32_t)(uintptr_t)pair;
        game[0x3c / 4] = game[0x40 / 4] = (uint32_t)(uintptr_t)(pair + pair_count * 2);
        create_glue(game);
        size_t expected_count = pair_count;
#if defined(AB_ORACLE_GAME_GLUE_POLICY)
        expected_count += (sample >> 1) & 1u;
        if (sample & 64u) {
            expected_count =
                ((sample & 1u) && !(sample & 128u) ? 1u : 0u) + ((sample >> 1) & 1u);
        }
        print_glue_policy(game, lua, bodies);
        ((uint32_t *)world_storage)[0x191d4 / 4] &= ~UINT32_C(2);
#endif
        if (world[0x19234 / 4] != expected_count || game[0x38 / 4] != game[0x3c / 4]) {
            fail("Original glue pair creation did not consume its queue\n");
        }
#else
        if (status != 0 || world[0x19234 / 4] != 1 ||
            game[0x17e4 / 4] != (uint32_t)(uintptr_t)(record + 5)) {
            fail("Original game joint construction changed controlled counts\n");
        }
#endif
        const uint32_t *joint = (const uint32_t *)(uintptr_t)world[0x1922c / 4];
        if (joint) {
            print_game_joint_definition(joint,
#if defined(AB_ORACLE_GAME_GLUE)
                                        2
#else
                                        input[1]
#endif
            );
        } else {
            const uint32_t empty[20] = {0};
            print_words(empty, 20);
        }
#if defined(AB_ORACLE_GAME_JOINT_LIMITS)
        probe_joint_limit_state(game, (uint32_t *)(uintptr_t)joint, bodies, record,
                                input);
#endif
#if defined(AB_ORACLE_GAME_JOINT_PARAMETERS)
        probe_joint_parameters(game, wrapper, lua, (uint32_t *)(uintptr_t)joint, bodies,
                               records, input);
#endif
        print_game_joint_metadata(lua, (uint8_t *)game + 0x17f8, reference);
        if (joint) {
            destroy_joint(world_storage, (void *)(uintptr_t)joint);
        }
        destroy_record(record);
#if defined(AB_ORACLE_GAME_GLUE_POLICY)
        if (game[0x17e4 / 4] != (uint32_t)(uintptr_t)records[0]) {
            destroy_record(records[0]);
        }
#endif
        game[0x17e4 / 4] = (uint32_t)(uintptr_t)records[0];
#if defined(AB_ORACLE_GAME_GLUE)
        game[0x17f0 / 4] = (uint32_t)(uintptr_t)record;
#endif
        for (size_t index = 0; index < 2; ++index) {
            destroy_body(world_storage, (void *)(uintptr_t)bodies[index]);
        }
    }
    table_destroy((uint8_t *)game + 0x17f8);
    world_destroy(world_storage);
    release(wrapper);
#if defined(AB_ORACLE_GAME_GLUE)
#if defined(AB_ORACLE_GAME_GLUE_POLICY)
    semihost(4, "AB game glue policy reference complete\n");
#else
    semihost(4, "AB game glue reference complete\n");
#endif
#else
#if defined(AB_ORACLE_GAME_JOINT_ZEROS)
    semihost(4, "AB game joint zeros reference complete\n");
#elif defined(AB_ORACLE_GAME_JOINT_LIMITS)
    semihost(4, "AB game joint limits reference complete\n");
#elif defined(AB_ORACLE_GAME_JOINT_PARAMETERS)
    semihost(4, "AB game joint parameters reference complete\n");
#else
    semihost(4, "AB game joint reference complete\n");
#endif
#endif
}
#endif

#endif
