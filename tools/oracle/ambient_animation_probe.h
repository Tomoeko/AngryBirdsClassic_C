#ifndef AB_ORACLE_AMBIENT_ANIMATION_H
#define AB_ORACLE_AMBIENT_ANIMATION_H

#include "selected_function.h"

static uint32_t ambient_input[25], ambient_calls, ambient_last_sprite;
static const char *const ambient_names[] = {"frame1", "frame2", "frame3",
                                            "frame4", "frame5", "frame6",
                                            "frame7", "frame8", "base"};
static void (*ambient_raw_set)(void *, int, int);
static void (*ambient_raw_get)(void *, int, int);
static const char *(*ambient_upvalue)(void *, int, int);

static uint32_t ambient_sprite(void *lua, int index) {
    if (damage_lua.type(lua, index) == 0) {
        return 0;
    }
    for (size_t item = 0; item < 9; ++item) {
        if (damage_named(lua, index, ambient_names[item])) {
            return (uint32_t)item + 1;
        }
    }
    fail("Unexpected original ambient animation sprite\n");
}

static void ambient_push_sprite(void *lua, uint32_t code) {
    if (code) {
        damage_lua.push_string(lua, ambient_names[code - 1]);
    } else {
        damage_lua.push_nil(lua);
    }
}

static uint32_t ambient_field(void *lua, int table, const char *name) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, name);
    if (damage_lua.type(lua, -1) != 3) {
        fail("Missing numeric original ambient animation state\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, top);
    return value.bits;
}

static int ambient_enabled(void *lua) {
    if (damage_lua.get_top(lua)) {
        fail("Unexpected original ambient physics query\n");
    }
    damage_lua.push_boolean(lua, 1);
    return 1;
}

static int ambient_set_sprite(void *lua) {
    if (damage_lua.get_top(lua) != 2 || !damage_named(lua, 1, "first")) {
        fail("Unexpected original ambient sprite arguments\n");
    }
    ++ambient_calls;
    ambient_last_sprite = ambient_sprite(lua, 2);
    damage_lua.get_field(lua, -10002, "_ab_ambient_object");
    ambient_push_sprite(lua, ambient_last_sprite);
    damage_lua.set_field(lua, 3, "sprite");
    return 0;
}

static void ambient_prepare(void *lua) {
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, 1, "name");
    ambient_push_sprite(lua, ambient_input[4]);
    damage_lua.set_field(lua, 1, "sprite");
    damage_lua.set_field(lua, -10002, "_ab_ambient_object");
    damage_lua.create_table(lua, 0, 3);
    damage_lua.push_number(lua, 99);
    damage_lua.set_field(lua, 1, "delayTimer");
    damage_lua.push_number(lua, 99);
    damage_lua.set_field(lua, 1, "delayDuration");
    ambient_push_sprite(lua, 9);
    damage_lua.set_field(lua, 1, "originalSprite");
    for (uint32_t item = 0; item < ambient_input[0]; ++item) {
        damage_lua.create_table(lua, 0, 8);
        damage_lua.push_number(lua, (float)item + 1);
        damage_lua.set_field(lua, 2, "id");
        damage_lua.push_number(lua, (float)item + 7);
        damage_lua.set_field(lua, 2, "timer");
        damage_number(lua, 2, "speed", ambient_input[2]);
        size_t base = 9 + item * 4;
        if (ambient_input[base] == 2) {
            damage_number(lua, 2, "delay", ambient_input[base + 1]);
        } else if (ambient_input[base] == 1) {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 2, "delay");
        }
        damage_number(lua, 2, "minDelay", ambient_input[base + 2]);
        damage_number(lua, 2, "maxDelay", ambient_input[base + 3]);
        damage_lua.create_table(lua, 0, 0);
        for (uint32_t frame = 0; frame < ambient_input[3]; ++frame) {
            ambient_push_sprite(lua, frame + 1);
            ambient_raw_set(lua, 3, (int)frame + 1);
        }
        damage_lua.set_field(lua, 2, "sprites");
        ambient_raw_set(lua, 1, (int)item + 1);
    }
    damage_lua.set_field(lua, -10002, "_ab_ambient_state");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.get_field(lua, -10002, "_ab_ambient_object");
    damage_lua.get_field(lua, -10002, "_ab_ambient_state");
    void (*raw_set)(void *, int) =
        (void (*)(void *, int))find_function("_Z10lua_rawsetP9lua_Statei");
    raw_set(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_ambient_map");
    damage_lua.get_field(lua, -10002, "_ab_ambient_update");
    damage_lua.get_field(lua, -10002, "_ab_ambient_map");
    if (!ambient_upvalue(lua, 1, 1)) {
        fail("Cannot bind original ambient object map\n");
    }
    damage_lua.set_top(lua, 0);
}

static void ambient_snapshot(void *lua) {
    uint32_t output[12] = {0};
    damage_lua.get_field(lua, -10002, "_ab_ambient_state");
    damage_lua.get_field(lua, 1, "current");
    if (damage_lua.type(lua, 2) == 5) {
        damage_lua.get_field(lua, 2, "id");
        output[0] = (uint32_t)damage_lua.to_number(lua, 3);
    }
    damage_lua.set_top(lua, 1);
    output[1] = ambient_field(lua, 1, "delayTimer");
    output[2] = ambient_field(lua, 1, "delayDuration");
    damage_lua.get_field(lua, 1, "originalSprite");
    output[3] = ambient_sprite(lua, 2);
    damage_lua.set_top(lua, 1);
    for (uint32_t index = 0; index < ambient_input[0]; ++index) {
        ambient_raw_get(lua, 1, (int)index + 1);
        damage_lua.get_field(lua, 2, "timer");
        if (damage_lua.type(lua, 3) == 3) {
            output[4] |= 1u << index;
            FloatBits value = {.number = damage_lua.to_number(lua, 3)};
            output[5 + index] = value.bits;
        } else if (damage_lua.type(lua, 3) != 0) {
            fail("Unexpected original ambient variant timer\n");
        }
        damage_lua.set_top(lua, 1);
    }
    output[9] = ambient_calls;
    output[10] = ambient_last_sprite;
    damage_lua.get_field(lua, -10002, "_ab_ambient_object");
    damage_lua.get_field(lua, 2, "sprite");
    output[11] = ambient_sprite(lua, 3);
    damage_lua.set_top(lua, 0);
    print_words(output, 12);
}

static void probe_ambient_animation(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original ambient animation state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    ambient_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    ambient_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    ambient_upvalue = (const char *(*)(void *, int, int))find_function(
        "_Z14lua_setupvalueP9lua_Stateii");
    const char *names[] = {"_ab_ambient_start", "_ab_ambient_advance",
                           "_ab_ambient_update"};
    const char *paths[] = {"/data/local/tmp/ab520/block-event/ambient-start.lua",
                           "/data/local/tmp/ab520/block-event/ambient-advance.lua",
                           "/data/local/tmp/ab520/block-event/ambient-update.lua"};
    for (size_t index = 0; index < 3; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    damage_lua.get_field(lua, -10002, names[2]);
    for (int index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, -10002, names[index]);
        if (!ambient_upvalue(lua, 1, index + 2)) {
            fail("Cannot bind original ambient animation helpers\n");
        }
    }
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 3);
    damage_function(lua, 1, "isPhysicsEnabled", ambient_enabled);
    damage_function(lua, 1, "setSprite", ambient_set_sprite);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 3, "first");
    damage_lua.set_field(lua, 2, "world");
    damage_lua.set_field(lua, 1, "objects");
    damage_lua.set_field(lua, -10002, "gamelua");
    void (*seed)(int32_t) = (void (*)(int32_t))find_function("srand48");
    int32_t (*next)(void) = (int32_t (*)(void))find_function("lrand48");
    uint32_t random = UINT32_C(0x520a4b17);
    semihost(4, "AB_ANDROID_AMBIENT_ANIMATION_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        ambient_input[0] = (uint32_t)(item % 4) + 1;
        ambient_input[1] = random_word(&random);
        FloatBits speed = {.number = (float)(random_word(&random) % 17)};
        ambient_input[2] = speed.bits;
        ambient_input[3] = (uint32_t)((item / 4) % 9);
        ambient_input[4] = (uint32_t)((item / 36) % 10);
        for (size_t step = 0; step < 4; ++step) {
            FloatBits seconds = {.number = (float)(random_word(&random) % 65) / 32};
            if (item % 17 == 0) {
                seconds.bits ^= UINT32_C(0x80000000);
            }
            if (item < 256) {
                seconds.number = step ? 0.125f : 0;
            }
            ambient_input[5 + step] = seconds.bits;
        }
        for (size_t variant = 0; variant < 4; ++variant) {
            size_t base = 9 + variant * 4;
            ambient_input[base] = (uint32_t)((item / 3 + variant) % 3);
            for (size_t field = 1; field < 4; ++field) {
                FloatBits value = {.number =
                                       (float)(random_word(&random) % 257) / 64 - 1};
                ambient_input[base + field] = value.bits;
            }
            if (item < 256) {
                ambient_input[base + 1] = item & 4 ? UINT32_C(0x80000000) : 0;
            }
        }
        ambient_prepare(lua);
        seed((int32_t)ambient_input[1]);
        print_words(ambient_input, 25);
        for (size_t step = 0; step < 4; ++step) {
            ambient_calls = ambient_last_sprite = 0;
            damage_lua.get_field(lua, -10002, names[2]);
            damage_lua.push_nil(lua);
            FloatBits seconds = {.bits = ambient_input[5 + step]};
            damage_lua.push_number(lua, seconds.number);
            damage_lua.push_nil(lua);
            if (damage_lua.call(lua, 3, 0, 0)) {
                const char *message = damage_lua.to_string(lua, -1, NULL);
                fail(message ? message : "Original ambient animation failed\n");
            }
            ambient_snapshot(lua);
        }
        uint32_t following = (uint32_t)next();
        print_words(&following, 1);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB ambient animation reference complete\n");
}

#endif
