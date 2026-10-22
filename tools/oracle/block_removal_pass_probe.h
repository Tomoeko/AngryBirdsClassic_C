#ifndef AB_ORACLE_BLOCK_REMOVAL_PASS_H
#define AB_ORACLE_BLOCK_REMOVAL_PASS_H

#include "../../tests/block_removal_pass_cases.h"
#include "block_removal_particles_probe.h"

static uint32_t removal_pass_input[5], removal_pass_output[48];
static uint32_t removal_pass_game[0x1a00 / 4];

static void removal_pass_event(void *lua, uint32_t kind, uint32_t pc, int arguments) {
    uint32_t count = removal_pass_output[1];
    if (count >= 24 || removal_particles_pc(lua) != pc ||
        damage_lua.get_top(lua) != arguments) {
        fail("Unexpected complete original removal boundary\n");
    }
    removal_pass_output[2 + count] = kind;
    removal_pass_output[1] = count + 1u;
}

static void removal_pass_boolean(void *lua, const char *field, int value) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
    damage_lua.push_boolean(lua, value);
    damage_lua.set_field(lua, top + 1, field);
    damage_lua.set_top(lua, top);
}

static int removal_pass_flag(void *lua, const char *field) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
    damage_lua.get_field(lua, top + 1, field);
    int value = damage_lua.to_boolean(lua, top + 2);
    damage_lua.set_top(lua, top);
    return value != 0;
}

static int removal_pass_destroy(void *lua) {
    removal_pass_event(lua, 1, 45, 1);
    if (removal_pass_input[1] & 32u) {
        removal_pass_boolean(lua, "levelGoal", !removal_pass_flag(lua, "levelGoal"));
    }
    return 0;
}

static int removal_pass_definition(void *lua) {
    removal_pass_event(lua, 2, 48, 1);
    if (!damage_named(lua, 1, "queued")) {
        fail("Complete original removal lost captured definition key\n");
    }
    damage_lua.get_field(lua, -10002, "_ab_particles_definition");
    return 1;
}

static int removal_pass_score(void *lua) {
    removal_pass_event(lua, 3, 51, 1);
    damage_lua.push_number(lua, removal_pass_flag(lua, "levelGoal") ? 5000 : 500);
    return 1;
}

static int removal_pass_floating(void *lua) {
    uint32_t pc = removal_particles_pc(lua);
    if (pc != 82 && pc != 123) {
        fail("Unexpected complete original floating branch\n");
    }
    removal_pass_event(lua, 4, pc, 1);
    const char *const fields[] = {"score", "lifetime", "maxScale"};
    for (uint32_t index = 0; index < 3; ++index) {
        damage_lua.get_field(lua, 1, fields[index]);
        if (damage_lua.type(lua, -1) != 3) {
            fail("Complete original floating score lost numeric metadata\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        removal_pass_output[44 + index] = value.bits;
        damage_lua.set_top(lua, 1);
    }
    if (removal_pass_input[1] & 16u) {
        removal_particles_set(lua, removal_particles_tables[0], "name", 4, "renamed",
                              3);
        damage_lua.get_field(lua, -10002, "scoreTable");
        damage_lua.get_field(lua, 2, "blocks");
        damage_number(lua, 3, "score", UINT32_C(0x42c80000));
        damage_lua.set_top(lua, 1);
    }
    return 0;
}

static int removal_pass_property(void *lua) {
    uint32_t pc = removal_particles_pc(lua);
    uint32_t kind = pc == 164 ? 5u : pc == 173 ? 7u : pc == 253 ? 11u : 12u;
    if (pc != 164 && pc != 173 && pc != 253 && pc != 261) {
        fail("Unexpected complete original property query\n");
    }
    removal_pass_event(lua, kind, pc, 2);
    const char *field = pc == 173   ? "particleAmount"
                        : pc == 253 ? "useNewParticleTable"
                                    : "particles";
    if (!damage_named(lua, 2, field)) {
        fail("Unexpected complete original property key\n");
    }
    if (pc == 173) {
        damage_lua.push_number(lua, 0);
    } else if (pc == 253) {
        damage_lua.push_boolean(lua, (removal_pass_input[1] & 2u) != 0);
    } else {
        damage_lua.get_field(lua, -10002, removal_particles_tables[2]);
    }
    return 1;
}

static int removal_pass_sound(void *lua) {
    removal_pass_event(lua, 6, 169, 3);
    damage_lua.push_nil(lua);
    return 1;
}

static int removal_pass_random(void *lua) {
    removal_pass_event(lua, 8, 182, 2);
    if (damage_lua.to_number(lua, 1) != 12 || damage_lua.to_number(lua, 2) != 20) {
        fail("Complete original removal changed random bounds\n");
    }
    damage_lua.push_number(lua, 17);
    return 1;
}

static int removal_pass_update(void *lua) {
    removal_pass_event(lua, 9, 230, 1);
    return 0;
}

static int removal_pass_notify(void *lua) {
    removal_pass_event(lua, 10, 233, 1);
    if (removal_pass_input[1] & 64u) {
        removal_pass_boolean(lua, "killSilently",
                             !removal_pass_flag(lua, "killSilently"));
        removal_pass_boolean(lua, "isGiftBox", !removal_pass_flag(lua, "isGiftBox"));
    }
    return 0;
}

static int removal_pass_new_particles(void *lua) {
    removal_pass_event(lua, 13, 265, 4);
    if (!damage_named(lua, 1, "queued") || damage_lua.type(lua, 2) != 1 ||
        damage_lua.to_boolean(lua, 2) || damage_lua.to_boolean(lua, 3) ||
        damage_lua.to_boolean(lua, 4)) {
        fail("Unexpected complete original new particles\n");
    }
    return 0;
}

static int removal_pass_old_particles(void *lua) {
    removal_pass_event(lua, 14, 273, 5);
    damage_lua.get_field(lua, 2, "_id");
    if (!damage_named(lua, 1, "queued") || damage_lua.to_number(lua, 6) != 203 ||
        damage_lua.type(lua, 3) != 3 || damage_lua.to_number(lua, 3) != 0 ||
        damage_lua.to_boolean(lua, 4) || damage_lua.to_boolean(lua, 5)) {
        fail("Unexpected complete original legacy particles\n");
    }
    damage_lua.set_top(lua, 5);
    return 0;
}

static int removal_pass_remove(void *lua) {
    removal_pass_event(lua, 15, 288, 1);
    if (!damage_named(lua, 1, "queued")) {
        fail("Complete original removal lost physical key\n");
    }
    void (*remove_object)(void *, const char **) =
        (void (*)(void *, const char **))find_function("_ZN7GameLua12removeObjectESs");
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } name = {6, 6, UINT32_MAX, "queued"};
    const char *key = name.bytes;
    remove_object(removal_pass_game, &key);
    return 0;
}

static int removal_pass_pig(void *lua) {
    removal_pass_event(lua, 16, 466, 1);
    if (!damage_named(lua, 1, "queued")) {
        fail("Complete original pig removal lost map key\n");
    }
    if (removal_pass_input[1] & 128u) {
        removal_pass_boolean(lua, "isGiftBox", 1);
        removal_particles_set(lua, NULL, "levelName", 4, "renamed", 3);
        damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
        damage_number(lua, 2, "x", UINT32_C(0x40400000));
        damage_number(lua, 2, "y", UINT32_C(0x40800000));
        damage_lua.set_top(lua, 1);
    }
    return 0;
}

static int removal_pass_gift(void *lua) {
    removal_pass_event(lua, 17, 475, 4);
    int changed = (removal_pass_input[1] & 128u) && removal_pass_flag(lua, "levelGoal");
    if (!damage_named(lua, 2, changed ? "renamed" : "level") ||
        damage_lua.to_number(lua, 3) != (changed ? 3 : 1) ||
        damage_lua.to_number(lua, 4) != (changed ? 4 : -2)) {
        fail("Complete original gift did not read current fields\n");
    }
    return 0;
}

static void removal_pass_prepare(void *lua) {
    for (uint32_t index = 0; index < 17; ++index) {
        removal_particles_input[index] = 0;
    }
    removal_particles_input[9] = 1;
    removal_particles_input[10] = 15;
    removal_particles_prepare(lua);
    removal_pass_boolean(lua, "levelGoal", (removal_pass_input[1] & 4u) != 0);
    removal_pass_boolean(lua, "isGiftBox", (removal_pass_input[1] & 8u) != 0);
    removal_pass_boolean(lua, "killSilently", (removal_pass_input[1] & 1u) != 0);
    damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
    damage_number(lua, 1, "x", UINT32_C(0x3f800000));
    damage_number(lua, 1, "y", UINT32_C(0xc0000000));
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "objects");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
    damage_lua.set_field(lua, 2, "queued");
    damage_lua.set_field(lua, 1, "world");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
    damage_lua.set_field(lua, 2, "queued");
    damage_lua.set_field(lua, 1, "dynamic");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 1, "queued");
    damage_lua.set_field(lua, -10002, "levelGoals");
    removal_particles_set(lua, NULL, "levelName", 4, "level", 3);
    damage_lua.get_field(lua, -10002, "_ab_particles_script");
    damage_function(lua, 1, "onDestroy", removal_pass_destroy);
    damage_function(lua, 1, "get", removal_pass_property);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "g_levelEffect");
    damage_function(lua, 1, "destroyedSound", removal_pass_sound);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "math");
    damage_function(lua, 1, "random", removal_pass_random);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "collectGiftBox", removal_pass_gift);
    damage_lua.set_field(lua, -10002, "GiftBoxManager");
    const char *const names[] = {
        "getObjectDefinition", "g_get_score_to_add_for_breaking",
        "addFloatingScore",    "updateDestroyedBlocksValues",
        "addParticlesNew",     "addParticles",
        "removeObject",        "removePig"};
    int (*const functions[])(void *) = {
        removal_pass_definition,    removal_pass_score,
        removal_pass_floating,      removal_pass_update,
        removal_pass_new_particles, removal_pass_old_particles,
        removal_pass_remove,        removal_pass_pig};
    for (uint32_t index = 0; index < 8; ++index) {
        damage_function(lua, -10002, names[index], functions[index]);
    }
}

static void removal_pass_snapshot(void *lua, const uint32_t *second) {
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "scoreTable");
    damage_lua.get_field(lua, 1, "blocks");
    damage_lua.get_field(lua, 2, "score");
    FloatBits total = {.number = damage_lua.to_number(lua, 3)};
    removal_pass_output[26] = total.bits;
    damage_lua.set_top(lua, 2);
    damage_lua.get_field(lua, 2, "individualDestructionScores");
    removal_particles_raw_get(lua, 3, 1);
    FloatBits first = {.number = damage_lua.to_number(lua, 4)};
    removal_pass_output[27] = damage_lua.type(lua, 4) == 3;
    removal_pass_output[28] = first.bits;
    removal_particles_raw_get(lua, 3, 2);
    if (damage_lua.type(lua, 5) != 0) {
        fail("Complete original removal appended more than once\n");
    }
    damage_lua.set_top(lua, 0);
    removal_pass_output[29] = (uint32_t)removal_pass_flag(lua, "levelGoal");
    removal_pass_output[30] = (uint32_t)removal_pass_flag(lua, "isGiftBox");
    removal_pass_output[31] = (uint32_t)removal_pass_flag(lua, "killSilently");
    damage_lua.get_field(lua, -10002, removal_particles_tables[0]);
    damage_lua.get_field(lua, 1, "name");
    removal_pass_output[32] = damage_named(lua, 2, "renamed") != 0;
    damage_lua.set_top(lua, 0);
    const char *const maps[] = {"deadBlocks", "world", "dynamic", "levelGoals"};
    for (uint32_t index = 0; index < 4; ++index) {
        if (index == 1 || index == 2) {
            damage_lua.get_field(lua, -10002, "objects");
            damage_lua.get_field(lua, 1, maps[index]);
        } else {
            damage_lua.get_field(lua, -10002, maps[index]);
        }
        damage_lua.get_field(lua, -1, "queued");
        removal_pass_output[33 + index] = damage_lua.type(lua, -1) != 0;
        damage_lua.set_top(lua, 0);
    }
    uint32_t *game = removal_pass_game;
    const uint32_t *world = (const uint32_t *)(uintptr_t)game[0x1784 / 4];
    removal_pass_output[37] = game[0x17b8 / 4];
    removal_pass_output[38] = world[0x19230 / 4];
    removal_pass_output[39] = world[0x19234 / 4];
    removal_pass_output[40] = (game[0x17e4 / 4] - game[0x17e0 / 4]) / 20;
    removal_pass_output[41] = (game[0x17f0 / 4] - game[0x17ec / 4]) / 20;
    removal_pass_output[42] = ((const uint8_t *)second)[0xa6];
    removal_pass_output[43] = second[0xa8 / 4];
}

static void probe_block_removal_pass(void) {
    typedef void (*Construct)(void *);
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    Construct state_construct = (Construct)find_function("_ZN3lua8LuaStateC1Ev");
    Construct claim = (Construct)find_function("_ZN4lang6Object5claimEv");
    Construct release = (Construct)find_function("_ZN4lang6Object7releaseEv");
    void (*table_construct)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    Construct table_destroy = (Construct)find_function("_ZN3lua8LuaTableD1Ev");
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
    void (*remove_object)(void *, const char **) =
        (void (*)(void *, const char **))find_function("_ZN7GameLua12removeObjectESs");
    int (*create_joint)(void *, void *) = (int (*)(void *, void *))find_function(
        "_ZN7GameLua14createJointLuaEPN3lua8LuaStateE");
    initialize_damage_lua();
    uint32_t *wrapper = allocate(20);
    if (!wrapper) {
        fail("Cannot allocate complete original removal Lua state\n");
    }
    state_construct(wrapper);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    removal_particles_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    removal_particles_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    int file = system_call(
        5, (int)(uintptr_t)"/data/local/tmp/ab520/collision/remove-blocks.lua", NULL,
        0);
    if (file < 0 ||
        system_call(3, file, removal_particles_chunk,
                    sizeof(removal_particles_chunk)) !=
            sizeof(removal_particles_chunk) ||
        system_call(6, file, NULL, 0)) {
        fail("Cannot read complete original removeBlocks\n");
    }
    damage_function(lua, -10002, "_ab_particles_notification", removal_pass_notify);
    uint32_t *game = removal_pass_game;
    static uint64_t world_storage[16384];
    static uint32_t records[1][5], glue[1][5];
    static const struct {
        uint32_t length, capacity, references;
        char bytes[8];
    } names[] = {{6, 6, UINT32_MAX, "queued"}, {6, 6, UINT32_MAX, "second"}};
    const uint32_t gravity[2] = {0, 0};
    const uint32_t body_definition[13] = {
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
    game[0x17e8 / 4] = (uint32_t)(uintptr_t)(records + 1);
    game[0x17ec / 4] = game[0x17f0 / 4] = (uint32_t)(uintptr_t)glue;
    game[0x17f4 / 4] = (uint32_t)(uintptr_t)(glue + 1);
    semihost(4, "AB_ANDROID_BLOCK_REMOVAL_PASS_V1 512\n");
    for (uint32_t sample = 0; sample < 512; ++sample) {
        block_removal_pass_case(sample, removal_pass_input);
        print_words(removal_pass_input, 5);
        for (uint32_t word = 0; word < 48; ++word) {
            removal_pass_output[word] = 0;
        }
        uint32_t *objects[2];
        for (uint32_t index = 0; index < 2; ++index) {
            const char *key = names[index].bytes;
            uint32_t *object = allocate(200);
            if (!object) {
                fail("Cannot allocate complete original removal object\n");
            }
            object_construct(object, world);
            object[0] = 0;
            object[11] = (uint32_t)(uintptr_t)create_body(world, body_definition);
            *insert(game + 0x17a4 / 4, &key) = object;
            set_rolling(game, &key, &key);
            uint32_t empty = object[32];
            object[32] = object[23];
            object[23] = empty;
            ((uint8_t *)object)[0xa6] = 0;
            object[0xa8 / 4] = UINT32_C(0x3e000000);
            objects[index] = object;
        }
        reference((uint8_t *)game + 0x17f8);
        damage_lua.create_table(lua, 0, 1);
        damage_lua.set_field(lua, 1, "joints");
        damage_lua.set_top(lua, 0);
        for (uint32_t argument = 0; argument < 9; ++argument) {
            damage_lua.push_number(lua, 0);
        }
        damage_lua.create_table(lua, 0, 8);
        const char *const joint_fields[] = {"name", "end1", "end2"};
        const char *joint_values[] = {"joint", names[removal_pass_input[4]].bytes,
                                      names[1u - removal_pass_input[4]].bytes};
        for (uint32_t field = 0; field < 3; ++field) {
            damage_lua.push_string(lua, joint_values[field]);
            damage_lua.set_field(lua, 10, joint_fields[field]);
        }
        damage_lua.push_number(lua, (float)removal_pass_input[3]);
        damage_lua.set_field(lua, 10, "type");
        const char *const anchors[] = {"x1", "y1", "x2", "y2"};
        for (uint32_t anchor = 0; anchor < 4; ++anchor) {
            damage_number(lua, 10, anchors[anchor], 0);
        }
        damage_number(lua, 10, "destroyTimer", UINT32_C(0x3f400000));
        if (create_joint(game, wrapper)) {
            fail("Cannot create complete original removal joint\n");
        }
        damage_lua.set_top(lua, 0);
        removal_pass_prepare(lua);
        world[0x191d4 / 4] |= removal_pass_input[2] ? 2u : 0u;
        damage_lua.get_field(lua, -10002, "_ab_particles_removeBlocks");
        int status = damage_lua.call(lua, 0, 0, 0);
        if (status) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message && !removal_particles_contains(message, "unexpected")
                     ? message
                     : "Unexpected complete original removal error\n");
        }
        removal_pass_snapshot(lua, objects[1]);
        print_words(removal_pass_output, 48);
        world[0x191d4 / 4] &= ~UINT32_C(2);
        if (game[0x17e4 / 4] != game[0x17e0 / 4] ||
            game[0x17f0 / 4] != game[0x17ec / 4]) {
            fail("Complete original removal retained a connected joint record\n");
        }
        const char *key = names[1].bytes;
        remove_object(game, &key);
        while (world[0x19228 / 4]) {
            destroy_body(world, (void *)(uintptr_t)world[0x19228 / 4]);
        }
        damage_lua.set_top(lua, 0);
    }
    table_destroy((uint8_t *)game + 0x17f8);
    world_destroy(world);
    release(wrapper);
    semihost(4, "AB block removal pass reference complete\n");
}

#endif
