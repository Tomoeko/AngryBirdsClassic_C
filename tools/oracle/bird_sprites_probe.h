#ifndef AB_ORACLE_BIRD_SPRITES_H
#define AB_ORACLE_BIRD_SPRITES_H

#include "bird_notify_probe.h"
#include "bird_collision_frame.h"
#include "../../tests/bird_sprites_cases.h"

static uint32_t sprites_input[18], sprites_events[4][40], sprites_count, sprites_cutoff;
static const char *const sprites_names[] = {"first", "second", "replacement"};
static const char *const sprites_payloads[] = {
    "_ab_sprite_payload1", "_ab_sprite_payload2", "_ab_sprite_payload3"};
static const char *const sprites_strings[] = {
    "old1", "old2", "old3", "new1", "new2", "changed", "GRENADE", "OTHER", "truthy"};

static uint32_t sprites_name_id(void *lua, int index) {
    return damage_named(lua, index, "first")     ? 1u
           : damage_named(lua, index, "second")  ? 2u
           : damage_named(lua, index, "renamed") ? 3u
                                                 : 0u;
}

static uint32_t sprites_object_id(void *lua, int index) {
    if (damage_lua.type(lua, index) == 0) {
        return 0;
    }
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, index, "_id");
    uint32_t id = (uint32_t)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, top);
    return id;
}

static void sprites_value(void *lua, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, -1) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t id = 0; id < 9; ++id) {
            if (damage_named(lua, -1, sprites_strings[id])) {
                output[1] = id + 1u;
                return;
            }
        }
        fail("Unknown original bird sprite string\n");
    } else if (output[0] == 5) {
        output[1] = sprites_object_id(lua, damage_lua.get_top(lua));
    } else if (output[0] != 0) {
        fail("Unexpected original bird sprite value type\n");
    }
}

static void sprites_scalar(void *lua, uint32_t kind) {
    if (kind == 3) {
        FloatBits value = {.bits = sprites_input[0] & 1u ? UINT32_C(0x80000000) : 0};
        damage_lua.push_number(lua, value.number);
    } else {
        bird_notify_scalar(lua, kind);
    }
}

static void sprites_specialty(void *lua, uint32_t kind) {
    if (kind == 4 || kind == 5) {
        damage_lua.push_string(lua, kind == 4 ? "GRENADE" : "OTHER");
    } else {
        sprites_scalar(lua, kind == 6 ? 5u : kind);
    }
}

static void sprites_collision_value(void *lua, uint32_t kind, size_t index) {
    if (kind == 4) {
        damage_lua.push_string(lua, index ? "new2" : "new1");
    } else if (kind == 5) {
        damage_lua.get_field(lua, -10002, sprites_payloads[index]);
    } else {
        sprites_scalar(lua, kind);
    }
}

static void sprites_field(void *lua, const char *object, const char *field,
                          uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, object);
    damage_lua.get_field(lua, top + 1, field);
    sprites_value(lua, output);
    damage_lua.set_top(lua, top);
}

static int sprites_world(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "objects");
    damage_lua.get_field(lua, top + 2, "world");
    return top + 3;
}

static void sprites_snapshot(void *lua, uint32_t output[35]) {
    int top = damage_lua.get_top(lua);
    for (size_t id = 0; id < 2; ++id) {
        damage_lua.get_field(lua, -10002, sprites_names[id]);
        damage_lua.get_field(lua, top + 1, "name");
        output[id] = sprites_name_id(lua, -1);
        damage_lua.set_top(lua, top);
        sprites_field(lua, sprites_names[id], "hasCollided", output + 2 + id * 2);
    }
    for (size_t id = 0; id < 3; ++id) {
        sprites_field(lua, sprites_names[id], "sprite", output + 6 + id * 2);
    }
    sprites_field(lua, "second", "sensor", output + 12);
    sprites_field(lua, "first", "dontUpdateDamageSprite", output + 14);
    sprites_field(lua, "second", "dontUpdateDamageSprite", output + 16);
    sprites_field(lua, "second", "frozenSolid", output + 18);
    sprites_field(lua, "gamelua", "birdSpecialtyAvailable", output + 20);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "flyingBird");
    output[22] = sprites_object_id(lua, top + 2);
    damage_lua.set_top(lua, top);
    int world = sprites_world(lua);
    damage_lua.get_field(lua, world, "first");
    output[23] = sprites_object_id(lua, world + 1);
    damage_lua.set_top(lua, world);
    damage_lua.get_field(lua, world, "second");
    output[24] = sprites_object_id(lua, world + 1);
    damage_lua.set_top(lua, top);
    sprites_field(lua, "_ab_sprite_def1", "spriteCollision", output + 25);
    sprites_field(lua, "_ab_sprite_def2", "spriteCollision", output + 27);
    sprites_field(lua, "first", "sensor", output + 29);
    sprites_field(lua, "first", "shot", output + 31);
    sprites_field(lua, "second", "shot", output + 33);
}

static uint32_t *sprites_record(void *lua, uint32_t kind, uint32_t pc) {
    if (sprites_count >= 4) {
        fail("Original bird sprite event capacity exceeded\n");
    }
    uint32_t *event = sprites_events[sprites_count++];
    for (size_t field = 0; field < 40; ++field) {
        event[field] = 0;
    }
    event[0] = kind;
    event[1] = pc;
    event[2] = sprites_name_id(lua, 1);
    if (!event[2]) {
        fail("Unknown original bird sprite argument name\n");
    }
    if (kind > 2) {
        int top = damage_lua.get_top(lua);
        damage_lua.set_top(lua, 2);
        sprites_value(lua, event + 3);
        damage_lua.set_top(lua, top);
    }
    sprites_snapshot(lua, event + 5);
    return event;
}

static void sprites_object_scalar(void *lua, const char *object, const char *field,
                                  uint32_t kind) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, object);
    sprites_scalar(lua, kind);
    damage_lua.set_field(lua, top + 1, field);
    damage_lua.set_top(lua, top);
}

static void sprites_world_first(void *lua, int present) {
    int top = damage_lua.get_top(lua);
    int world = sprites_world(lua);
    if (present) {
        damage_lua.get_field(lua, -10002, "replacement");
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, world, "first");
    damage_lua.set_top(lua, top);
}

static int sprites_definition(void *lua) {
    if (damage_lua.get_top(lua) != 1) {
        fail("Original bird sprite definition arity mismatch\n");
    }
    uint32_t pc = bird_collision_caller_pc(lua);
    if (pc == 44 || pc == 48) {
        damage_lua.create_table(lua, 0, 0);
        return 1;
    }
    if (pc == 522) {
        sprites_cutoff = pc;
        damage_lua.push_nil(lua);
        return 1;
    }
    if (pc != 378 && pc != 444) {
        fail("Unexpected original bird sprite definition instruction\n");
    }
    uint32_t id = pc == 378 ? 1u : 2u;
    (void)sprites_record(lua, id, pc);
    uint32_t mutation = sprites_input[14];
    if (id == 1) {
        if (mutation & 1u) {
            sprites_object_scalar(lua, "second", "sensor", 2);
        }
        if (mutation & 2u) {
            sprites_object_scalar(lua, "second", "shot", 0);
        }
        if (mutation & 4u) {
            sprites_object_scalar(lua, "first", "dontUpdateDamageSprite", 1);
        }
        if (mutation & 8u) {
            sprites_world_first(lua, 1);
        }
        if (mutation & 16u) {
            damage_lua.get_field(lua, -10002, "gamelua");
            damage_lua.get_field(lua, -10002, "first");
            damage_lua.set_field(lua, 2, "flyingBird");
            damage_lua.set_top(lua, 1);
        }
        if (mutation & 32u) {
            sprites_object_scalar(lua, "gamelua", "birdSpecialtyAvailable", 1);
        }
        if (mutation & 1024u) {
            sprites_world_first(lua, 0);
        }
    }
    if (sprites_input[13] & (1u << (id - 1u))) {
        damage_lua.get_field(lua, -10002,
                             id == 1 ? "_ab_sprite_def1" : "_ab_sprite_def2");
    } else {
        damage_lua.push_nil(lua);
    }
    return 1;
}

static int sprites_set(void *lua) {
    if (damage_lua.get_top(lua) != 2) {
        fail("Original bird setSprite arity mismatch\n");
    }
    uint32_t pc = bird_collision_caller_pc(lua);
    uint32_t id = pc == 412 || pc == 433 ? 1u : pc == 478 || pc == 499 ? 2u : 0;
    if (!id) {
        fail("Unexpected original bird setSprite instruction\n");
    }
    (void)sprites_record(lua, id + 2u, pc);
    uint32_t mutation = sprites_input[14];
    if (id == 1) {
        if (mutation & 64u) {
            sprites_object_scalar(lua, "second", "frozenSolid", 1);
        }
        if (mutation & 128u) {
            sprites_object_scalar(lua, "second", "dontUpdateDamageSprite", 1);
        }
        if (mutation & 256u) {
            damage_lua.get_field(lua, -10002, "_ab_sprite_def2");
            damage_lua.push_string(lua, "changed");
            damage_lua.set_field(lua, 3, "spriteCollision");
            damage_lua.set_top(lua, 2);
        }
        if (mutation & 512u) {
            damage_lua.get_field(lua, -10002, "first");
            damage_lua.push_string(lua, "renamed");
            damage_lua.set_field(lua, 3, "name");
            damage_lua.set_top(lua, 2);
        }
        if (mutation & 2048u) {
            sprites_object_scalar(lua, "second", "sensor", 2);
        }
    }
    damage_lua.push_boolean(lua, 1);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static void sprites_prepare(void *lua) {
    for (size_t field = 0; field < 16; ++field) {
        bird_notify_input[field] = 0;
    }
    bird_notify_count = 0;
    bird_notify_prepare(lua);
    for (size_t id = 0; id < 3; ++id) {
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_number(lua, (float)id + 201.0f);
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.set_field(lua, -10002, sprites_payloads[id]);
    }
    damage_lua.create_table(lua, 0, 3);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, 1, "name");
    damage_lua.push_number(lua, 3.0f);
    damage_lua.set_field(lua, 1, "_id");
    damage_lua.set_field(lua, -10002, "replacement");
    for (size_t id = 0; id < 3; ++id) {
        damage_lua.get_field(lua, -10002, sprites_names[id]);
        damage_lua.push_string(lua, sprites_strings[id]);
        damage_lua.set_field(lua, 1, "sprite");
        if (id < 2) {
            sprites_scalar(lua, sprites_input[1 + id]);
            damage_lua.set_field(lua, 1, "shot");
            sprites_scalar(lua, sprites_input[4 + id]);
            damage_lua.set_field(lua, 1, "dontUpdateDamageSprite");
            sprites_scalar(lua, sprites_input[16 + id]);
            damage_lua.set_field(lua, 1, "hasCollided");
            sprites_scalar(lua, sprites_input[id == 0 ? 15 : 3]);
            damage_lua.set_field(lua, 1, "sensor");
            sprites_scalar(lua,
                           id == 0 ? (sprites_input[0] / 11u) % 6u : sprites_input[6]);
            damage_lua.set_field(lua, 1, "frozenSolid");
        }
        damage_lua.set_top(lua, 0);
    }
    for (size_t id = 0; id < 2; ++id) {
        damage_lua.create_table(lua, 0, 2);
        sprites_specialty(lua, sprites_input[9 + id]);
        damage_lua.set_field(lua, 1, "specialty");
        sprites_collision_value(lua, sprites_input[11 + id], id);
        damage_lua.set_field(lua, 1, "spriteCollision");
        damage_lua.set_field(lua, -10002, id ? "_ab_sprite_def2" : "_ab_sprite_def1");
    }
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_function(lua, 1, "getObjectDefinition", sprites_definition);
    damage_function(lua, 1, "setSprite", sprites_set);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "g_birdHats");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "achievementProcessor");
    sprites_scalar(lua, sprites_input[7]);
    damage_lua.set_field(lua, 1, "birdSpecialtyAvailable");
    if (sprites_input[8]) {
        damage_lua.get_field(lua, -10002, sprites_names[sprites_input[8] - 1u]);
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, 1, "flyingBird");
    damage_lua.set_top(lua, 0);
    int world = sprites_world(lua);
    damage_lua.get_field(lua, -10002, "replacement");
    damage_lua.set_field(lua, world, "renamed");
    damage_lua.set_top(lua, 0);
}

static int sprites_contains(const char *message, const char *wanted) {
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

static void probe_bird_sprites(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original bird sprite state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/collision-callbacks.lua");
    semihost(4, "AB_ANDROID_BIRD_SPRITES_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        bird_sprites_case(sample, sprites_input);
        print_words(sprites_input, 18);
        sprites_count = sprites_cutoff = 0;
        sprites_prepare(lua);
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
        if (!sprites_cutoff && sprites_contains(message, "length")) {
            sprites_cutoff = 789;
        }
        if (!status || (!sprites_cutoff && !sprites_contains(message, "index"))) {
            fail(message ? message : "Unexpected original bird sprite exit\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t result[] = {sprites_cutoff == 0, sprites_cutoff};
        print_words(result, 2);
        uint32_t state[35];
        sprites_snapshot(lua, state);
        print_words(state, 35);
        print_words(&sprites_count, 1);
        for (uint32_t event = 0; event < sprites_count; ++event) {
            print_words(sprites_events[event], 40);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird sprites reference complete\n");
}

#endif
