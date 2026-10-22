#ifndef AB_ORACLE_FLAME_EFFECT_H
#define AB_ORACLE_FLAME_EFFECT_H

#include "selected_function.h"

static uint32_t flame_input[20];
static const char *const flame_frames[] = {"frame1", "frame2", "frame3", "frame4",
                                           "frame5", "frame6", "frame7", "frame8"};

static int flame_get_angle(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "first")) {
        fail("Unexpected original flame angle query\n");
    }
    FloatBits value = {.bits = flame_input[5]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

static int flame_get_velocity(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "first")) {
        fail("Unexpected original flame velocity query\n");
    }
    FloatBits x = {.bits = flame_input[6]}, y = {.bits = flame_input[7]};
    damage_lua.push_number(lua, x.number);
    damage_lua.push_number(lua, y.number);
    return 2;
}

static uint32_t flame_field(void *lua, int table, const char *name) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, name);
    if (damage_lua.type(lua, -1) != 3) {
        fail("Missing numeric original flame state field\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, top);
    return value.bits;
}

static void flame_prepare(void *lua, void (*raw_set)(void *, int, int)) {
    damage_lua.create_table(lua, 0, 2);
    damage_lua.create_table(lua, 0, 0);
    for (uint32_t index = 0; index < flame_input[8]; ++index) {
        damage_lua.push_string(lua, flame_frames[index]);
        raw_set(lua, 2, (int)index + 1);
    }
    damage_lua.set_field(lua, 1, "sprites");
    damage_lua.create_table(lua, 0, 2);
    const char *keys[] = {"base", "default"};
    for (size_t index = 0; index < 2; ++index) {
        if (flame_input[11] & (1u << index)) {
            damage_lua.create_table(lua, 0, 2);
            damage_number(lua, 3, "x", flame_input[12 + index * 2]);
            damage_number(lua, 3, "y", flame_input[13 + index * 2]);
        } else {
            damage_lua.push_boolean(lua, 0);
        }
        damage_lua.set_field(lua, 2, keys[index]);
    }
    damage_lua.set_field(lua, 1, "offset");
    damage_lua.set_field(lua, -10002, "_ab_flame_definition");
    damage_lua.create_table(lua, 0, 3);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, 1, "name");
    if (flame_input[10]) {
        damage_lua.push_string(lua, flame_input[10] == 1 ? "base" : "other");
        damage_lua.set_field(lua, 1, "sprite");
    }
    damage_lua.create_table(lua, 0, 9);
    damage_lua.get_field(lua, -10002, "_ab_flame_definition");
    damage_lua.set_field(lua, 2, "definition");
    damage_number(lua, 2, "timer", flame_input[1]);
    damage_number(lua, 2, "timerSpeed", flame_input[2]);
    if (flame_input[4] == 2) {
        damage_number(lua, 2, "angle", flame_input[3]);
    } else if (flame_input[4] == 1) {
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 2, "angle");
    }
    damage_number(lua, 2, "scale", flame_input[16]);
    damage_number(lua, 2, "oldAngle", flame_input[17]);
    damage_number(lua, 2, "newAngle", flame_input[18]);
    damage_lua.push_string(lua, flame_frames[flame_input[19]]);
    damage_lua.set_field(lua, 2, "sprite");
    damage_lua.set_field(lua, 1, "flameEffect");
    damage_lua.set_field(lua, -10002, "_ab_flame_object");
}

#if !defined(AB_ORACLE_FLAME_APPLY_ONLY)
static void probe_flame_effect(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original flame update state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/block-event/flame-update.lua",
                           "_ab_flame_update");
    load_selected_function(lua, "/data/local/tmp/ab520/block-event/flame-clamp.lua",
                           "_ab_flame_clamp");
    damage_lua.create_table(lua, 0, 3);
    damage_function(lua, 1, "getAngle", flame_get_angle);
    damage_function(lua, 1, "getVelocity", flame_get_velocity);
    damage_lua.get_field(lua, -10002, "_ab_flame_clamp");
    damage_lua.set_field(lua, 1, "clampAngle");
    damage_lua.set_field(lua, -10002, "gamelua");
    void (*raw_set)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void (*seed)(int32_t) = (void (*)(int32_t))find_function("srand48");
    int32_t (*next)(void) = (int32_t (*)(void))find_function("lrand48");
    uint32_t random = UINT32_C(0x520f1a4e);
    semihost(4, "AB_ANDROID_FLAME_EFFECT_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 20; ++field) {
            flame_input[field] = random_float(&random, 118, 16, 1);
        }
        FloatBits seconds = {.number = (float)(random_word(&random) % 513) / 1024};
        if (item % 17 == 0) {
            seconds.bits ^= UINT32_C(0x80000000);
        }
        flame_input[0] = seconds.bits;
        flame_input[4] = (uint32_t)(item % 3);
        flame_input[8] = (uint32_t)((item / 3) % 9);
        flame_input[9] = random_word(&random);
        flame_input[10] = (uint32_t)((item / 27) % 3);
        flame_input[11] = (uint32_t)((item / 81) & 3);
        flame_input[19] = (uint32_t)(item & 7);
        if (item < 256) {
            static const uint32_t timers[] = {0,
                                              UINT32_C(0x80000000),
                                              UINT32_C(0x3f7fffff),
                                              UINT32_C(0x3f800000),
                                              UINT32_C(0x3f800001),
                                              UINT32_C(0x40000000)};
            flame_input[0] = item & 1 ? UINT32_C(0x80000000) : 0;
            flame_input[1] = timers[(item / 2) % 6];
            flame_input[2] = item & 2 ? 0 : UINT32_C(0x3f800000);
            flame_input[3] = item & 4 ? UINT32_C(0x80000000) : 0;
            flame_input[6] = item & 8 ? UINT32_C(0x80000000) : 0;
            flame_input[7] = item & 16 ? UINT32_C(0x80000000) : 0;
        }
        if (item >= 256 && item < 512) {
            flame_input[6] = item & 1 ? UINT32_C(0x3f800001) : UINT32_C(0x3f800000);
            flame_input[7] = item & 2 ? UINT32_C(0x3f7fffff) : UINT32_C(0x3f800000);
        }
        if (item % 31 == 0) {
            flame_input[16] = UINT32_C(0x7fc12345);
        }
        flame_prepare(lua, raw_set);
        seed((int32_t)flame_input[9]);
        damage_lua.get_field(lua, -10002, "_ab_flame_update");
        damage_lua.get_field(lua, -10002, "_ab_flame_object");
        seconds.bits = flame_input[0];
        damage_lua.push_number(lua, seconds.number);
        if (damage_lua.call(lua, 2, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original flame update failed\n");
        }
        damage_lua.get_field(lua, -10002, "_ab_flame_object");
        damage_lua.get_field(lua, 1, "flameEffect");
        uint32_t output[10] = {flame_field(lua, 2, "timer"),
                               0,
                               flame_field(lua, 2, "newAngle"),
                               flame_field(lua, 2, "oldAngle"),
                               flame_field(lua, 2, "angle"),
                               0,
                               0,
                               flame_field(lua, 2, "scale"),
                               flame_field(lua, 2, "timerSpeed"),
                               (uint32_t)next()};
        damage_lua.get_field(lua, 2, "sprite");
        for (size_t index = 0; index < 8; ++index) {
            if (damage_named(lua, 3, flame_frames[index])) {
                output[1] = (uint32_t)index + 1;
            }
        }
        if (!output[1] && damage_lua.type(lua, 3) != 0) {
            fail("Unexpected original flame sprite result\n");
        }
        damage_lua.set_top(lua, 2);
        damage_lua.get_field(lua, 2, "offset");
        output[5] = flame_field(lua, 3, "x");
        output[6] = flame_field(lua, 3, "y");
        damage_lua.set_top(lua, 0);
        print_words(flame_input, 20);
        print_words(output, 10);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB flame effect reference complete\n");
}
#endif

#endif
