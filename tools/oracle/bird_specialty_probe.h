#ifndef AB_ORACLE_BIRD_SPECIALTY_H
#define AB_ORACLE_BIRD_SPECIALTY_H

#include "bird_notify_probe.h"
#include "bird_collision_frame.h"
#include "../../tests/bird_specialty_cases.h"

static uint32_t specialty_input[25], specialty_events[6][80], specialty_count;
static uint32_t specialty_prints;
static void (*specialty_raw_get)(void *, int);
static void (*specialty_raw_set)(void *, int);
static const char *const specialty_names[] = {"first", "second", "third", "hat"};
static const char *const specialty_tables[] = {
    "_ab_specialty_key", "_ab_specialty_binding", "_ab_specialty_value"};
static const char *const specialty_strings[] = {
    "old1",    "old2", "old3",    "normal",   "special",  "changed", "OTHER",
    "GRENADE", "BOMB", "BUBBLES", "handlerA", "handlerB", "truthy"};

static int specialty_function_first(void *lua);
static int specialty_function_second(void *lua);
static int specialty_print_first(void *lua);
static int specialty_print_second(void *lua);

static uint32_t specialty_name_id(void *lua, int index) {
    for (uint32_t id = 0; id < 4; ++id) {
        if (damage_named(lua, index, specialty_names[id])) {
            return id + 1u;
        }
    }
    fail("Unknown original specialty object name\n");
    return 0;
}

static uint32_t specialty_object_id(void *lua, int index) {
    if (damage_lua.type(lua, index) == 0) {
        return 0;
    }
    if (damage_lua.type(lua, index) != 5) {
        fail("Original specialty object identity is not a table\n");
    }
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, index, "_id");
    uint32_t id = (uint32_t)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, top);
    return id;
}

static uint32_t specialty_function_id(void *lua, int index, int print) {
    const uint32_t *closure = damage_lua.to_pointer(lua, index);
    if (!closure || ((const uint8_t *)closure)[6] != 1) {
        fail("Unexpected original specialty C-closure ABI\n");
    }
    if (closure[4] ==
        (uintptr_t)(print ? specialty_print_first : specialty_function_first)) {
        return 1;
    }
    if (closure[4] ==
        (uintptr_t)(print ? specialty_print_second : specialty_function_second)) {
        return 2;
    }
    fail("Unknown original specialty function binding\n");
    return 0;
}

static void specialty_value(void *lua, int index, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, index);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, index) != 0;
    } else if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[1] = value.bits;
    } else if (output[0] == 4) {
        for (uint32_t id = 0; id < 13; ++id) {
            if (damage_named(lua, index, specialty_strings[id])) {
                output[1] = id + 1u;
                return;
            }
        }
        fail("Unknown original specialty data string\n");
    } else if (output[0] == 5) {
        output[1] = specialty_object_id(lua, index);
    } else if (output[0] == 6) {
        output[1] = specialty_function_id(lua, index, 0);
    } else if (output[0] != 0) {
        fail("Unexpected original specialty value type\n");
    }
}

static void specialty_scalar(void *lua, uint32_t kind) {
    if (kind == 3) {
        FloatBits value = {.bits = specialty_input[24]};
        damage_lua.push_number(lua, value.number);
    } else if (kind == 5) {
        damage_lua.get_field(lua, -10002, specialty_tables[2]);
    } else {
        bird_notify_scalar(lua, kind);
    }
}

static void specialty_kind(void *lua, uint32_t kind) {
    if (kind >= 4 && kind < 8) {
        static const char *const values[] = {"OTHER", "GRENADE", "BOMB", "BUBBLES"};
        damage_lua.push_string(lua, values[kind - 4u]);
    } else {
        specialty_scalar(lua, kind == 8 ? 5u : kind);
    }
}

static void specialty_collision_key(void *lua, uint32_t kind) {
    if (kind == 3) {
        damage_lua.push_number(lua, 0.0f);
    } else if (kind == 4 || kind == 6) {
        damage_lua.push_string(lua, kind == 4 ? "handlerA" : "handlerB");
    } else if (kind == 5) {
        damage_lua.get_field(lua, -10002, specialty_tables[0]);
    } else {
        specialty_scalar(lua, kind);
    }
}

static void specialty_sprite_value(void *lua, uint32_t kind, int special) {
    if (kind == 4) {
        damage_lua.push_string(lua, special ? "special" : "normal");
    } else {
        specialty_scalar(lua, kind);
    }
}

static void specialty_push_key(void *lua, uint32_t index) {
    if (index == 0) {
        damage_lua.push_boolean(lua, 1);
    } else if (index == 1) {
        damage_lua.push_number(lua, 0.0f);
    } else if (index == 2) {
        damage_lua.get_field(lua, -10002, specialty_tables[0]);
    } else {
        damage_lua.push_string(lua, index == 3 ? "handlerA" : "handlerB");
    }
}

static void specialty_push_binding(void *lua, uint32_t kind) {
    if (kind == 0) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0.0f);
    } else if (kind == 4) {
        damage_lua.get_field(lua, -10002, specialty_tables[1]);
    } else {
        damage_lua.push_closure(
            lua, kind == 5 ? specialty_function_first : specialty_function_second, 0);
    }
}

static uint32_t specialty_binding_kind(void *lua) {
    int type = damage_lua.type(lua, -1);
    if (type == 0) {
        return 0;
    }
    if (type == 1) {
        return damage_lua.to_boolean(lua, -1) ? 2u : 1u;
    }
    if (type == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        if (value.bits) {
            fail("Original specialty binding number changed unexpectedly\n");
        }
        return 3;
    }
    if (type == 5 && specialty_object_id(lua, damage_lua.get_top(lua)) == 202) {
        return 4;
    }
    if (type == 6) {
        return specialty_function_id(lua, -1, 0) + 4u;
    }
    fail("Unexpected original specialty binding value\n");
    return 0;
}

static void specialty_field(void *lua, const char *object, const char *field,
                            uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, object);
    damage_lua.get_field(lua, top + 1, field);
    specialty_value(lua, top + 2, output);
    damage_lua.set_top(lua, top);
}

static int specialty_world(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "objects");
    damage_lua.get_field(lua, top + 2, "world");
    return top + 3;
}

static void specialty_snapshot(void *lua, uint32_t output[67]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "flyingBird");
    output[0] = specialty_object_id(lua, top + 2);
    damage_lua.set_top(lua, top);
    specialty_field(lua, "gamelua", "birdSpecialtyAvailable", output + 1);
    int world = specialty_world(lua);
    for (size_t id = 0; id < 2; ++id) {
        damage_lua.get_field(lua, world, specialty_names[id]);
        output[3 + id] = specialty_object_id(lua, world + 1);
        damage_lua.set_top(lua, world);
    }
    damage_lua.get_field(lua, world, "hat");
    output[5] = damage_lua.type(lua, -1) == 5;
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "g_birdHats");
    damage_lua.get_field(lua, top + 2, "first");
    output[6] = damage_lua.type(lua, -1) == 5;
    damage_lua.set_top(lua, top);
    static const char *const definition_fields[] = {
        "specialty",       "onFlyingBirdCollided", "specialtyAvailableAfterCollision",
        "spriteCollision", "explosionTimer",       "spriteSpecial1"};
    for (size_t field = 0; field < 6; ++field) {
        specialty_field(lua, "_ab_specialty_definition", definition_fields[field],
                        output + 7 + field * 2);
    }
    damage_lua.get_field(lua, -10002, "gamelua");
    for (uint32_t key = 0; key < 5; ++key) {
        specialty_push_key(lua, key);
        specialty_raw_get(lua, top + 1);
        output[19 + key] = specialty_binding_kind(lua);
        damage_lua.set_top(lua, top + 1);
    }
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, -10002, "print");
    output[24] = specialty_function_id(lua, -1, 1);
    damage_lua.set_top(lua, top);
    static const char *const fields[] = {"collision",    "sprite",
                                         "damageSprite", "bombTimer",
                                         "pinkTimer",    "dontUpdateDamageSprite",
                                         "inBubble"};
    for (size_t id = 0; id < 3; ++id) {
        for (size_t field = 0; field < 7; ++field) {
            specialty_field(lua, specialty_names[id], fields[field],
                            output + 25 + id * 14 + field * 2);
        }
    }
}

static uint32_t *specialty_record(void *lua, uint32_t kind, uint32_t pc,
                                  uint32_t variant, uint32_t arguments) {
    if (specialty_count >= 6 || (uint32_t)damage_lua.get_top(lua) != arguments) {
        fail("Original specialty event arity/capacity mismatch\n");
    }
    uint32_t *event = specialty_events[specialty_count++];
    for (size_t field = 0; field < 80; ++field) {
        event[field] = 0;
    }
    event[0] = kind;
    event[1] = pc;
    event[2] = variant;
    event[12] = arguments;
    specialty_snapshot(lua, event + 13);
    return event;
}

static void specialty_object_scalar(void *lua, const char *object, const char *field,
                                    uint32_t kind) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, object);
    specialty_scalar(lua, kind);
    damage_lua.set_field(lua, top + 1, field);
    damage_lua.set_top(lua, top);
}

static void specialty_object_text(void *lua, const char *object, const char *field,
                                  const char *value) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, object);
    damage_lua.push_string(lua, value);
    damage_lua.set_field(lua, top + 1, field);
    damage_lua.set_top(lua, top);
}

static void specialty_clear_flying(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.push_nil(lua);
    damage_lua.set_field(lua, top + 1, "flyingBird");
    damage_lua.set_top(lua, top);
}

static int specialty_definition(void *lua) {
    uint32_t pc = bird_collision_caller_pc(lua);
    if (pc == 44 || pc == 48) {
        damage_lua.create_table(lua, 0, 0);
        return 1;
    }
    if (pc != 522) {
        fail("Unexpected original bird specialty definition instruction\n");
    }
    uint32_t *event = specialty_record(lua, 1, pc, 1, 1);
    event[3] = 4;
    event[4] = specialty_name_id(lua, 1);
    if (specialty_input[19] & 16384u) {
        specialty_clear_flying(lua);
    }
    if (specialty_input[18]) {
        damage_lua.get_field(lua, -10002, "_ab_specialty_definition");
    } else {
        damage_lua.push_nil(lua);
    }
    return 1;
}

static int specialty_print(void *lua, uint32_t variant) {
    uint32_t pc = bird_collision_caller_pc(lua);
    if ((pc != 534 && pc != 538) || specialty_prints != (uint32_t)(pc == 538)) {
        fail("Unexpected original bird specialty print instruction/order\n");
    }
    uint32_t *event = specialty_record(lua, pc == 534 ? 2u : 3u, pc, variant, 1);
    specialty_value(lua, 1, event + 3);
    ++specialty_prints;
    uint32_t mutation = specialty_input[19];
    if (pc == 534) {
        if (mutation & 2u) {
            specialty_object_text(lua, "_ab_specialty_definition",
                                  "onFlyingBirdCollided", "handlerB");
        }
        if (mutation & 8u) {
            damage_function(lua, -10002, "print", specialty_print_second);
        }
    } else {
        if (mutation & 4u) {
            damage_lua.get_field(lua, -10002, "gamelua");
            damage_lua.get_field(lua, -10002, "_ab_specialty_definition");
            damage_lua.get_field(lua, 3, "onFlyingBirdCollided");
            if (damage_lua.type(lua, -1) == 0) {
                fail("Unexpected nil specialty binding mutation key\n");
            }
            specialty_push_binding(lua, 6);
            specialty_raw_set(lua, 2);
            damage_lua.set_top(lua, 1);
        }
        if (mutation & 16u) {
            specialty_clear_flying(lua);
        }
    }
    return 0;
}

static int specialty_print_first(void *lua) {
    return specialty_print(lua, 1);
}

static int specialty_print_second(void *lua) {
    return specialty_print(lua, 2);
}

static void specialty_redirect_world(void *lua, int missing) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "flyingBird");
    if (damage_lua.type(lua, -1) == 5) {
        damage_lua.get_field(lua, top + 2, "name");
        uint32_t id = specialty_name_id(lua, -1);
        damage_lua.set_top(lua, top);
        if (id <= 2) {
            int world = specialty_world(lua);
            if (missing) {
                damage_lua.push_nil(lua);
            } else {
                damage_lua.get_field(lua, -10002, "third");
            }
            damage_lua.set_field(lua, world, specialty_names[id - 1u]);
        }
    }
    damage_lua.set_top(lua, top);
}

static int specialty_function(void *lua, uint32_t variant) {
    uint32_t pc = bird_collision_caller_pc(lua);
    if (pc != 549) {
        fail("Unexpected original bird specialty named instruction\n");
    }
    uint32_t *event = specialty_record(lua, 4, pc, variant, 7);
    event[3] = (uint32_t)damage_lua.type(lua, 1);
    event[4] = specialty_object_id(lua, 1);
    event[5] = specialty_name_id(lua, 2);
    event[6] = specialty_name_id(lua, 3);
    for (int argument = 4; argument <= 7; ++argument) {
        if (damage_lua.type(lua, argument) != 3) {
            fail("Original bird specialty numeric argument mismatch\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        event[7 + argument - 4] = value.bits;
    }
    event[11] = variant;
    uint32_t mutation = specialty_input[19];
    if (mutation & 32u) {
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, -10002, "second");
        damage_lua.set_field(lua, 8, "flyingBird");
        damage_lua.set_top(lua, 7);
    }
    if (mutation & 1u) {
        specialty_object_text(lua, "_ab_specialty_definition", "specialty", "BOMB");
    }
    if (mutation & 64u) {
        specialty_object_scalar(lua, "_ab_specialty_definition",
                                "specialtyAvailableAfterCollision", 1);
    }
    if (mutation & 128u) {
        specialty_object_text(lua, "_ab_specialty_definition", "spriteCollision",
                              "changed");
    }
    if (mutation & 256u) {
        specialty_object_scalar(lua, "_ab_specialty_definition", "explosionTimer", 1);
        specialty_object_text(lua, "_ab_specialty_definition", "spriteSpecial1",
                              "changed");
    }
    if (mutation & 512u) {
        specialty_redirect_world(lua, 0);
    }
    if (mutation & 1024u) {
        specialty_redirect_world(lua, 1);
    }
    damage_lua.push_boolean(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int specialty_function_first(void *lua) {
    return specialty_function(lua, 1);
}

static int specialty_function_second(void *lua) {
    return specialty_function(lua, 2);
}

static int specialty_sprite(void *lua) {
    uint32_t pc = bird_collision_caller_pc(lua);
    if (pc != 595 && pc != 619 && pc != 678) {
        fail("Unexpected original bird specialty sprite instruction\n");
    }
    uint32_t *event = specialty_record(lua, 5, pc, 1, 2);
    event[3] = 4;
    event[4] = specialty_name_id(lua, 1);
    specialty_value(lua, 2, event + 5);
    if (specialty_input[19] & 2048u) {
        specialty_object_scalar(lua, "gamelua", "birdSpecialtyAvailable", 1);
    }
    if (specialty_input[19] & 4096u) {
        specialty_clear_flying(lua);
    }
    damage_lua.push_boolean(lua, 1);
    damage_lua.create_table(lua, 0, 0);
    return 2;
}

static int specialty_release(void *lua) {
    uint32_t pc = bird_collision_caller_pc(lua);
    if (pc != 736) {
        fail("Unexpected original bird specialty releaseHat instruction\n");
    }
    uint32_t *event = specialty_record(lua, 6, pc, 1, 1);
    event[3] = 4;
    event[4] = specialty_name_id(lua, 1);
    if (specialty_input[19] & 8192u) {
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, 2, "flyingBird");
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 3, "collision");
        damage_lua.set_top(lua, 1);
        specialty_object_scalar(lua, "gamelua", "birdSpecialtyAvailable", 1);
    }
    damage_lua.push_boolean(lua, 0);
    return 1;
}

static void specialty_prepare(void *lua) {
    for (size_t field = 0; field < 16; ++field) {
        bird_notify_input[field] = 0;
    }
    bird_notify_count = 0;
    bird_notify_prepare(lua);
    for (size_t id = 0; id < 3; ++id) {
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_number(lua, (float)id + 201.0f);
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.set_field(lua, -10002, specialty_tables[id]);
    }
    for (uint32_t id = 2; id < 4; ++id) {
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_number(lua, (float)id + 1.0f);
        damage_lua.set_field(lua, 1, "_id");
        damage_lua.push_string(lua, specialty_names[id]);
        damage_lua.set_field(lua, 1, "name");
        damage_lua.set_field(lua, -10002, specialty_names[id]);
    }
    for (size_t id = 0; id < 3; ++id) {
        damage_lua.get_field(lua, -10002, specialty_names[id]);
        damage_lua.push_string(lua, specialty_strings[id]);
        damage_lua.set_field(lua, 1, "sprite");
        damage_lua.push_string(lua, specialty_strings[id]);
        damage_lua.set_field(lua, 1, "damageSprite");
        if (specialty_input[1] == id + 1u) {
            specialty_scalar(lua, specialty_input[4]);
            damage_lua.set_field(lua, 1, "collision");
            specialty_scalar(lua, specialty_input[11]);
            damage_lua.set_field(lua, 1, "dontUpdateDamageSprite");
            specialty_scalar(lua, specialty_input[14]);
            damage_lua.set_field(lua, 1, "inBubble");
            specialty_scalar(lua, specialty_input[15]);
            damage_lua.set_field(lua, 1, "pinkTimer");
        }
        if (id < 2) {
            specialty_scalar(lua, specialty_input[id == 0 ? 3 : 2]);
            damage_lua.set_field(lua, 1, "sensor");
        }
        damage_lua.set_top(lua, 0);
    }
    damage_lua.create_table(lua, 0, 6);
    specialty_kind(lua, specialty_input[5]);
    damage_lua.set_field(lua, 1, "specialty");
    specialty_collision_key(lua, specialty_input[6]);
    damage_lua.set_field(lua, 1, "onFlyingBirdCollided");
    specialty_scalar(lua, specialty_input[9]);
    damage_lua.set_field(lua, 1, "specialtyAvailableAfterCollision");
    specialty_sprite_value(lua, specialty_input[10], 0);
    damage_lua.set_field(lua, 1, "spriteCollision");
    specialty_scalar(lua, specialty_input[12]);
    damage_lua.set_field(lua, 1, "explosionTimer");
    specialty_sprite_value(lua, specialty_input[13], 1);
    damage_lua.set_field(lua, 1, "spriteSpecial1");
    damage_lua.set_field(lua, -10002, "_ab_specialty_definition");
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_function(lua, 1, "getObjectDefinition", specialty_definition);
    damage_function(lua, 1, "setSprite", specialty_sprite);
    damage_function(lua, 1, "releaseHat", specialty_release);
    for (uint32_t key = 0; key < 5; ++key) {
        specialty_push_key(lua, key);
        specialty_push_binding(lua, specialty_input[key == 4 ? 8 : 7]);
        specialty_raw_set(lua, 1);
    }
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "achievementProcessor");
    specialty_scalar(lua, specialty_input[16]);
    damage_lua.set_field(lua, 1, "birdSpecialtyAvailable");
    if (specialty_input[1]) {
        damage_lua.get_field(lua, -10002, specialty_names[specialty_input[1] - 1u]);
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, 1, "flyingBird");
    damage_lua.create_table(lua, 0, 2);
    if (specialty_input[17]) {
        for (size_t id = 0; id < 2; ++id) {
            damage_lua.create_table(lua, 0, 1);
            damage_lua.push_string(lua, "hat");
            damage_lua.set_field(lua, 3, "hat");
            damage_lua.set_field(lua, 2, specialty_names[id]);
        }
    }
    damage_lua.set_field(lua, 1, "g_birdHats");
    damage_lua.set_top(lua, 0);
    int world = specialty_world(lua);
    damage_lua.get_field(lua, -10002, "third");
    damage_lua.set_field(lua, world, "third");
    if (specialty_input[17] == 1) {
        damage_lua.get_field(lua, -10002, "hat");
        damage_lua.set_field(lua, world, "hat");
    }
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "print", specialty_print_first);
}

static int specialty_contains(const char *message, const char *wanted) {
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

static void probe_bird_specialty(void) {
    initialize_damage_lua();
    specialty_raw_get =
        (void (*)(void *, int))find_function("_Z10lua_rawgetP9lua_Statei");
    specialty_raw_set =
        (void (*)(void *, int))find_function("_Z10lua_rawsetP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original bird specialty state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/collision-callbacks.lua");
    semihost(4, "AB_ANDROID_BIRD_SPECIALTY_V1 8192\n");
    for (uint32_t sample = 0; sample < 8192; ++sample) {
        bird_specialty_case(sample, specialty_input);
        print_words(specialty_input, 25);
        specialty_count = specialty_prints = 0;
        specialty_prepare(lua);
        damage_lua.get_field(lua, -10002, "CollisionCallbacks");
        damage_lua.get_field(lua, 1, "birdCollision");
        damage_lua.push_string(lua, "first");
        damage_lua.push_string(lua, "second");
        for (size_t field = 20; field < 24; ++field) {
            FloatBits value = {.bits = specialty_input[field]};
            damage_lua.push_number(lua, value.number);
        }
        damage_lua.push_nil(lua);
        damage_lua.push_nil(lua);
        int status = damage_lua.call(lua, 8, 0, 0);
        const char *message = status ? damage_lua.to_string(lua, -1, NULL) : NULL;
        int length_cutoff = specialty_contains(message, "length");
        if (!status || (!length_cutoff && !specialty_contains(message, "index") &&
                        !specialty_contains(message, "call"))) {
            fail(message ? message : "Unexpected original bird specialty exit\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t failure = !length_cutoff;
        print_words(&failure, 1);
        uint32_t state[67];
        specialty_snapshot(lua, state);
        print_words(state, 67);
        print_words(&specialty_count, 1);
        for (uint32_t event = 0; event < specialty_count; ++event) {
            print_words(specialty_events[event], 80);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird specialty reference complete\n");
}

#endif
