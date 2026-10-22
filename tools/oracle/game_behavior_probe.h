#ifndef AB_ORACLE_GAME_BEHAVIOR_H
#define AB_ORACLE_GAME_BEHAVIOR_H

#include "../../tests/game_behavior_cases.h"

static uint32_t behavior_input[60], behavior_events[24][20];
static uint32_t behavior_count, behavior_mutated;
static void (*behavior_set_index)(void *, int, int);
static void (*behavior_get_index)(void *, int, int);
static const char *const behavior_globals[] = {
    "_ab_behavior_1",  "_ab_behavior_2",  "_ab_behavior_3",  "_ab_behavior_4",
    "_ab_behavior_5",  "_ab_behavior_6",  "_ab_behavior_7",  "_ab_behavior_8",
    "_ab_behavior_9",  "_ab_behavior_10", "_ab_behavior_11", "_ab_behavior_12",
    "_ab_behavior_13", "_ab_behavior_14", "_ab_behavior_15", "_ab_behavior_16",
    "_ab_behavior_17", "_ab_behavior_18", "_ab_behavior_19", "_ab_behavior_20"};
static const char *const behavior_groups[] = {
    "_ab_group_1", "_ab_group_2", "_ab_group_3", "_ab_group_4", "_ab_group_5"};

static int behavior_method_first(void *lua);
static int behavior_method_second(void *lua);

static uint32_t behavior_id(void *lua, int argument) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, argument, "_id");
    uint32_t id = (uint32_t)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, top);
    return id;
}

static void behavior_argument(void *lua, int argument, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, argument);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, argument) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        if (!damage_named(lua, argument, "payload")) {
            fail("Original behavior forwarded an unexpected string\n");
        }
        output[1] = 1;
    } else if (output[0] == 5) {
        output[1] = behavior_id(lua, argument);
        if (output[1] != 101 && output[1] != 102) {
            fail("Original behavior forwarded an unexpected table\n");
        }
    } else if (output[0] != 0) {
        fail("Unexpected original behavior argument type\n");
    }
}

static void behavior_binding(void *lua, uint32_t kind) {
    switch (kind) {
        case 0:
            damage_lua.push_nil(lua);
            break;
        case 1:
            damage_lua.push_boolean(lua, 0);
            break;
        case 2:
            damage_lua.push_closure(lua, behavior_method_first, 0);
            break;
        case 3:
            damage_lua.push_closure(lua, behavior_method_second, 0);
            break;
        case 4:
            damage_lua.push_number(lua, 0.0f);
            break;
        default:
            damage_lua.create_table(lua, 0, 0);
            break;
    }
}

static uint32_t behavior_binding_kind(void *lua) {
    int type = damage_lua.type(lua, -1);
    if (type == 0) {
        return 0;
    }
    if (type == 1 && !damage_lua.to_boolean(lua, -1)) {
        return 1;
    }
    if (type == 3) {
        return 4;
    }
    if (type == 5) {
        return 5;
    }
    if (type == 6) {
        const uint32_t *closure = damage_lua.to_pointer(lua, -1);
        const uint8_t *header = (const uint8_t *)closure;
        if (closure && header[6] == 1) {
            if (closure[4] == (uintptr_t)behavior_method_first) {
                return 2;
            }
            if (closure[4] == (uintptr_t)behavior_method_second) {
                return 3;
            }
        }
    }
    fail("Unexpected original behavior method binding\n");
    return 0;
}

static void behavior_mutate(void *lua, uint32_t id) {
    if (behavior_mutated) {
        return;
    }
    behavior_mutated = 1;
    uint32_t group = (id - 1u) / 4u, slot = (id - 1u) % 4u;
    uint32_t action = behavior_input[5];
    int top = damage_lua.get_top(lua);
    if (action == 1 || action == 2 || action == 8) {
        uint32_t target = action == 8 ? id : id + 1u;
        if (target > 20) {
            fail("Invalid original behavior mutation target\n");
        }
        damage_lua.get_field(lua, -10002, behavior_globals[target - 1u]);
        behavior_binding(lua, action == 2 ? 3u : 0u);
        damage_lua.set_field(lua, top + 1, behavior_event_names[behavior_input[6]]);
    } else if (action >= 3 && action <= 5) {
        if (group >= 4 || slot >= 3) {
            fail("Unexpected original behavior first callback identity\n");
        }
        damage_lua.get_field(lua, -10002, behavior_groups[group]);
        if (action == 3) {
            damage_lua.push_nil(lua);
        } else {
            damage_lua.get_field(lua, -10002, behavior_globals[action == 4 ? 16 : 17]);
        }
        behavior_set_index(lua, top + 1, (int)(action == 5 ? 4u : slot + 2u));
    } else if (action == 6 || action == 7) {
        if (group >= 4) {
            fail("Unexpected original behavior group mutation\n");
        }
        damage_lua.get_field(lua, -10002, "_ab_behavior_map");
        const char *key = behavior_group_names[action == 6 ? (group + 1u) % 4u : group];
        damage_lua.get_field(lua, top + 1, key);
        if (damage_lua.type(lua, -1) != 0) {
            damage_lua.set_top(lua, top + 1);
            if (action == 6) {
                damage_lua.push_nil(lua);
            } else {
                damage_lua.get_field(lua, -10002, behavior_groups[4]);
            }
            damage_lua.set_field(lua, top + 1, key);
        }
    }
    damage_lua.set_top(lua, top);
}

static int behavior_method(void *lua, uint32_t variant) {
    uint32_t argc = (uint32_t)damage_lua.get_top(lua);
    if (argc != behavior_input[7] + 1u || behavior_count >= 24 ||
        damage_lua.type(lua, 1) != 5) {
        fail("Original behavior call arity/receiver mismatch\n");
    }
    uint32_t id = behavior_id(lua, 1);
    if (id < 1 || id > 20) {
        fail("Original behavior call identity mismatch\n");
    }
    uint32_t *event = behavior_events[behavior_count++];
    for (size_t field = 0; field < 20; ++field) {
        event[field] = 0;
    }
    event[0] = id;
    event[1] = variant;
    event[2] = argc - 1u;
    event[3] = behavior_mutated;
    for (uint32_t argument = 1; argument < argc; ++argument) {
        behavior_argument(lua, (int)argument + 1, event + 4 + (argument - 1u) * 2u);
    }
    behavior_mutate(lua, id);
    /* Arbitrary method return values must not stop later behaviors. */
    damage_lua.push_boolean(lua, variant == 1);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int behavior_method_first(void *lua) {
    return behavior_method(lua, 1);
}

static int behavior_method_second(void *lua) {
    return behavior_method(lua, 2);
}

static int behavior_class_create(void *lua) {
    damage_lua.create_table(lua, 0, 32);
    return 1;
}

static void behavior_prepare(void *lua) {
    for (uint32_t id = 0; id < 20; ++id) {
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_number(lua, (float)id + 1.0f);
        damage_lua.set_field(lua, 1, "_id");
        behavior_binding(lua, behavior_input[8 + id]);
        damage_lua.set_field(lua, 1, behavior_event_names[behavior_input[6]]);
        damage_lua.set_field(lua, -10002, behavior_globals[id]);
    }
    for (uint32_t group = 0; group < 5; ++group) {
        damage_lua.create_table(lua, 4, 0);
        for (uint32_t slot = 0; slot < 4; ++slot) {
            uint32_t id = group < 4  ? behavior_input[28 + group * 4 + slot]
                          : slot < 3 ? 17u + slot
                                     : 0;
            if (id) {
                damage_lua.get_field(lua, -10002, behavior_globals[id - 1u]);
                behavior_set_index(lua, 1, (int)slot + 1);
            }
        }
        damage_lua.set_field(lua, -10002, behavior_groups[group]);
    }
    damage_lua.create_table(lua, 0, (int)behavior_input[1]);
    for (uint32_t index = 0; index < behavior_input[2]; ++index) {
        uint32_t group =
            (behavior_input[3] + (behavior_input[4] ? 4u - index : index)) % 4u;
        damage_lua.get_field(lua, -10002, behavior_groups[group]);
        damage_lua.set_field(lua, 1, behavior_group_names[group]);
    }
    damage_lua.set_field(lua, -10002, "_ab_behavior_map");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_behavior_map");
    damage_lua.set_field(lua, 1, "behaviors");
    damage_lua.get_field(lua, -10002, "GameObject");
    damage_lua.get_field(lua, 2, "delegateFunctionCall");
    damage_lua.set_field(lua, 1, "delegateFunctionCall");
    damage_lua.set_top(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_game_object");
    for (uint32_t id = 0; id < 2; ++id) {
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_number(lua, (float)id + 101.0f);
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.set_field(lua, -10002,
                             id ? "_ab_argument_second" : "_ab_argument_first");
    }
}

static void behavior_snapshot(void *lua, int status) {
    uint32_t output[46] = {status != 0, behavior_mutated};
    for (uint32_t id = 0; id < 20; ++id) {
        damage_lua.get_field(lua, -10002, behavior_globals[id]);
        damage_lua.get_field(lua, 1, behavior_event_names[behavior_input[6]]);
        output[2 + id] = behavior_binding_kind(lua);
        damage_lua.set_top(lua, 0);
    }
    for (uint32_t group = 0; group < 5; ++group) {
        damage_lua.get_field(lua, -10002, behavior_groups[group]);
        for (uint32_t slot = 0; slot < 4; ++slot) {
            behavior_get_index(lua, 1, (int)slot + 1);
            output[22 + group * 4 + slot] =
                damage_lua.type(lua, -1) == 0 ? 0 : behavior_id(lua, 2);
            damage_lua.set_top(lua, 1);
        }
        damage_lua.set_top(lua, 0);
    }
    damage_lua.get_field(lua, -10002, "_ab_behavior_map");
    for (uint32_t group = 0; group < 4; ++group) {
        damage_lua.get_field(lua, 1, behavior_group_names[group]);
        if (damage_lua.type(lua, 2) != 0) {
            const void *actual = damage_lua.to_pointer(lua, 2);
            damage_lua.get_field(lua, -10002, behavior_groups[group]);
            if (actual == damage_lua.to_pointer(lua, 3)) {
                output[42 + group] = group + 1u;
            } else {
                damage_lua.get_field(lua, -10002, behavior_groups[4]);
                if (actual != damage_lua.to_pointer(lua, 4)) {
                    fail("Original behavior group identity mismatch\n");
                }
                output[42 + group] = 5;
            }
        }
        damage_lua.set_top(lua, 1);
    }
    damage_lua.set_top(lua, 0);
    print_words(output, 46);
    print_words(&behavior_count, 1);
    for (uint32_t event = 0; event < behavior_count; ++event) {
        print_words(behavior_events[event], 20);
    }
}

static void probe_game_behavior(void) {
    initialize_damage_lua();
    behavior_set_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    behavior_get_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original game behavior state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "requireFile", damage_require);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "create", behavior_class_create);
    damage_lua.set_field(lua, 1, "Class");
    damage_lua.set_field(lua, -10002, "lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/behavior-game-object.lua");
    semihost(4, "AB_ANDROID_GAME_BEHAVIOR_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        game_behavior_case(sample, behavior_input);
        print_words(behavior_input, 60);
        behavior_count = behavior_mutated = 0;
        behavior_prepare(lua);
        damage_lua.get_field(lua, -10002, "GameObject");
        uint32_t event = behavior_input[6];
        damage_lua.get_field(
            lua, 1, event < 3 ? behavior_event_names[event] : "delegateFunctionCall");
        damage_lua.get_field(lua, -10002, "_ab_game_object");
        if (event == 3) {
            damage_lua.push_string(lua, behavior_event_names[event]);
        }
        for (uint32_t argument = 0; argument < behavior_input[7]; ++argument) {
            uint32_t kind = behavior_input[44 + argument * 2];
            if (kind == 0) {
                damage_lua.push_nil(lua);
            } else if (kind <= 2) {
                damage_lua.push_boolean(lua, kind == 2);
            } else if (kind == 3) {
                FloatBits value = {.bits = behavior_input[45 + argument * 2]};
                damage_lua.push_number(lua, value.number);
            } else if (kind == 4) {
                damage_lua.push_string(lua, "payload");
            } else {
                damage_lua.get_field(lua, -10002,
                                     behavior_input[45 + argument * 2] == 101
                                         ? "_ab_argument_first"
                                         : "_ab_argument_second");
            }
        }
        int status =
            damage_lua.call(lua, (int)behavior_input[7] + (event == 3 ? 2 : 1), 0, 0);
        if (status) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            const char *needle = "attempt to call";
            int found = 0;
            for (size_t start = 0; message && message[start]; ++start) {
                size_t matched = 0;
                while (needle[matched] && message[start + matched] &&
                       message[start + matched] == needle[matched]) {
                    ++matched;
                }
                if (!needle[matched]) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                fail(message ? message : "Unexpected original behavior failure\n");
            }
        }
        damage_lua.set_top(lua, 0);
        behavior_snapshot(lua, status);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB game behavior reference complete\n");
}

#endif
