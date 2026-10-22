#ifndef AB_ORACLE_BIRD_EFFECTS_H
#define AB_ORACLE_BIRD_EFFECTS_H

#include "bird_notify_probe.h"
#include "bird_collision_frame.h"
#include "../../tests/bird_effects_cases.h"

static uint32_t bird_effect_input[24], bird_effect_events[6][36], bird_effect_count;
static uint32_t bird_effect_name_calls;
static int bird_effect_audio_first(void *lua);
static int bird_effect_audio_second(void *lua);

static void bird_effect_push(void *lua, uint32_t kind) {
    if (kind == 4) {
        damage_lua.push_string(lua, "sound");
    } else if (kind == 5) {
        damage_lua.get_field(lua, -10002, "_ab_effect_value");
    } else {
        bird_notify_scalar(lua, kind);
    }
}

static void bird_effect_value(void *lua, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, -1) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        output[1] = damage_named(lua, -1, "changed") ? 2u : 1u;
        if (output[1] == 1 && !damage_named(lua, -1, "sound")) {
            fail("Unknown original bird effect value string\n");
        }
    } else if (output[0] == 5) {
        damage_lua.get_field(lua, -1, "_id");
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, -2);
        if (output[1] != 201 && output[1] != 202) {
            fail("Unknown original bird effect value table\n");
        }
    } else if (output[0] != 0) {
        fail("Unexpected original bird effect value type\n");
    }
}

static void bird_effect_field(void *lua, const char *object, const char *field,
                              uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, object);
    damage_lua.get_field(lua, top + 1, field);
    bird_effect_value(lua, output);
    damage_lua.set_top(lua, top);
}

static uint32_t bird_effect_audio_id(void *lua) {
    if (damage_lua.type(lua, -1) == 0) {
        return 0;
    }
    const uint32_t *closure = damage_lua.to_pointer(lua, -1);
    if (!closure || ((const uint8_t *)closure)[6] != 1) {
        fail("Unexpected original bird effect audio closure\n");
    }
    if (closure[4] == (uintptr_t)bird_effect_audio_first) {
        return 1;
    }
    if (closure[4] == (uintptr_t)bird_effect_audio_second) {
        return 2;
    }
    fail("Unknown original bird effect audio binding\n");
    return 0;
}

static void bird_effect_snapshot(void *lua, uint32_t output[22]) {
    const char *const objects[] = {
        "_ab_effect_bird", "_ab_effect_bird", "_ab_effect_bird",     "_ab_effect_def1",
        "_ab_effect_def2", "_ab_effect_def2", "_ab_effect_particles"};
    const char *const fields[] = {"mass",           "xVel",           "yVel",
                                  "collisionSound", "collisionSound", "damageSound",
                                  "collision"};
    for (size_t field = 0; field < 7; ++field) {
        bird_effect_field(lua, objects[field], fields[field], output + field * 2u);
    }
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_effect_bird");
    damage_lua.get_field(lua, top + 1, "gameObjectScript");
    output[14] = damage_lua.type(lua, -1) == 0 ? 0u : 2u;
    if (output[14]) {
        damage_lua.get_field(lua, top + 2, "get");
        if (damage_lua.type(lua, -1) == 6) {
            output[14] = 1;
        }
    }
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "birds");
    damage_lua.get_field(lua, top + 2, "first");
    output[15] = 0;
    if (damage_lua.type(lua, -1) == 5) {
        damage_lua.get_field(lua, top + 3, "_id");
        output[15] = (uint32_t)damage_lua.to_number(lua, -1);
    }
    damage_lua.set_top(lua, top + 1);
    damage_lua.get_field(lua, top + 1, "playAudio");
    output[16] = bird_effect_audio_id(lua);
    damage_lua.set_top(lua, top);
    bird_effect_field(lua, "second", "sensor", output + 17);
    output[19] = bird_effect_name_calls;
    damage_lua.get_field(lua, -10002, "_ab_effect_property");
    bird_effect_value(lua, output + 20);
    damage_lua.set_top(lua, top);
}

static uint32_t *bird_effect_record(void *lua, uint32_t kind, uint32_t pc,
                                    uint32_t variant, uint32_t argc) {
    if (bird_effect_count >= 6 || (uint32_t)damage_lua.get_top(lua) != argc) {
        fail("Original bird effect arity/capacity mismatch\n");
    }
    uint32_t *event = bird_effect_events[bird_effect_count++];
    for (size_t field = 0; field < 36; ++field) {
        event[field] = 0;
    }
    event[0] = kind;
    event[1] = pc;
    event[2] = variant;
    event[3] = argc;
    bird_effect_snapshot(lua, event + 14);
    return event;
}

static int bird_effect_definition(void *lua) {
    uint32_t pc = bird_collision_caller_pc(lua);
    if (pc != 44 && pc != 48) {
        fail("Unexpected original bird effect definition instruction\n");
    }
    damage_lua.get_field(lua, -10002, pc == 44 ? "_ab_effect_def1" : "_ab_effect_def2");
    return 1;
}

static void bird_effect_mass(void *lua, uint32_t bits) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_effect_bird");
    damage_number(lua, top + 1, "mass", bits);
    damage_lua.set_top(lua, top);
}

static int bird_effect_audio_name(void *lua) {
    uint32_t pc = bird_collision_caller_pc(lua);
    if (pc != 883 && pc != 942) {
        fail("Unexpected original bird effect audio-name instruction\n");
    }
    uint32_t *event = bird_effect_record(lua, 1, 0, 0, 1);
    bird_effect_value(lua, event + 4);
    if (!bird_effect_name_calls++) {
        uint32_t mutation = bird_effect_input[15];
        if (mutation & 1u) {
            damage_lua.get_field(lua, -10002, "gamelua");
            damage_function(lua, 2, "playAudio", bird_effect_audio_second);
            damage_lua.set_top(lua, 1);
        }
        if (mutation & 2u) {
            bird_effect_mass(lua, bird_effect_input[22]);
        }
        if (mutation & 4u) {
            damage_lua.get_field(lua, -10002, "_ab_effect_def1");
            damage_lua.push_string(lua, "changed");
            damage_lua.set_field(lua, 2, "collisionSound");
            damage_lua.set_top(lua, 1);
        }
        if (mutation & 8u) {
            damage_lua.get_field(lua, -10002, "_ab_effect_bird");
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, 2, "gameObjectScript");
            damage_lua.set_top(lua, 1);
        }
        if (mutation & 16u) {
            damage_lua.get_field(lua, -10002, "gamelua");
            damage_lua.get_field(lua, 2, "birds");
            damage_lua.get_field(lua, -10002, "_ab_effect_alternate");
            damage_lua.set_field(lua, 3, "first");
            damage_lua.set_top(lua, 1);
        }
        if (mutation & 512u) {
            damage_lua.get_field(lua, -10002, "second");
            damage_lua.push_boolean(lua, 1);
            damage_lua.set_field(lua, 2, "sensor");
            damage_lua.set_top(lua, 1);
        }
    }
    bird_effect_push(lua, bird_effect_input[14]);
    return 1;
}

static int bird_effect_audio(void *lua, uint32_t variant) {
    uint32_t pc = bird_collision_caller_pc(lua);
    if (pc != 887 && pc != 946) {
        fail("Unexpected original bird effect audio instruction\n");
    }
    uint32_t *event = bird_effect_record(lua, 2, 0, variant, 4);
    for (int argument = 1; argument <= 4; ++argument) {
        int top = damage_lua.get_top(lua);
        void (*push_value)(void *, int) =
            (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
        push_value(lua, argument);
        bird_effect_value(lua, event + 4 + (size_t)(argument - 1) * 2u);
        damage_lua.set_top(lua, top);
    }
    return 0;
}

static int bird_effect_audio_first(void *lua) {
    return bird_effect_audio(lua, 1);
}

static int bird_effect_audio_second(void *lua) {
    return bird_effect_audio(lua, 2);
}

static int bird_effect_property(void *lua) {
    if (bird_collision_caller_pc(lua) != 896 || !damage_named(lua, 2, "particles")) {
        fail("Unexpected original bird effect particle-property boundary\n");
    }
    uint32_t *event = bird_effect_record(lua, 3, 896, 0, 2);
    damage_lua.get_field(lua, 1, "_id");
    event[4] = (uint32_t)damage_lua.to_number(lua, -1);
    if (event[4] != 301) {
        fail("Original bird effect particle query lost receiver\n");
    }
    damage_lua.set_top(lua, 2);
    if (bird_effect_input[15] & 32u) {
        bird_effect_mass(lua, bird_effect_input[22]);
    }
    if (bird_effect_input[15] & 64u) {
        damage_lua.get_field(lua, -10002, "_ab_effect_particles");
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 3, "collision");
        damage_lua.set_top(lua, 2);
    }
    damage_lua.get_field(lua, -10002, "_ab_effect_property");
    return 1;
}

static int bird_effect_particles(void *lua) {
    if (bird_collision_caller_pc(lua) != 906 || !damage_named(lua, 1, "first")) {
        fail("Unexpected original bird contact-particle boundary\n");
    }
    uint32_t *event = bird_effect_record(lua, 4, 906, 0, 4);
    event[4] = 1;
    void (*push_value)(void *, int) =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    push_value(lua, 2);
    bird_effect_value(lua, event + 5);
    damage_lua.set_top(lua, 4);
    for (int argument = 3; argument <= 4; ++argument) {
        if (damage_lua.type(lua, argument) != 1 ||
            damage_lua.to_boolean(lua, argument)) {
            fail("Original bird contact-particle flag must be false\n");
        }
    }
    if (bird_effect_input[15] & 128u) {
        damage_lua.get_field(lua, -10002, "_ab_effect_def1");
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 5, "collisionSound");
        damage_lua.set_top(lua, 4);
    }
    if (bird_effect_input[15] & 256u) {
        bird_effect_mass(lua, 0);
    }
    return 0;
}

static int bird_effect_inert(void *lua) {
    (void)lua;
    return 0;
}

static int bird_effect_score(void *lua) {
    damage_lua.push_number(lua, 0);
    return 1;
}

static int bird_effect_damage(void *lua) {
    if (bird_collision_caller_pc(lua) != 959) {
        fail("Unexpected original bird effect suffix boundary\n");
    }
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "old2");
    damage_lua.set_field(lua, 3, "sprite");
    return 1;
}

static void bird_effect_prepare(void *lua) {
    for (size_t field = 0; field < 16; ++field) {
        bird_notify_input[field] = 0;
    }
    bird_notify_count = 0;
    bird_notify_prepare(lua);
    for (uint32_t id = 0; id < 2; ++id) {
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_number(lua, (float)id + 201.0f);
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.set_field(lua, -10002,
                             id ? "_ab_effect_particles" : "_ab_effect_value");
    }
    damage_lua.get_field(lua, -10002, "_ab_effect_particles");
    bird_effect_push(lua, bird_effect_input[10]);
    damage_lua.set_field(lua, 1, "collision");
    damage_lua.set_top(lua, 0);
    if (bird_effect_input[9] == 5) {
        damage_lua.get_field(lua, -10002, "_ab_effect_particles");
    } else {
        bird_effect_push(lua, bird_effect_input[9]);
    }
    damage_lua.set_field(lua, -10002, "_ab_effect_property");
    for (uint32_t id = 0; id < 2; ++id) {
        damage_lua.create_table(lua, 0, 3);
        bird_effect_push(lua, bird_effect_input[id ? 12 : 11]);
        damage_lua.set_field(lua, 1, "collisionSound");
        if (id) {
            bird_effect_push(lua, bird_effect_input[13]);
            damage_lua.set_field(lua, 1, "damageSound");
            uint32_t material = bird_effect_input[4];
            if (material >= 3) {
                const char *const names[] = {"immovable", "staticGround", "OTHER"};
                damage_lua.push_string(lua, names[material - 3u]);
            } else {
                bird_effect_push(lua, material);
            }
            damage_lua.set_field(lua, 1, "materialName");
        }
        damage_lua.set_field(lua, -10002, id ? "_ab_effect_def2" : "_ab_effect_def1");
    }
    damage_lua.create_table(lua, 0, 5);
    damage_lua.push_number(lua, 1);
    damage_lua.set_field(lua, 1, "_id");
    const char *const fields[] = {"xVel", "yVel", "mass"};
    for (size_t field = 0; field < 3; ++field) {
        if (bird_effect_input[5 + field] == 2) {
            FloatBits number = {.bits = bird_effect_input[field < 2 ? 19 + field : 18]};
            damage_lua.push_number(lua, number.number);
        } else {
            bird_effect_push(lua, bird_effect_input[5 + field]);
        }
        damage_lua.set_field(lua, 1, fields[field]);
    }
    if (bird_effect_input[8]) {
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_number(lua, 301);
        damage_lua.set_field(lua, 2, "_id");
        if (bird_effect_input[8] == 1) {
            damage_function(lua, 2, "get", bird_effect_property);
        }
        damage_lua.set_field(lua, 1, "gameObjectScript");
    }
    damage_lua.set_field(lua, -10002, "_ab_effect_bird");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_number(lua, 2);
    damage_lua.set_field(lua, 1, "_id");
    damage_lua.set_field(lua, -10002, "_ab_effect_alternate");
    damage_lua.get_field(lua, -10002, "second");
    bird_effect_push(lua, bird_effect_input[1]);
    damage_lua.set_field(lua, 1, "sensor");
    damage_lua.push_string(lua, "old2");
    damage_lua.set_field(lua, 1, "damageSprite");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_function(lua, 1, "getObjectDefinition", bird_effect_definition);
    damage_function(lua, 1, "getAudioName", bird_effect_audio_name);
    damage_function(lua, 1, "addParticlesNew", bird_effect_particles);
    damage_function(lua, 1, "getDamageSprite", bird_effect_damage);
    damage_function(lua, 1, "g_get_score_to_add_for_damaging", bird_effect_score);
    damage_function(lua, 1, "addFloatingScore", bird_effect_inert);
    damage_number(lua, 1, "floatingScoreScaling", UINT32_C(0x3f800000));
    damage_number(lua, 1, "collisionParticleForceThreshold", bird_effect_input[21]);
    if (bird_effect_input[23]) {
        damage_function(lua, 1, "playAudio", bird_effect_audio_first);
    }
    damage_lua.get_field(lua, -10002, "_ab_effect_length");
    damage_lua.set_field(lua, 1, "vLength");
    const char *const empty[] = {"g_birdHats", "achievementProcessor",
                                 "collisionScores"};
    for (size_t field = 0; field < 3; ++field) {
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, 1, empty[field]);
    }
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_number(lua, 3, "score", 0);
    damage_lua.set_field(lua, 2, "first");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 3, "collisionScores");
    damage_lua.set_field(lua, 2, "blocks");
    damage_lua.set_field(lua, 1, "scoreTable");
    damage_lua.create_table(lua, 0, 1);
    if (bird_effect_input[3]) {
        damage_lua.get_field(lua, -10002, "_ab_effect_bird");
        damage_lua.set_field(lua, 2, "first");
    }
    damage_lua.set_field(lua, 1, "birds");
    damage_lua.create_table(lua, 1, 0);
    if (!bird_effect_input[2]) {
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_string(lua, "first");
        damage_lua.set_field(lua, 3, "name");
        void (*set_i)(void *, int, int) =
            (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
        set_i(lua, 2, 1);
    }
    damage_lua.set_field(lua, 1, "flyingGrenades");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 2, "blocks");
    damage_lua.set_field(lua, 1, "blockTable");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "insert", bird_effect_inert);
    damage_lua.set_field(lua, -10002, "table");
}

static void probe_bird_effects(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original bird effects state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    char length_chunk[116];
    int file = system_call(
        5,
        (int)(uintptr_t)"/data/local/tmp/ab520/camera/touch-utils-vLength-oracle.lua",
        NULL, 0);
    if (file < 0 ||
        system_call(3, file, length_chunk, sizeof(length_chunk)) !=
            sizeof(length_chunk) ||
        system_call(6, file, NULL, 0) != 0 ||
        damage_lua.load(lua, length_chunk, sizeof(length_chunk), "original-vLength")) {
        fail("Cannot load original vLength utility\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_effect_length");
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/collision-callbacks.lua");
    semihost(4, "AB_ANDROID_BIRD_EFFECTS_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        bird_effects_case(sample, bird_effect_input);
        print_words(bird_effect_input, 24);
        bird_effect_count = bird_effect_name_calls = 0;
        bird_effect_prepare(lua);
        damage_lua.get_field(lua, -10002, "CollisionCallbacks");
        damage_lua.get_field(lua, 1, "birdCollision");
        damage_lua.push_string(lua, "first");
        damage_lua.push_string(lua, "second");
        for (size_t field = 16; field < 18; ++field) {
            FloatBits value = {.bits = bird_effect_input[field]};
            damage_lua.push_number(lua, value.number);
        }
        damage_lua.push_number(lua, 0);
        damage_lua.push_number(lua, 0);
        damage_lua.push_nil(lua);
        damage_lua.push_nil(lua);
        uint32_t failed = damage_lua.call(lua, 8, 0, 0) != 0;
        damage_lua.set_top(lua, 0);
        print_words(&failed, 1);
        uint32_t state[22];
        bird_effect_snapshot(lua, state);
        print_words(state, 22);
        print_words(&bird_effect_count, 1);
        for (uint32_t event = 0; event < bird_effect_count; ++event) {
            print_words(bird_effect_events[event], 36);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird effects reference complete\n");
}

#endif
