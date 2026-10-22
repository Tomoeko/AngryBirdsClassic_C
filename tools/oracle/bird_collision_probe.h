#ifndef AB_ORACLE_BIRD_COLLISION_H
#define AB_ORACLE_BIRD_COLLISION_H

#include "bird_notify_probe.h"
#include "bird_collision_frame.h"

static uint32_t bird_full_flags, bird_full_counts[8], bird_full_trace[32];
static uint32_t bird_full_trace_count, bird_full_appends, bird_full_append_bits;
static void (*bird_full_push_value)(void *, int);
static void (*bird_full_set_i)(void *, int, int);
static const char *const bird_full_objects[] = {"first", "second", "third", "hat"};
static const char *const bird_full_strings[] = {"normal", "special", "damaged", "blink",
                                                "smile"};

static void bird_full_mark(uint32_t kind) {
    if (bird_full_trace_count >= 32) {
        fail("Original coupled bird trace capacity exceeded\n");
    }
    bird_full_trace[bird_full_trace_count++] = kind;
}

static void bird_full_value(void *lua, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, -1) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t id = 0; id < 5; ++id) {
            if (damage_named(lua, -1, bird_full_strings[id])) {
                output[1] = id + 1u;
                return;
            }
        }
        fail("Unexpected original coupled bird state string\n");
    } else if (output[0] != 0) {
        fail("Unexpected original coupled bird state type\n");
    }
}

static void bird_full_snapshot(void *lua, uint32_t output[128]) {
    int top = damage_lua.get_top(lua);
    const char *const fields[] = {
        "shot",        "hasCollided", "collision", "boomerangActive", "explode",
        "bombTimer",   "defence",     "strength",  "sprite",          "damageSprite",
        "blinkSprite", "smileSprite", "sensor",    "controllable"};
    for (size_t id = 0; id < 4; ++id) {
        damage_lua.get_field(lua, -10002, bird_full_objects[id]);
        for (size_t field = 0; field < 14; ++field) {
            damage_lua.get_field(lua, top + 1, fields[field]);
            bird_full_value(lua, output + id * 28u + field * 2u);
            damage_lua.set_top(lua, top + 1);
        }
        damage_lua.set_top(lua, top);
    }
    for (size_t count = 0; count < 8; ++count) {
        output[112 + count] = bird_full_counts[count];
    }
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "scoreTable");
    damage_lua.get_field(lua, top + 2, "first");
    output[120] = damage_lua.type(lua, -1) == 5;
    output[121] = 0;
    if (output[120]) {
        damage_lua.get_field(lua, top + 3, "score");
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[121] = value.bits;
    }
    output[122] = bird_full_appends;
    output[123] = bird_full_append_bits;
    damage_lua.set_top(lua, top + 1);
    damage_lua.get_field(lua, top + 1, "birdSpecialtyAvailable");
    bird_full_value(lua, output + 124);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "objects");
    damage_lua.get_field(lua, top + 2, "world");
    damage_lua.get_field(lua, top + 3, "second");
    damage_lua.get_field(lua, top + 4, "_id");
    output[126] = (uint32_t)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "_ab_full_definitions");
    damage_lua.get_field(lua, top + 1, "second");
    damage_lua.get_field(lua, top + 2, "_id");
    output[127] = (uint32_t)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, top);
}

static int bird_full_definition(void *lua) {
    uint32_t pc = bird_collision_caller_pc(lua);
    if (damage_lua.get_top(lua) != 1 ||
        (pc != 44 && pc != 48 && pc != 378 && pc != 444 && pc != 522 && pc != 747 &&
         pc != 762)) {
        fail("Unexpected original coupled bird definition boundary\n");
    }
    bird_full_mark(1);
    const char *name = damage_lua.to_string(lua, 1, NULL);
    if (!name) {
        fail("Original coupled bird definition name is not a string\n");
    }
    damage_lua.get_field(lua, -10002, "_ab_full_definitions");
    damage_lua.get_field(lua, 2, name);
    return 1;
}

static int bird_full_notification(void *lua) {
    uint32_t pc = bird_collision_caller_pc(lua);
    uint32_t side = bird_full_counts[0];
    if (side >= 2 || pc != (side ? 106u : 79u) || damage_lua.get_top(lua) != 7 ||
        !damage_named(lua, 2, "collision")) {
        fail("Unexpected original coupled bird notification boundary\n");
    }
    damage_lua.get_field(lua, 1, "_id");
    if (damage_lua.to_number(lua, -1) != (float)side + 1.0f) {
        fail("Original notification lost cached contact identity\n");
    }
    damage_lua.set_top(lua, 7);
    bird_full_mark(2);
    ++bird_full_counts[0];
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, 8, "objects");
    damage_lua.get_field(lua, 9, "world");
    damage_lua.get_field(lua, -10002, "third");
    damage_lua.set_field(lua, 10, "second");
    damage_lua.set_top(lua, 7);
    damage_lua.get_field(lua, -10002, "_ab_full_definitions");
    damage_lua.get_field(lua, -10002, "_ab_full_def3");
    damage_lua.set_field(lua, 8, "second");
    return 0;
}

static int bird_full_score(void *lua) {
    if (bird_collision_caller_pc(lua) != 213 || damage_lua.get_top(lua) != 1 ||
        damage_lua.to_number(lua, 1) != 11.0f) {
        fail("Unexpected original coupled bird score boundary\n");
    }
    bird_full_mark(3);
    damage_lua.push_number(lua, 17);
    return 1;
}

static int bird_full_floating(void *lua) {
    if (bird_collision_caller_pc(lua) != 235 || damage_lua.get_top(lua) != 1 ||
        damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original coupled bird floating-score boundary\n");
    }
    bird_full_mark(4);
    return 0;
}

static int bird_full_insert(void *lua) {
    if (bird_collision_caller_pc(lua) != 265 || damage_lua.get_top(lua) != 2 ||
        damage_lua.type(lua, 1) != 5 || damage_lua.to_number(lua, 2) != 17.0f ||
        bird_full_appends) {
        fail("Unexpected original coupled bird score append\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, 2)};
    bird_full_append_bits = value.bits;
    bird_full_push_value(lua, 2);
    bird_full_set_i(lua, 1, (int)++bird_full_appends);
    return 0;
}

static int bird_full_cooldown(void *lua) {
    if (bird_collision_caller_pc(lua) != 289 || damage_lua.get_top(lua) != 1 ||
        damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original coupled bird cooldown boundary\n");
    }
    bird_full_mark(5);
    ++bird_full_counts[7];
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, 2, "birdSpecialtyAvailable");
    if (!damage_lua.to_boolean(lua, -1)) {
        fail("Original coupled cooldown lost canonical availability\n");
    }
    damage_lua.set_top(lua, 2);
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, 2, "birdSpecialtyAvailable");
    return 0;
}

static int bird_full_mighty(void *lua) {
    if (bird_collision_caller_pc(lua) != 345 || damage_lua.get_top(lua) != 2 ||
        !damage_named(lua, 1, "first") || !damage_named(lua, 2, "second")) {
        fail("Unexpected original coupled mighty-bird boundary\n");
    }
    bird_full_mark(6);
    ++bird_full_counts[3];
    return 0;
}

static int bird_full_sprite(void *lua) {
    uint32_t pc = bird_collision_caller_pc(lua);
    if ((pc != 433 && pc != 499 && pc != 678) || damage_lua.get_top(lua) != 2 ||
        !damage_named(lua, 1, pc == 499 ? "second" : "first") ||
        !damage_named(lua, 2, pc == 678 ? "special" : "normal")) {
        fail("Unexpected original coupled bird sprite boundary\n");
    }
    bird_full_mark(7);
    ++bird_full_counts[1];
    return 0;
}

static int bird_full_release(void *lua) {
    if (bird_collision_caller_pc(lua) != 736 || damage_lua.get_top(lua) != 1 ||
        !damage_named(lua, 1, "first")) {
        fail("Unexpected original coupled bird hat-release boundary\n");
    }
    bird_full_mark(8);
    ++bird_full_counts[2];
    if (bird_full_flags & 32u) {
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 2, "getObjectDefinition");
    }
    return 0;
}

static int bird_full_audio_name(void *lua) {
    uint32_t pc = bird_collision_caller_pc(lua);
    if ((pc != 883 && pc != 942) || damage_lua.get_top(lua) != 1 ||
        !damage_named(lua, 1, bird_full_counts[4] ? "bird" : "damage")) {
        fail("Original coupled bird sound lost cached definition\n");
    }
    bird_full_mark(9);
    bird_full_push_value(lua, 1);
    return 1;
}

static int bird_full_audio(void *lua) {
    uint32_t pc = bird_collision_caller_pc(lua);
    FloatBits volume = {.number = damage_lua.to_number(lua, 2)};
    if ((pc != 887 && pc != 946) || damage_lua.get_top(lua) != 4 ||
        volume.bits != UINT32_C(0x3f333333) || damage_lua.type(lua, 3) != 1 ||
        damage_lua.to_boolean(lua, 3) || damage_lua.to_number(lua, 4) != 2.0f) {
        fail("Unexpected original coupled bird audio request\n");
    }
    bird_full_mark(10);
    ++bird_full_counts[4];
    return 0;
}

static int bird_full_damage(void *lua) {
    if (bird_collision_caller_pc(lua) != 959 || damage_lua.get_top(lua) != 2) {
        fail("Unexpected original coupled bird damage-sprite boundary\n");
    }
    damage_lua.get_field(lua, 1, "_id");
    if (damage_lua.to_number(lua, -1) != 2.0f) {
        fail("Original bird damage refresh lost cached second object\n");
    }
    damage_lua.set_top(lua, 2);
    bird_full_mark(11);
    ++bird_full_counts[5];
    damage_lua.create_table(lua, 0, 3);
    const char *const fields[] = {"sprite", "blink", "smile"};
    const char *const values[] = {"damaged", "blink", "smile"};
    for (size_t field = 0; field < 3; ++field) {
        damage_lua.push_string(lua, values[field]);
        damage_lua.set_field(lua, 3, fields[field]);
    }
    return 1;
}

static int bird_full_transform(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.to_number(lua, 1) != 0 ||
        damage_lua.to_number(lua, 2) != 0) {
        fail("Unexpected original coupled bird particle position\n");
    }
    damage_lua.push_number(lua, 0);
    damage_lua.push_number(lua, 0);
    return 2;
}

static int bird_full_particles(void *lua) {
    if (damage_lua.get_top(lua) != 9 || !damage_named(lua, 1, "dust") ||
        damage_lua.to_number(lua, 2) != 1.0f) {
        fail("Unexpected original coupled bird damage particles\n");
    }
    for (int argument = 3; argument <= 7; ++argument) {
        if (damage_lua.type(lua, argument) != 3 ||
            damage_lua.to_number(lua, argument) != 0) {
            fail("Original coupled particle coordinate/zero argument mismatch\n");
        }
    }
    for (int argument = 8; argument <= 9; ++argument) {
        if (damage_lua.type(lua, argument) != 1 ||
            damage_lua.to_boolean(lua, argument)) {
            fail("Original coupled particle flag mismatch\n");
        }
    }
    bird_full_mark(12);
    ++bird_full_counts[6];
    return 0;
}

static void bird_full_prepare(void *lua) {
    for (size_t field = 0; field < 16; ++field) {
        bird_notify_input[field] = 0;
    }
    bird_notify_count = 0;
    bird_notify_prepare(lua);
    for (uint32_t id = 2; id < 4; ++id) {
        damage_lua.create_table(lua, 0, 4);
        damage_lua.push_number(lua, (float)id + 1.0f);
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.push_string(lua, id == 2 ? "second" : "hat");
        damage_lua.set_field(lua, 1, "name");
        if (id == 2) {
            damage_lua.push_boolean(lua, 1);
            damage_lua.set_field(lua, 1, "sensor");
        }
        damage_lua.set_field(lua, -10002, bird_full_objects[id]);
    }
    for (size_t id = 0; id < 2; ++id) {
        damage_lua.get_field(lua, -10002, bird_full_objects[id]);
        damage_lua.push_boolean(lua, 1);
        damage_lua.set_field(lua, 1, "shot");
        if (bird_full_flags & 128u) {
            damage_lua.push_boolean(lua, 1);
            damage_lua.set_field(lua, 1, "hasCollided");
        }
        if (id == 0) {
            damage_lua.push_boolean(lua, 1);
            damage_lua.set_field(lua, 1, "controllable");
            damage_lua.push_boolean(lua, 1);
            damage_lua.set_field(lua, 1, "isMightyBait");
            damage_function(lua, 1, "collisionFunction", bird_full_mighty);
            damage_number(lua, 1, "x", UINT32_C(0x40000000));
            damage_number(lua, 1, "y", UINT32_C(0x40400000));
            if (bird_full_flags & 1u) {
                damage_lua.push_boolean(lua, 1);
                damage_lua.set_field(lua, 1, "projectileFeather");
            }
        } else {
            damage_lua.push_number(lua, 0);
            damage_lua.set_field(lua, 1, "controllable");
            damage_lua.push_boolean(lua, 1);
            damage_lua.set_field(lua, 1, "boomerangActive");
            damage_lua.push_string(lua, "Object");
            damage_lua.set_field(lua, 1, "definition");
            damage_lua.push_number(lua, 0);
            damage_lua.set_field(lua, 1, "x");
            damage_lua.push_number(lua, 0);
            damage_lua.set_field(lua, 1, "y");
            if (bird_full_flags & 4u) {
                damage_lua.push_boolean(lua, 1);
                damage_lua.set_field(lua, 1, "sensor");
            }
        }
        damage_lua.set_top(lua, 0);
    }
    const char *const definitions[] = {"_ab_full_def1", "_ab_full_def2",
                                       "_ab_full_def3"};
    for (uint32_t id = 0; id < 3; ++id) {
        damage_lua.create_table(lua, 0, 6);
        damage_lua.push_number(lua, (float)id + 1.0f);
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.push_string(lua, id == 0 ? "BOMB" : id == 1 ? "OTHER" : "BOOMERANG");
        damage_lua.set_field(lua, 1, "specialty");
        damage_lua.push_string(lua, id == 0   ? "bird"
                                    : id == 1 ? "target"
                                              : "replacement");
        damage_lua.set_field(lua, 1, "collisionSound");
        if (id) {
            damage_lua.push_string(lua, id == 1 ? "damage" : "replacement");
            damage_lua.set_field(lua, 1, "damageSound");
        }
        if (id != 1) {
            damage_lua.push_string(lua, "normal");
            damage_lua.set_field(lua, 1, "spriteCollision");
        }
        if (!id) {
            damage_lua.push_string(lua, "special");
            damage_lua.set_field(lua, 1, "spriteSpecial1");
        }
        damage_lua.set_field(lua, -10002, definitions[id]);
    }
    damage_lua.create_table(lua, 0, 2);
    damage_lua.get_field(lua, -10002, definitions[0]);
    damage_lua.set_field(lua, 1, "first");
    damage_lua.get_field(lua, -10002, definitions[1]);
    damage_lua.set_field(lua, 1, "second");
    damage_lua.set_field(lua, -10002, "_ab_full_definitions");
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_function(lua, 1, "getObjectDefinition", bird_full_definition);
    damage_function(lua, 1, "g_get_score_to_add_for_damaging", bird_full_score);
    damage_function(lua, 1, "addFloatingScore", bird_full_floating);
    damage_function(lua, 1, "setSprite", bird_full_sprite);
    damage_function(lua, 1, "releaseHat", bird_full_release);
    damage_function(lua, 1, "getAudioName", bird_full_audio_name);
    damage_function(lua, 1, "playAudio", bird_full_audio);
    damage_function(lua, 1, "getDamageSprite", bird_full_damage);
    damage_function(lua, 1, "physicsToWorldTransform", bird_full_transform);
    damage_number(lua, 1, "floatingScoreScaling", UINT32_C(0x3f800000));
    damage_number(lua, 1, "collisionParticleForceThreshold", 0);
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 1, "g_deltaBasedSlingshot");
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 1, "birdSpecialtyAvailable");
    damage_lua.get_field(lua, -10002, "first");
    damage_lua.set_field(lua, 1, "flyingBird");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "activateCooldown", bird_full_cooldown);
    damage_lua.set_field(lua, 1, "slingshotSystem");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "achievementProcessor");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "birds");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, bird_full_flags & 2u ? "second" : "hat");
    damage_lua.set_field(lua, 3, "hat");
    damage_lua.set_field(lua, 2, "first");
    damage_lua.set_field(lua, 1, "g_birdHats");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 3, "collisionScores");
    damage_lua.set_field(lua, 2, "blocks");
    damage_lua.set_field(lua, 1, "scoreTable");
    if (!(bird_full_flags & 64u)) {
        damage_lua.create_table(lua, 2, 0);
        int count = 0;
        for (size_t id = 0; id < 2; ++id) {
            if (bird_full_flags & (8u << id)) {
                damage_lua.get_field(lua, -10002, bird_full_objects[id]);
                bird_full_set_i(lua, 2, ++count);
            }
        }
        damage_lua.set_field(lua, 1, "flyingGrenades");
    }
    damage_lua.get_field(lua, 1, "objects");
    damage_lua.get_field(lua, 2, "world");
    damage_lua.get_field(lua, -10002, "hat");
    damage_lua.set_field(lua, 3, "hat");
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "blocks");
    damage_lua.get_field(lua, 2, "BlockScriptManager");
    damage_function(lua, 3, "triggerEvent", bird_full_notification);
    damage_lua.set_top(lua, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_string(lua, "dust");
    damage_lua.set_field(lua, 5, "name");
    damage_lua.push_number(lua, 1);
    damage_lua.set_field(lua, 5, "amount");
    damage_lua.set_field(lua, 4, "damageStateChangedParticles");
    damage_lua.set_field(lua, 3, "Object");
    damage_lua.set_field(lua, 2, "blocks");
    damage_lua.set_field(lua, 1, "blockTable");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "insert", bird_full_insert);
    damage_lua.set_field(lua, -10002, "table");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "addParticles", bird_full_particles);
    damage_lua.set_field(lua, -10002, "particles");
}

static int bird_full_contains(const char *message, const char *wanted) {
    for (size_t start = 0; message && message[start]; ++start) {
        size_t offset = 0;
        while (wanted[offset] && message[start + offset] &&
               wanted[offset] == message[start + offset]) {
            ++offset;
        }
        if (!wanted[offset]) {
            return 1;
        }
    }
    return 0;
}

static void probe_bird_collision(void) {
    initialize_damage_lua();
    bird_full_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    bird_full_set_i =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original coupled bird state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/collision-callbacks.lua");
    semihost(4, "AB_ANDROID_BIRD_COLLISION_V1 256\n");
    for (bird_full_flags = 0; bird_full_flags < 256; ++bird_full_flags) {
        for (size_t count = 0; count < 8; ++count) {
            bird_full_counts[count] = 0;
        }
        bird_full_trace_count = bird_full_appends = bird_full_append_bits = 0;
        print_words(&bird_full_flags, 1);
        bird_full_prepare(lua);
        damage_lua.get_field(lua, -10002, "CollisionCallbacks");
        damage_lua.get_field(lua, 1, "birdCollision");
        damage_lua.push_string(lua, "first");
        damage_lua.push_string(lua, "second");
        damage_lua.push_number(lua, 5);
        damage_lua.push_number(lua, 11);
        damage_lua.push_number(lua, 1);
        damage_lua.push_number(lua, 0);
        damage_lua.push_nil(lua);
        damage_lua.push_nil(lua);
        uint32_t failed = damage_lua.call(lua, 8, 0, 0) != 0;
        uint32_t expected_failure =
            !(bird_full_flags & 3u) &&
            (((bird_full_flags & 32u) && !(bird_full_flags & 4u)) ||
             (bird_full_flags & 64u));
        if (failed != expected_failure) {
            const char *message = failed ? damage_lua.to_string(lua, -1, NULL) : NULL;
            fail(message ? message : "Unexpected original coupled bird success\n");
        }
        if (failed) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            const char *kind =
                (bird_full_flags & 32u) && !(bird_full_flags & 4u) ? "call" : "length";
            if (!bird_full_contains(message, kind)) {
                fail(message ? message : "Missing coupled bird error diagnostic\n");
            }
        }
        damage_lua.set_top(lua, 0);
        print_words(&failed, 1);
        uint32_t state[128];
        bird_full_snapshot(lua, state);
        print_words(state, 128);
        print_words(&bird_full_trace_count, 1);
        print_words(bird_full_trace, bird_full_trace_count);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird collision reference complete\n");
}

#endif
