#ifndef AB_ORACLE_BIRD_NOTIFY_H
#define AB_ORACLE_BIRD_NOTIFY_H

#if !defined(AB_ORACLE_BIRD_PRELUDE_ONLY) && !defined(AB_ORACLE_BIRD_MIGHTY_ONLY) &&   \
    !defined(AB_ORACLE_BIRD_SPRITES_ONLY) &&                                           \
    !defined(AB_ORACLE_BIRD_SPECIALTY_ONLY) &&                                         \
    !defined(AB_ORACLE_BIRD_PROJECTILES_ONLY) &&                                       \
    !defined(AB_ORACLE_BIRD_COLLISION_DAMAGE_ONLY) &&                                  \
    !defined(AB_ORACLE_BIRD_EFFECTS_ONLY) && !defined(AB_ORACLE_BIRD_COLLISION_ONLY)
#include "../../tests/bird_notify_cases.h"
#endif

static uint32_t bird_notify_input[16], bird_notify_events[16][14], bird_notify_count;
static const char *const bird_notify_names[] = {"first", "second"};
static const char *const bird_notify_values[] = {"truthy", "first_handler",
                                                 "second_handler"};

static uint32_t bird_notify_id(void *lua, int argument) {
    int top = damage_lua.get_top(lua);
    if (damage_lua.type(lua, argument) == 5) {
        damage_lua.get_field(lua, argument, "_id");
        uint32_t id = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, top);
        if (id == 1 || id == 2) {
            return id;
        }
    } else {
        for (uint32_t id = 1; id <= 2; ++id) {
            if (damage_named(lua, argument, bird_notify_names[id - 1u])) {
                return id;
            }
        }
    }
    fail("Unknown original bird-notification object\n");
    return 0;
}

static void bird_notify_scalar(void *lua, uint32_t kind) {
    switch (kind) {
        case 0:
            damage_lua.push_nil(lua);
            break;
        case 1:
            damage_lua.push_boolean(lua, 0);
            break;
        case 2:
            damage_lua.push_boolean(lua, 1);
            break;
        case 3:
            damage_lua.push_number(lua, 0);
            break;
        case 4:
            damage_lua.push_string(lua, "truthy");
            break;
        default:
            damage_lua.create_table(lua, 0, 0);
            break;
    }
}

#if !defined(AB_ORACLE_BIRD_SPRITES_ONLY) &&                                           \
    !defined(AB_ORACLE_BIRD_SPECIALTY_ONLY) &&                                         \
    !defined(AB_ORACLE_BIRD_PROJECTILES_ONLY) &&                                       \
    !defined(AB_ORACLE_BIRD_COLLISION_DAMAGE_ONLY) &&                                  \
    !defined(AB_ORACLE_BIRD_EFFECTS_ONLY) && !defined(AB_ORACLE_BIRD_COLLISION_ONLY)
static void bird_notify_value(void *lua, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = (uint32_t)(damage_lua.to_boolean(lua, -1) != 0);
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t id = 1; id <= 3; ++id) {
            if (damage_named(lua, -1, bird_notify_values[id - 1u])) {
                output[1] = id;
                return;
            }
        }
        fail("Unknown bird-notification field string\n");
    }
}
#endif

static uint32_t *bird_notify_record(void *lua, uint32_t kind, uint32_t first,
                                    uint32_t second, uint32_t receiver) {
    if (bird_notify_count >= 16) {
        fail("Original bird-notification event capacity exceeded\n");
    }
    uint32_t *event = bird_notify_events[bird_notify_count++];
    for (size_t field = 0; field < 14; ++field) {
        event[field] = 0;
    }
    event[0] = kind;
    event[1] = first;
    event[2] = second;
    event[3] = receiver;
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_bird_def_second");
    damage_lua.get_field(lua, -1, "onCollision");
    event[12] = (uint32_t)(damage_lua.to_boolean(lua, -1) != 0);
    damage_lua.set_top(lua, top);
    for (uint32_t object = 0; object < 2; ++object) {
        damage_lua.get_field(lua, -10002, bird_notify_names[object]);
        damage_lua.get_field(lua, -1, "sensor");
        if (damage_lua.to_boolean(lua, -1)) {
            event[13] |= 1u << object;
        }
        damage_lua.set_top(lua, top);
    }
    damage_lua.get_field(lua, -10002, "second");
    damage_lua.get_field(lua, -1, "controllable");
    if (damage_lua.to_boolean(lua, -1)) {
        event[13] |= 4u;
    }
    damage_lua.set_top(lua, top + 1);
    damage_lua.get_field(lua, -1, "gameObjectScript");
    if (damage_lua.to_boolean(lua, -1)) {
        event[13] |= 8u;
    }
    damage_lua.set_top(lua, top);
    return event;
}

static void bird_notify_numbers(void *lua, uint32_t *event, int first, int last) {
    for (int argument = first; argument <= last; ++argument) {
        if (damage_lua.type(lua, argument) != 3) {
            fail("Original bird-notification number type mismatch\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        event[4 + argument - first] = value.bits;
    }
}

static int bird_notify_level(void *lua) {
    if (damage_lua.get_top(lua) != 4) {
        fail("Original bird level hook arity mismatch\n");
    }
    uint32_t *event =
        bird_notify_record(lua, 1, bird_notify_id(lua, 1), bird_notify_id(lua, 2), 0);
    bird_notify_numbers(lua, event, 3, 4);
    if (bird_notify_input[7] & 16u) {
        damage_lua.get_field(lua, -10002, "second");
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, -2, "sensor");
    }
    bird_notify_scalar(lua, (bird_notify_input[0] / 17u) % 6u);
    return 1;
}

static int bird_notify_definition(void *lua) {
    uint32_t id = bird_notify_id(lua, 1);
    (void)bird_notify_record(lua, 2, id, 0, 0);
    damage_lua.get_field(lua, -10002,
                         id == 1 ? "_ab_bird_def_first" : "_ab_bird_def_second");
    return 1;
}

static int bird_notify_named(void *lua, uint32_t kind) {
    if (damage_lua.get_top(lua) != 6) {
        fail("Original named bird notification arity mismatch\n");
    }
    uint32_t *event = bird_notify_record(lua, kind, bird_notify_id(lua, 1),
                                         bird_notify_id(lua, 2), 0);
    bird_notify_numbers(lua, event, 3, 6);
    event[11] = 3;
    if (kind == 3) {
        if (bird_notify_input[7] & 1u) {
            damage_lua.get_field(lua, -10002, "_ab_bird_def_second");
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, -2, "onCollision");
        }
        if (bird_notify_input[7] & 8u) {
            damage_lua.get_field(lua, -10002, "second");
            damage_lua.push_boolean(lua, 1);
            damage_lua.set_field(lua, -2, "controllable");
        }
    }
    return 0;
}

static int bird_notify_named_first(void *lua) {
    return bird_notify_named(lua, 3);
}

static int bird_notify_named_second(void *lua) {
    return bird_notify_named(lua, 4);
}

static int bird_notify_scripts(void *lua) {
    if (damage_lua.get_top(lua) != 7 || !damage_named(lua, 2, "collision")) {
        fail("Original bird block-event argument mismatch\n");
    }
    uint32_t *event =
        bird_notify_record(lua, 5, bird_notify_id(lua, 1), bird_notify_id(lua, 3), 0);
    bird_notify_numbers(lua, event, 4, 7);
    event[11] = 3;
    return 0;
}

static int bird_notify_game_script(void *lua) {
    if (damage_lua.get_top(lua) != 9) {
        fail("Original onBirdCollision method arity mismatch\n");
    }
    uint32_t receiver = bird_notify_id(lua, 1);
    uint32_t *event = bird_notify_record(lua, 6, bird_notify_id(lua, 2),
                                         bird_notify_id(lua, 3), receiver);
    bird_notify_numbers(lua, event, 4, 7);
    event[11] = 3;
    int point_type = damage_lua.type(lua, 8);
    if (point_type != damage_lua.type(lua, 9) || (point_type != 0 && point_type != 3)) {
        fail("Original bird point presence mismatch\n");
    }
    event[10] = (uint32_t)point_type;
    if (point_type == 3) {
        FloatBits x = {.number = damage_lua.to_number(lua, 8)};
        FloatBits y = {.number = damage_lua.to_number(lua, 9)};
        event[8] = x.bits;
        event[9] = y.bits;
    }
    if (receiver == 1) {
        uint32_t changes = bird_notify_input[7];
        if (changes & 6u) {
            damage_lua.get_field(lua, -10002, "first");
            damage_lua.push_boolean(lua, (changes & 2u) != 0);
            damage_lua.set_field(lua, -2, "sensor");
        }
        if (changes & 32u) {
            damage_lua.get_field(lua, -10002, "second");
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, -2, "gameObjectScript");
        }
    }
    return 0;
}

static void bird_notify_prepare(void *lua) {
    uint32_t flags = bird_notify_input[6];
    for (uint32_t id = 1; id <= 2; ++id) {
        damage_lua.create_table(lua, 0, 6);
        damage_lua.push_number(lua, (float)id);
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.push_string(lua, bird_notify_names[id - 1u]);
        damage_lua.set_field(lua, 1, "name");
        bird_notify_scalar(lua, bird_notify_input[id]);
        damage_lua.set_field(lua, 1, "projectileFeather");
        bird_notify_scalar(lua, bird_notify_input[id + 2u]);
        damage_lua.set_field(lua, 1, "sensor");
        if (id == 2) {
            bird_notify_scalar(lua, bird_notify_input[5]);
            damage_lua.set_field(lua, 1, "controllable");
        }
        if (flags & (8u << (id - 1u))) {
            damage_lua.create_table(lua, 0, 2);
            damage_lua.push_number(lua, (float)id);
            damage_lua.set_field(lua, 2, "_id");
            damage_function(lua, 2, "onBirdCollision", bird_notify_game_script);
            damage_lua.set_field(lua, 1, "gameObjectScript");
        }
        damage_lua.set_field(lua, -10002, bird_notify_names[id - 1u]);
        damage_lua.create_table(lua, 0, 1);
        if (flags & (2u << (id - 1u))) {
            damage_lua.push_string(lua, id == 1 ? "first_handler" : "second_handler");
            damage_lua.set_field(lua, 1, "onCollision");
        }
        damage_lua.set_field(lua, -10002,
                             id == 1 ? "_ab_bird_def_first" : "_ab_bird_def_second");
    }
    damage_lua.create_table(lua, 0, 8);
    damage_function(lua, 1, "getObjectDefinition", bird_notify_definition);
    if (flags & 32u) {
        damage_function(lua, 1, "first_handler", bird_notify_named_first);
    }
    if (flags & 64u) {
        damage_function(lua, 1, "second_handler", bird_notify_named_second);
    }
    damage_lua.push_string(lua, "level");
    damage_lua.set_field(lua, 1, "levelName");
    damage_lua.create_table(lua, 0, 1);
    if (flags & 1u) {
        damage_lua.create_table(lua, 0, 1);
        damage_function(lua, 3, "onBirdCollision", bird_notify_level);
        damage_lua.set_field(lua, 2, "level");
    }
    damage_lua.set_field(lua, 1, "g_level_scripts");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 2);
    for (uint32_t object = 0; object < 2; ++object) {
        damage_lua.get_field(lua, -10002, bird_notify_names[object]);
        damage_lua.set_field(lua, 3, bird_notify_names[object]);
    }
    damage_lua.set_field(lua, 2, "world");
    damage_lua.set_field(lua, 1, "objects");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 3, "triggerEvent", bird_notify_scripts);
    damage_lua.set_field(lua, 2, "BlockScriptManager");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "collision");
    damage_lua.set_field(lua, 3, "EID_COLLISION");
    damage_lua.set_field(lua, 2, "events");
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.create_table(lua, 0, 1);
    uint32_t first = bird_notify_input[15] ? 1u : 0u;
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, bird_notify_names[1u - first]);
    damage_lua.set_field(lua, 3, "hat");
    damage_lua.set_field(lua, 2, bird_notify_names[first]);
    damage_lua.set_field(lua, 1, "g_birdHats");
    damage_lua.set_field(lua, -10002, "gamelua");
}

#if !defined(AB_ORACLE_BIRD_PRELUDE_ONLY) && !defined(AB_ORACLE_BIRD_MIGHTY_ONLY) &&   \
    !defined(AB_ORACLE_BIRD_SPRITES_ONLY) &&                                           \
    !defined(AB_ORACLE_BIRD_SPECIALTY_ONLY) &&                                         \
    !defined(AB_ORACLE_BIRD_PROJECTILES_ONLY) &&                                       \
    !defined(AB_ORACLE_BIRD_COLLISION_DAMAGE_ONLY) &&                                  \
    !defined(AB_ORACLE_BIRD_EFFECTS_ONLY) && !defined(AB_ORACLE_BIRD_COLLISION_ONLY)
static void bird_notify_snapshot(void *lua, int status) {
    uint32_t result[] = {status != 0, status == 0 && !bird_notify_count};
    print_words(result, 2);
    for (uint32_t object = 0; object < 2; ++object) {
        uint32_t fields[9];
        damage_lua.get_field(lua, -10002, bird_notify_names[object]);
        static const char *const names[] = {"projectileFeather", "sensor",
                                            "controllable"};
        for (uint32_t field = 0; field < 3; ++field) {
            damage_lua.get_field(lua, 1, names[field]);
            bird_notify_value(lua, fields + field * 2);
            damage_lua.set_top(lua, 1);
        }
        damage_lua.get_field(lua, 1, "gameObjectScript");
        fields[6] = damage_lua.type(lua, -1) == 5;
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(
            lua, -10002, object == 0 ? "_ab_bird_def_first" : "_ab_bird_def_second");
        damage_lua.get_field(lua, 1, "onCollision");
        bird_notify_value(lua, fields + 7);
        damage_lua.set_top(lua, 0);
        print_words(fields, 9);
    }
    print_words(&bird_notify_count, 1);
    for (uint32_t event = 0; event < bird_notify_count; ++event) {
        print_words(bird_notify_events[event], 14);
    }
}

static void probe_bird_notify(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original bird-notification state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/collision-callbacks.lua");
    semihost(4, "AB_ANDROID_BIRD_NOTIFY_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        bird_notify_case(sample, bird_notify_input);
        print_words(bird_notify_input, 16);
        bird_notify_count = 0;
        bird_notify_prepare(lua);
        damage_lua.get_field(lua, -10002, "CollisionCallbacks");
        damage_lua.get_field(lua, 1, "birdCollision");
        damage_lua.push_string(lua, "first");
        damage_lua.push_string(lua, "second");
        for (size_t field = 8; field <= 13; ++field) {
            if (field >= 12 && !bird_notify_input[14]) {
                damage_lua.push_nil(lua);
            } else {
                FloatBits value = {.bits = bird_notify_input[field]};
                damage_lua.push_number(lua, value.number);
            }
        }
        int status = damage_lua.call(lua, 8, 0, 0);
        if (status && (bird_notify_input[6] & 96u) == 96u) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Unexpected original bird prefix failure\n");
        }
        damage_lua.set_top(lua, 0);
        bird_notify_snapshot(lua, status);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird notify reference complete\n");
}
#endif

#endif
