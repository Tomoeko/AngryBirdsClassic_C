#ifndef AB_ORACLE_BIRD_COLLISION_DAMAGE_H
#define AB_ORACLE_BIRD_COLLISION_DAMAGE_H

#include "bird_notify_probe.h"
#include "bird_collision_frame.h"
#include "../../tests/bird_collision_damage_cases.h"

static uint32_t bird_refresh_input[20], bird_refresh_events[3][40], bird_refresh_count;
static const char *const bird_refresh_tables[] = {
    "_ab_damage_value",  "_ab_damage_empty", "_ab_damage_catalog",
    "_ab_damage_blocks", "_ab_damage_raw",   "_ab_damage_descriptor"};
static const char *const bird_refresh_strings[] = {"sprite", "changed", "first",
                                                   "second", "invalid"};

static void bird_refresh_value_push(void *lua, uint32_t kind) {
    if (kind < 3) {
        bird_notify_scalar(lua, kind);
    } else if (kind < 6) {
        FloatBits value = {.bits = kind == 3   ? 0u
                                   : kind == 4 ? UINT32_C(0x80000000)
                                               : UINT32_C(0x7fc01234)};
        damage_lua.push_number(lua, value.number);
    } else if (kind == 6 || kind == 8) {
        damage_lua.push_string(lua, kind == 6 ? "sprite" : "changed");
    } else {
        damage_lua.get_field(lua, -10002, bird_refresh_tables[0]);
    }
}

static void bird_refresh_gate(void *lua, uint32_t kind) {
    bird_refresh_value_push(lua, kind < 4 ? kind : kind == 4 ? 6u : 7u);
}

static void bird_refresh_value(void *lua, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, -1) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t id = 0; id < 5; ++id) {
            if (damage_named(lua, -1, bird_refresh_strings[id])) {
                output[1] = id + 1u;
                return;
            }
        }
        fail("Unknown original damage-refresh string\n");
    } else if (output[0] == 5) {
        damage_lua.get_field(lua, -1, "_id");
        output[1] = (uint32_t)damage_lua.to_number(lua, -1);
        damage_lua.set_top(lua, -2);
        if (output[1] < 201 || output[1] > 206) {
            fail("Unknown original damage-refresh table\n");
        }
    } else if (output[0] != 0) {
        fail("Unexpected original damage-refresh value type\n");
    }
}

static void bird_refresh_field(void *lua, const char *object, const char *field,
                               uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, object);
    damage_lua.get_field(lua, top + 1, field);
    bird_refresh_value(lua, output);
    damage_lua.set_top(lua, top);
}

static void bird_refresh_snapshot(void *lua, uint32_t output[26]) {
    static const char *const fields[] = {"sensor", "controllable", "damageSprite",
                                         "blinkSprite", "smileSprite"};
    for (size_t field = 0; field < 5; ++field) {
        bird_refresh_field(lua, "second", fields[field], output + field * 2u);
    }
    uint32_t position[2];
    bird_refresh_field(lua, "second", "x", position);
    output[10] = position[1];
    bird_refresh_field(lua, "second", "y", position);
    output[11] = position[1];
    bird_refresh_field(lua, "gamelua", "blockTable", output + 12);
    bird_refresh_field(lua, bird_refresh_tables[4], "damageStateChangedParticles",
                       output + 14);
    bird_refresh_field(lua, bird_refresh_tables[5], "name", output + 16);
    bird_refresh_field(lua, bird_refresh_tables[5], "amount", output + 18);
    static const char *const names[] = {"sprite", "blink", "smile"};
    for (size_t field = 0; field < 3; ++field) {
        bird_refresh_field(lua, "_ab_damage_names", names[field],
                           output + 20 + field * 2u);
    }
}

static uint32_t *bird_refresh_record(void *lua, uint32_t kind, uint32_t pc,
                                     uint32_t argc) {
    if (bird_refresh_count >= 3 || (uint32_t)damage_lua.get_top(lua) != argc) {
        fail("Unexpected original damage-refresh boundary arity/capacity\n");
    }
    uint32_t *event = bird_refresh_events[bird_refresh_count++];
    for (size_t field = 0; field < 40; ++field) {
        event[field] = 0;
    }
    event[0] = kind;
    event[1] = pc;
    event[2] = argc;
    bird_refresh_snapshot(lua, event + 14);
    return event;
}

static int bird_refresh_definition(void *lua) {
    uint32_t pc = bird_collision_caller_pc(lua);
    if (pc != 44 && pc != 48 && pc != 762) {
        fail("Unexpected original damage-refresh prior definition boundary\n");
    }
    damage_lua.create_table(lua, 0, 0);
    return 1;
}

static void bird_refresh_descriptor_mutate(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, bird_refresh_tables[5]);
    damage_lua.push_string(lua, "second");
    damage_lua.set_field(lua, top + 1, "name");
    damage_number(lua, top + 1, "amount", bird_refresh_input[19]);
    damage_lua.set_top(lua, top);
}

static int bird_refresh_sprites(void *lua) {
    uint32_t pc = bird_collision_caller_pc(lua);
    if (pc != 959) {
        fail("Unexpected original damage-refresh query instruction\n");
    }
    uint32_t *event = bird_refresh_record(lua, 1, pc, 2);
    damage_lua.get_field(lua, 1, "_id");
    event[3] = (uint32_t)damage_lua.to_number(lua, -1);
    if (event[3] != 2) {
        fail("Original damage-refresh query lost cached second object\n");
    }
    damage_lua.set_top(lua, 2);
    bird_refresh_value(lua, event + 4);
    uint32_t mutation = bird_refresh_input[12];
    if (mutation & 1u) {
        damage_lua.get_field(lua, -10002, "_ab_damage_names");
        damage_lua.get_field(lua, 3, "sprite");
        damage_lua.set_field(lua, 1, "damageSprite");
        damage_lua.set_top(lua, 2);
    }
    if (mutation & 2u) {
        damage_lua.push_boolean(lua, 1);
        damage_lua.set_field(lua, 1, "sensor");
    }
    if (mutation & 4u) {
        damage_lua.push_boolean(lua, 1);
        damage_lua.set_field(lua, 1, "controllable");
    }
    if (mutation & 8u) {
        damage_number(lua, 1, "x", bird_refresh_input[16]);
        damage_number(lua, 1, "y", bird_refresh_input[17]);
    }
    if (mutation & 16u) {
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, -10002, bird_refresh_tables[1]);
        damage_lua.set_field(lua, 3, "blockTable");
        damage_lua.set_top(lua, 2);
    }
    if (mutation & 32u) {
        damage_lua.get_field(lua, -10002, "_ab_damage_names");
        damage_lua.push_string(lua, "changed");
        damage_lua.set_field(lua, 3, "sprite");
        damage_lua.set_top(lua, 2);
    }
    if (mutation & 64u) {
        bird_refresh_descriptor_mutate(lua);
    }
    if (mutation & 128u) {
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 3, "blockTable");
        damage_lua.set_top(lua, 2);
    }
    if (bird_refresh_input[5]) {
        damage_lua.get_field(lua, -10002, "_ab_damage_names");
    } else {
        damage_lua.push_nil(lua);
    }
    return 1;
}

static int bird_refresh_transform(void *lua) {
    uint32_t *event = bird_refresh_record(lua, 2, 0, 2);
    for (int argument = 1; argument <= 2; ++argument) {
        if (damage_lua.type(lua, argument) != 3) {
            fail("Original damage-refresh transform requires numeric position\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        event[2 + argument] = value.bits;
        event[4 + argument] = bird_refresh_input[15 + argument];
    }
    if (bird_refresh_input[12] & 256u) {
        bird_refresh_descriptor_mutate(lua);
    }
    if (bird_refresh_input[12] & 512u) {
        damage_lua.get_field(lua, -10002, bird_refresh_tables[4]);
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 3, "damageStateChangedParticles");
        damage_lua.set_top(lua, 2);
    }
    for (size_t field = 16; field < 18; ++field) {
        FloatBits value = {.bits = bird_refresh_input[field]};
        damage_lua.push_number(lua, value.number);
    }
    return 2;
}

static int bird_refresh_emit(void *lua) {
    uint32_t *event = bird_refresh_record(lua, 3, 0, 9);
    if (damage_named(lua, 1, "first")) {
        event[3] = 3;
    } else if (damage_named(lua, 1, "second")) {
        event[3] = 4;
    } else {
        fail("Unexpected original damage-refresh emission name\n");
    }
    for (int argument = 2; argument <= 7; ++argument) {
        if (damage_lua.type(lua, argument) != 3) {
            fail("Original damage-refresh emission requires numeric argument\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        if (argument < 5) {
            event[2 + argument] = value.bits;
        } else if (value.bits) {
            fail("Original damage-refresh trailing argument must be positive zero\n");
        }
    }
    for (int argument = 8; argument <= 9; ++argument) {
        if (damage_lua.type(lua, argument) != 1 ||
            damage_lua.to_boolean(lua, argument)) {
            fail("Original damage-refresh trailing argument must be Boolean false\n");
        }
    }
    return 0;
}

static void bird_refresh_prepare(void *lua) {
    for (size_t field = 0; field < 16; ++field) {
        bird_notify_input[field] = 0;
    }
    bird_notify_count = 0;
    bird_notify_prepare(lua);
    for (uint32_t id = 0; id < 6; ++id) {
        damage_lua.create_table(lua, 0, 4);
        damage_lua.push_number(lua, (float)id + 201.0f);
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.set_field(lua, -10002, bird_refresh_tables[id]);
    }
    damage_lua.get_field(lua, -10002, bird_refresh_tables[5]);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, 1, "name");
    damage_number(lua, 1, "amount", bird_refresh_input[18]);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, bird_refresh_tables[4]);
    uint32_t descriptor = bird_refresh_input[13];
    if (descriptor == 1) {
        damage_lua.get_field(lua, -10002, bird_refresh_tables[5]);
    } else if (descriptor >= 2 && descriptor <= 4) {
        bird_refresh_value_push(lua, descriptor - 1u);
    } else if (descriptor == 5) {
        damage_lua.push_string(lua, "invalid");
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, 1, "damageStateChangedParticles");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, bird_refresh_tables[3]);
    damage_lua.get_field(lua, -10002, bird_refresh_tables[4]);
    damage_lua.set_field(lua, 1, "Object");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, bird_refresh_tables[2]);
    if (bird_refresh_input[4] == 6) {
        damage_lua.get_field(lua, -10002, bird_refresh_tables[3]);
    } else {
        bird_refresh_gate(lua, bird_refresh_input[4]);
    }
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 3);
    static const char *const names[] = {"sprite", "blink", "smile"};
    for (size_t field = 0; field < 3; ++field) {
        bird_refresh_value_push(lua, bird_refresh_input[6 + field]);
        damage_lua.set_field(lua, 1, names[field]);
    }
    damage_lua.set_field(lua, -10002, "_ab_damage_names");
    damage_lua.get_field(lua, -10002, "second");
    static const char *const fields[] = {"sensor", "controllable", "damageSprite",
                                         "blinkSprite", "smileSprite"};
    for (size_t field = 0; field < 5; ++field) {
        if (field < 2) {
            bird_refresh_gate(lua, bird_refresh_input[1 + field]);
        } else {
            bird_refresh_value_push(lua, bird_refresh_input[7 + field]);
        }
        damage_lua.set_field(lua, 1, fields[field]);
    }
    damage_lua.push_string(lua, "Object");
    damage_lua.set_field(lua, 1, "definition");
    damage_number(lua, 1, "x", bird_refresh_input[14]);
    damage_number(lua, 1, "y", bird_refresh_input[15]);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_function(lua, 1, "getObjectDefinition", bird_refresh_definition);
    damage_function(lua, 1, "getDamageSprite", bird_refresh_sprites);
    damage_function(lua, 1, "physicsToWorldTransform", bird_refresh_transform);
    static const char *const empty[] = {"g_birdHats", "achievementProcessor", "birds",
                                        "flyingGrenades"};
    for (size_t field = 0; field < 4; ++field) {
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, 1, empty[field]);
    }
    damage_number(lua, 1, "collisionParticleForceThreshold", UINT32_C(0x7f7fffff));
    uint32_t catalog = bird_refresh_input[3];
    if (catalog >= 5) {
        damage_lua.get_field(lua, -10002, bird_refresh_tables[catalog == 6 ? 2u : 1u]);
    } else {
        bird_refresh_gate(lua, catalog);
    }
    damage_lua.set_field(lua, 1, "blockTable");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "addParticles", bird_refresh_emit);
    damage_lua.set_field(lua, -10002, "particles");
}

static void probe_bird_collision_damage(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original bird damage-refresh state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/collision-callbacks.lua");
    semihost(4, "AB_ANDROID_BIRD_COLLISION_DAMAGE_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        bird_collision_damage_case(sample, bird_refresh_input);
        print_words(bird_refresh_input, 20);
        bird_refresh_count = 0;
        bird_refresh_prepare(lua);
        damage_lua.get_field(lua, -10002, "CollisionCallbacks");
        damage_lua.get_field(lua, 1, "birdCollision");
        damage_lua.push_string(lua, "first");
        damage_lua.push_string(lua, "second");
        for (size_t field = 0; field < 4; ++field) {
            damage_lua.push_number(lua, 0);
        }
        damage_lua.push_nil(lua);
        damage_lua.push_nil(lua);
        int status = damage_lua.call(lua, 8, 0, 0);
        if (status && !damage_lua.to_string(lua, -1, NULL)) {
            fail("Original damage-refresh error lacks diagnostic\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t failed = status != 0, state[26];
        print_words(&failed, 1);
        bird_refresh_snapshot(lua, state);
        print_words(state, 26);
        print_words(&bird_refresh_count, 1);
        for (uint32_t event = 0; event < bird_refresh_count; ++event) {
            print_words(bird_refresh_events[event], 40);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird collision damage reference complete\n");
}

#endif
