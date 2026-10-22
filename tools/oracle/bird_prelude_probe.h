#ifndef AB_ORACLE_BIRD_PRELUDE_H
#define AB_ORACLE_BIRD_PRELUDE_H

#include "bird_notify_probe.h"
#include "../../tests/bird_prelude_cases.h"

static uint32_t bird_prelude_input[21], bird_prelude_events[8][24];
static uint32_t bird_prelude_count, bird_prelude_appends, bird_prelude_cutoff;
static uint32_t bird_prelude_append_bits;
static int (*bird_prelude_set_metatable)(void *, int);
static void (*bird_prelude_set_index)(void *, int, int);

static void bird_prelude_state(void *lua, uint32_t output[24]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    int game = top + 1;
    damage_lua.get_field(lua, game, "scoreTable");
    damage_lua.get_field(lua, -1, "first");
    output[12] = damage_lua.type(lua, -1) == 5;
    output[13] = 0;
    if (output[12]) {
        damage_lua.get_field(lua, -1, "score");
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[13] = value.bits;
    }
    damage_lua.set_top(lua, game);
    damage_lua.get_field(lua, -10002, "_ab_hat");
    damage_lua.get_field(lua, -1, "defence");
    FloatBits defence = {.number = damage_lua.to_number(lua, -1)};
    output[14] = defence.bits;
    damage_lua.set_top(lua, game + 1);
    damage_lua.get_field(lua, -1, "strength");
    FloatBits strength = {.number = damage_lua.to_number(lua, -1)};
    output[15] = strength.bits;
    damage_lua.set_top(lua, game);
    damage_lua.get_field(lua, game, "g_deltaBasedSlingshot");
    bird_notify_value(lua, output + 16);
    damage_lua.set_top(lua, game);
    damage_lua.get_field(lua, game, "birdSpecialtyAvailable");
    bird_notify_value(lua, output + 18);
    damage_lua.set_top(lua, game);
    for (uint32_t id = 0; id < 2; ++id) {
        damage_lua.get_field(lua, -10002, bird_notify_names[id]);
        damage_lua.get_field(lua, -1, "shot");
        output[20 + id * 2] = damage_lua.to_boolean(lua, -1) != 0;
        damage_lua.set_top(lua, game + 1);
        damage_lua.get_field(lua, -1, "hasCollided");
        output[21 + id * 2] = damage_lua.to_boolean(lua, -1) != 0;
        damage_lua.set_top(lua, game);
    }
    damage_lua.set_top(lua, top);
}

static uint32_t *bird_prelude_record(void *lua, uint32_t kind) {
    if (bird_prelude_count >= 8) {
        fail("Original bird prelude event capacity exceeded\n");
    }
    uint32_t *event = bird_prelude_events[bird_prelude_count++];
    for (size_t field = 0; field < 24; ++field) {
        event[field] = 0;
    }
    event[0] = kind;
    bird_prelude_state(lua, event);
    return event;
}

static int bird_prelude_score(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 3) {
        fail("Original bird prelude score query mismatch\n");
    }
    uint32_t *event = bird_prelude_record(lua, 1);
    FloatBits damage = {.number = damage_lua.to_number(lua, 1)};
    event[1] = damage.bits;
    if (bird_prelude_input[9] & 1u) {
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_number(lua, damage_lua.get_top(lua), "floatingScoreScaling",
                      UINT32_C(0x40000000));
    }
    if (bird_prelude_input[9] & 2u) {
        damage_lua.get_field(lua, -10002, "first");
        int object = damage_lua.get_top(lua);
        damage_number(lua, object, "x", UINT32_C(0x80000000));
        damage_number(lua, object, "y", UINT32_C(0x41000000));
    }
    FloatBits value = {.bits = bird_prelude_input[11]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

static uint32_t bird_prelude_hash(const char *value, unsigned length) {
    uint32_t hash = UINT32_C(2166136261);
    for (unsigned index = 0; index < length; ++index) {
        hash = (hash ^ (uint8_t)value[index]) * UINT32_C(16777619);
    }
    return hash;
}

static int bird_prelude_floating(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Original bird floating score arity mismatch\n");
    }
    uint32_t *event = bird_prelude_record(lua, 2);
    static const char *const fields[] = {"x",        "y",        "score", "time",
                                         "lifetime", "maxScale", "xs"};
    for (size_t field = 0; field < 7; ++field) {
        damage_lua.get_field(lua, 1, fields[field]);
        if (damage_lua.type(lua, -1) != 3) {
            fail("Original bird floating score field is not numeric\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        event[1 + field] = value.bits;
        damage_lua.set_top(lua, 1);
    }
    damage_lua.get_field(lua, 1, "text");
    unsigned length = 0;
    const char *text = damage_lua.to_string(lua, -1, &length);
    if (!text || damage_lua.type(lua, -1) != 4) {
        fail("Original bird floating score text mismatch\n");
    }
    event[8] = length;
    event[9] = bird_prelude_hash(text, length);
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, -10002, "gamelua");
    if (bird_prelude_input[9] & 4u) {
        damage_lua.get_field(lua, 2, "scoreTable");
        damage_lua.create_table(lua, 0, 1);
        damage_number(lua, 4, "score", UINT32_C(0x42c80000));
        damage_lua.set_field(lua, 3, "first");
        damage_lua.set_top(lua, 2);
    }
    if (bird_prelude_input[9] & 8u) {
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 2, "g_deltaBasedSlingshot");
    }
    if (bird_prelude_input[9] & 16u) {
        for (size_t id = 0; id < 2; ++id) {
            damage_lua.get_field(lua, -10002, bird_notify_names[id]);
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 3, "hasCollided");
            damage_lua.set_top(lua, 2);
        }
    }
    if (bird_prelude_input[9] & 32u) {
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 2, "birdSpecialtyAvailable");
    }
    return 0;
}

static int bird_prelude_cooldown(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Original bird cooldown receiver mismatch\n");
    }
    (void)bird_prelude_record(lua, 3);
    if (bird_prelude_input[9] & 64u) {
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, -1, "scoreTable");
        damage_lua.get_field(lua, -1, "first");
        if (damage_lua.type(lua, -1) == 5) {
            damage_number(lua, damage_lua.get_top(lua), "score", UINT32_C(0xc0a00000));
        }
    }
    if (bird_prelude_input[9] & 128u) {
        damage_lua.get_field(lua, -10002, "first");
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, -2, "shot");
    }
    return 0;
}

static int bird_prelude_achievement(void *lua) {
    if (damage_lua.get_top(lua) != 3 || damage_lua.type(lua, 1) != 5) {
        fail("Original bird achievement arity mismatch\n");
    }
    uint32_t *event = bird_prelude_record(lua, 4);
    event[1] = bird_notify_id(lua, 2);
    event[2] = bird_notify_id(lua, 3);
    return 0;
}

static int bird_prelude_boundary(void *lua) {
    if (damage_named(lua, 2, "_ab_cutoff")) {
        ++bird_prelude_cutoff;
        (void)bird_prelude_record(lua, 5);
    }
    /* Return nil; the original next instruction indexes isMightyBait, causing
     * an original VM error after this C callback has returned. Never unwind an
     * original Lua error across authored C frames. This bounds the comparison
     * before flying-bird handling without modifying original instructions. */
    damage_lua.push_nil(lua);
    return 1;
}

static int bird_prelude_insert(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 5 ||
        damage_lua.type(lua, 2) != 3 || bird_prelude_appends != 0) {
        fail("Original bird collision score append mismatch\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, 2)};
    bird_prelude_append_bits = value.bits;
    damage_lua.push_number(lua, value.number);
    bird_prelude_set_index(lua, 1, (int)++bird_prelude_appends);
    return 0;
}

static void bird_prelude_prepare(void *lua) {
    for (size_t word = 0; word < 16; ++word) {
        bird_notify_input[word] = 0;
    }
    bird_notify_count = 0;
    bird_notify_prepare(lua);
    for (uint32_t id = 0; id < 2; ++id) {
        damage_lua.get_field(lua, -10002, bird_notify_names[id]);
        bird_notify_scalar(lua, bird_prelude_input[2 + id]);
        damage_lua.set_field(lua, 1, "shot");
        bird_notify_scalar(lua, bird_prelude_input[4 + id]);
        damage_lua.set_field(lua, 1, "hasCollided");
        damage_number(lua, 1, "x", bird_prelude_input[id ? 18 : 14]);
        damage_number(lua, 1, "y", bird_prelude_input[id ? 19 : 15]);
        damage_lua.push_string(lua, id ? "second" : "actor_first");
        damage_lua.set_field(lua, 1, "name");
        damage_lua.set_top(lua, 0);
    }
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 1, "defence", bird_prelude_input[16]);
    damage_number(lua, 1, "strength", bird_prelude_input[17]);
    damage_lua.set_field(lua, -10002, "_ab_hat");
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, 1, "objects");
    damage_lua.get_field(lua, 2, "world");
    damage_lua.get_field(lua, -10002, "first");
    damage_lua.set_field(lua, 3, "actor_first");
    damage_lua.get_field(lua, -10002, "_ab_hat");
    damage_lua.set_field(lua, 3, "hat");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 4, "__index", bird_prelude_boundary);
    (void)bird_prelude_set_metatable(lua, 3);
    damage_lua.set_top(lua, 1);
    damage_lua.create_table(lua, 0, 2);
    uint32_t mode = bird_prelude_input[1];
    for (uint32_t id = 0; id < 2; ++id) {
        const char *name = NULL;
        if (id == 0) {
            name = mode == 1                ? "second"
                   : mode == 3 || mode == 6 ? "hat"
                   : mode == 4              ? "missing_hat"
                                            : NULL;
        } else {
            name = mode == 2 ? "actor_first" : mode == 5 || mode == 6 ? "hat" : NULL;
        }
        if (name) {
            damage_lua.create_table(lua, 0, 1);
            damage_lua.push_string(lua, name);
            damage_lua.set_field(lua, 3, "hat");
            damage_lua.set_field(lua, 2, id ? "second" : "actor_first");
        }
    }
    damage_lua.set_field(lua, 1, "g_birdHats");
    damage_function(lua, 1, "g_get_score_to_add_for_damaging", bird_prelude_score);
    damage_function(lua, 1, "addFloatingScore", bird_prelude_floating);
    damage_number(lua, 1, "floatingScoreScaling", bird_prelude_input[12]);
    bird_notify_scalar(lua, bird_prelude_input[6]);
    damage_lua.set_field(lua, 1, "g_deltaBasedSlingshot");
    bird_notify_scalar(lua, bird_prelude_input[7]);
    damage_lua.set_field(lua, 1, "birdSpecialtyAvailable");
    damage_lua.create_table(lua, 0, 2);
    if (bird_prelude_input[8]) {
        damage_lua.create_table(lua, 0, 1);
        damage_number(lua, 3, "score", bird_prelude_input[13]);
        damage_lua.set_field(lua, 2, "first");
    }
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 3, "collisionScores");
    damage_lua.set_field(lua, 2, "blocks");
    damage_lua.set_field(lua, 1, "scoreTable");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "activateCooldown", bird_prelude_cooldown);
    damage_lua.set_field(lua, 1, "slingshotSystem");
    damage_lua.create_table(lua, 0, 1);
    if (bird_prelude_input[20]) {
        damage_function(lua, 2, "collisionCheck", bird_prelude_achievement);
    }
    damage_lua.set_field(lua, 1, "achievementProcessor");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "_ab_cutoff");
    damage_lua.set_field(lua, 2, "name");
    damage_lua.set_field(lua, 1, "flyingBird");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "insert", bird_prelude_insert);
    damage_lua.set_field(lua, -10002, "table");
}

static void probe_bird_prelude(void) {
    initialize_damage_lua();
    bird_prelude_set_metatable =
        (int (*)(void *, int))find_function("_Z16lua_setmetatableP9lua_Statei");
    bird_prelude_set_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original bird prelude state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/collision-callbacks.lua");
    semihost(4, "AB_ANDROID_BIRD_PRELUDE_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        bird_prelude_case(sample, bird_prelude_input);
        print_words(bird_prelude_input, 21);
        bird_prelude_count = bird_prelude_appends = bird_prelude_cutoff = 0;
        bird_prelude_prepare(lua);
        damage_lua.get_field(lua, -10002, "CollisionCallbacks");
        damage_lua.get_field(lua, 1, "birdCollision");
        damage_lua.push_string(lua, "first");
        damage_lua.push_string(lua, "second");
        damage_lua.push_number(lua, 1.0f);
        FloatBits damage = {.bits = bird_prelude_input[10]};
        damage_lua.push_number(lua, damage.number);
        for (size_t field = 0; field < 4; ++field) {
            damage_lua.push_number(lua, 0.0f);
        }
        int status = damage_lua.call(lua, 8, 0, 0);
        uint32_t related = bird_prelude_input[1] == 1 || bird_prelude_input[1] == 2;
        if ((related && status) ||
            (!related && (!status || bird_prelude_cutoff != 1))) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Unexpected bird prelude exit\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t result[24] = {0, related};
        bird_prelude_state(lua, result);
        print_words(result, 24);
        print_words(&bird_prelude_appends, 1);
        if (bird_prelude_appends) {
            print_words(&bird_prelude_append_bits, 1);
        }
        print_words(&bird_prelude_count, 1);
        for (uint32_t event = 0; event < bird_prelude_count; ++event) {
            print_words(bird_prelude_events[event], 24);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird prelude reference complete\n");
}

#endif
