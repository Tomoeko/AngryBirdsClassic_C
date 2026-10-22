#ifndef AB_ORACLE_BIRD_MIGHTY_H
#define AB_ORACLE_BIRD_MIGHTY_H

#include "bird_notify_probe.h"
#include "../../tests/bird_mighty_cases.h"
#include "bird_collision_frame.h"

static uint32_t mighty_input[8], mighty_event[17], mighty_count, mighty_definitions;
static const char *const mighty_names[] = {"first", "second", "third", "missing",
                                           "renamed"};

static int mighty_collision_first(void *lua);
static int mighty_collision_second(void *lua);

static uint32_t mighty_name(void *lua, int index) {
    for (uint32_t id = 0; id < 5; ++id) {
        if (damage_named(lua, index, mighty_names[id])) {
            return id + 1u;
        }
    }
    fail("Unexpected original mighty-bird name\n");
    return 0;
}

static void mighty_binding(void *lua, uint32_t kind) {
    if (kind == 0) {
        damage_lua.push_nil(lua);
    } else if (kind == 1) {
        damage_lua.push_boolean(lua, 0);
    } else if (kind == 2) {
        damage_lua.push_number(lua, 0.0f);
    } else if (kind == 3) {
        damage_lua.create_table(lua, 0, 0);
    } else {
        damage_lua.push_closure(
            lua, kind == 4 ? mighty_collision_first : mighty_collision_second, 0);
    }
}

static void mighty_snapshot(void *lua, uint32_t output[14]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "flyingBird");
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 5) {
        damage_lua.get_field(lua, -1, "name");
        output[1] = mighty_name(lua, -1);
    }
    damage_lua.set_top(lua, top);
    for (size_t id = 0; id < 2; ++id) {
        damage_lua.get_field(lua, -10002, mighty_names[id]);
        damage_lua.get_field(lua, top + 1, "name");
        output[2 + id] = mighty_name(lua, -1);
        damage_lua.set_top(lua, top + 1);
        damage_lua.get_field(lua, top + 1, "sensor");
        bird_notify_value(lua, output + 4 + id * 2);
        damage_lua.set_top(lua, top);
    }
    damage_lua.get_field(lua, -10002, "_ab_mighty_metadata");
    damage_lua.get_field(lua, top + 1, "isMightyBait");
    bird_notify_value(lua, output + 8);
    damage_lua.set_top(lua, top + 1);
    damage_lua.get_field(lua, top + 1, "isMightyEagle");
    bird_notify_value(lua, output + 10);
    damage_lua.set_top(lua, top + 1);
    damage_lua.get_field(lua, top + 1, "collisionFunction");
    output[12] = (uint32_t)damage_lua.type(lua, -1);
    output[13] = 0;
    if (output[12] == 6) {
        const uint32_t *closure = damage_lua.to_pointer(lua, -1);
        if (!closure || ((const uint8_t *)closure)[6] != 1) {
            fail("Unexpected original mighty-bird function closure\n");
        }
        if (closure[4] == (uintptr_t)mighty_collision_first) {
            output[13] = 1;
        } else if (closure[4] == (uintptr_t)mighty_collision_second) {
            output[13] = 2;
        } else {
            fail("Unknown original mighty-bird function binding\n");
        }
    }
    damage_lua.set_top(lua, top);
}

static int mighty_collision(void *lua, uint32_t variant) {
    if (damage_lua.get_top(lua) != 2 || mighty_count) {
        fail("Original mighty collisionFunction arity/count mismatch\n");
    }
    mighty_count = 1;
    mighty_event[0] = variant;
    mighty_event[1] = mighty_name(lua, 1);
    mighty_event[2] = mighty_name(lua, 2);
    uint32_t pc = bird_collision_caller_pc(lua);
    if (pc != (mighty_event[1] == 1 ? 345u : 367u)) {
        fail("Original mighty collisionFunction instruction mismatch\n");
    }
    mighty_snapshot(lua, mighty_event + 3);
    if (mighty_input[7] & 1u) {
        damage_lua.get_field(lua, -10002, "first");
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 3, "sensor");
        damage_lua.set_top(lua, 2);
    }
    if (mighty_input[7] & 2u) {
        damage_lua.get_field(lua, -10002, "second");
        damage_lua.push_boolean(lua, 1);
        damage_lua.set_field(lua, 3, "sensor");
        damage_lua.set_top(lua, 2);
    }
    if (mighty_input[7] & 4u) {
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 3, "flyingBird");
        damage_lua.set_top(lua, 2);
    }
    damage_lua.get_field(lua, -10002, "_ab_mighty_metadata");
    if (mighty_input[7] & 8u) {
        damage_lua.push_string(lua, "renamed");
        damage_lua.set_field(lua, 3, "name");
    }
    if (mighty_input[7] & 16u) {
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 3, "collisionFunction");
    }
    if (mighty_input[7] & 32u) {
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 3, "isMightyBait");
    }
    damage_lua.push_boolean(lua, 1);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int mighty_collision_first(void *lua) {
    return mighty_collision(lua, 1);
}

static int mighty_collision_second(void *lua) {
    return mighty_collision(lua, 2);
}

static int mighty_definition(void *lua) {
    ++mighty_definitions;
    uint32_t pc = bird_collision_caller_pc(lua);
    if (pc != (mighty_definitions == 1 ? 44u : mighty_definitions == 2 ? 48u : 522u)) {
        fail("Original mighty definition instruction mismatch\n");
    }
    if (mighty_definitions <= 2) {
        damage_lua.create_table(lua, 0, 0);
    } else if (mighty_definitions == 3) {
        /* First later flying-bird definition boundary. Return nil so the next
         * original VM instruction errors after this C host has returned. */
        damage_lua.push_nil(lua);
    } else {
        fail("Unexpected original mighty-bird later definition query\n");
    }
    return 1;
}

static void mighty_prepare(void *lua) {
    for (size_t field = 0; field < 16; ++field) {
        bird_notify_input[field] = 0;
    }
    bird_notify_count = 0;
    bird_notify_prepare(lua);
    for (size_t id = 0; id < 2; ++id) {
        damage_lua.get_field(lua, -10002, mighty_names[id]);
        bird_notify_scalar(lua, mighty_input[4 + id]);
        damage_lua.set_field(lua, 1, "sensor");
        damage_lua.set_top(lua, 0);
    }
    damage_lua.create_table(lua, 0, 4);
    damage_lua.push_string(lua, "third");
    damage_lua.set_field(lua, 1, "name");
    damage_lua.set_field(lua, -10002, "third");
    uint32_t mode = mighty_input[1];
    damage_lua.get_field(lua, -10002,
                         mode == 1   ? "first"
                         : mode == 2 ? "second"
                                     : "third");
    bird_notify_scalar(lua, mighty_input[2]);
    damage_lua.set_field(lua, 1, "isMightyBait");
    bird_notify_scalar(lua, mighty_input[3]);
    damage_lua.set_field(lua, 1, "isMightyEagle");
    mighty_binding(lua, mighty_input[6]);
    damage_lua.set_field(lua, 1, "collisionFunction");
    damage_lua.set_field(lua, -10002, "_ab_mighty_metadata");
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_function(lua, 1, "getObjectDefinition", mighty_definition);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "g_birdHats");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "achievementProcessor");
    damage_lua.get_field(lua, 1, "objects");
    damage_lua.get_field(lua, 2, "world");
    damage_lua.get_field(lua, -10002, "third");
    damage_lua.set_field(lua, 3, "third");
    damage_lua.set_top(lua, 1);
    if (mode >= 1 && mode <= 3) {
        damage_lua.get_field(lua, -10002, mighty_names[mode - 1u]);
    } else if (mode == 4) {
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_string(lua, "missing");
        damage_lua.set_field(lua, 2, "name");
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, 1, "flyingBird");
    /* Shots and controllable are nil, so intermediate later branches have no
     * effects. Unselected later flying handling reaches LEN flyingGrenades;
     * its absent value causes an original VM length error before other effects. */
    damage_lua.set_top(lua, 0);
}

static int mighty_contains(const char *message, const char *wanted) {
    for (size_t start = 0; message && message[start]; ++start) {
        size_t index = 0;
        while (wanted[index] && message[start + index] &&
               wanted[index] == message[start + index]) {
            ++index;
        }
        if (!wanted[index]) {
            return 1;
        }
    }
    return 0;
}

static void probe_bird_mighty(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original mighty-bird state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/collision-callbacks.lua");
    semihost(4, "AB_ANDROID_BIRD_MIGHTY_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        bird_mighty_case(sample, mighty_input);
        print_words(mighty_input, 8);
        mighty_count = mighty_definitions = 0;
        mighty_prepare(lua);
        damage_lua.get_field(lua, -10002, "CollisionCallbacks");
        damage_lua.get_field(lua, 1, "birdCollision");
        damage_lua.push_string(lua, "first");
        damage_lua.push_string(lua, "second");
        damage_lua.push_number(lua, 1.0f);
        for (size_t field = 0; field < 5; ++field) {
            damage_lua.push_number(lua, 0.0f);
        }
        int status = damage_lua.call(lua, 8, 0, 0);
        const char *message = status ? damage_lua.to_string(lua, -1, NULL) : NULL;
        uint32_t suffix = mighty_definitions == 3              ? 1u
                          : mighty_contains(message, "length") ? 2u
                                                               : 0u;
        if (!status || mighty_definitions < 2 || mighty_definitions > 3 ||
            (!suffix && !mighty_contains(message, "index") &&
             !mighty_contains(message, "call"))) {
            fail(message ? message : "Unexpected original mighty-bird exit\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t result[] = {suffix == 0, suffix};
        print_words(result, 2);
        uint32_t state[14];
        mighty_snapshot(lua, state);
        print_words(state, 14);
        print_words(&mighty_count, 1);
        if (mighty_count) {
            print_words(mighty_event, 17);
        }
        for (size_t id = 0; id < 2; ++id) {
            damage_lua.get_field(lua, -10002, mighty_names[id]);
            damage_lua.get_field(lua, 1, "hasCollided");
            if (damage_lua.type(lua, 2) != 0) {
                fail("Unmeasured bird collision flag changed after mighty phase\n");
            }
            damage_lua.set_top(lua, 0);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird mighty reference complete\n");
}

#endif
