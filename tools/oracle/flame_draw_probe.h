#ifndef AB_ORACLE_FLAME_DRAW_H
#define AB_ORACLE_FLAME_DRAW_H

#include "selected_function.h"

static uint32_t flame_draw_input[11], flame_draw_output[9];
static const char *const flame_draw_frames[] = {"frame1", "frame2", "frame3", "frame4",
                                                "frame5", "frame6", "frame7", "frame8"};

static int flame_get_scale(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "first")) {
        fail("Unexpected original flame scale query\n");
    }
    ++flame_draw_output[0];
    FloatBits scale = {.bits = flame_draw_input[1]};
    damage_lua.push_number(lua, scale.number);
    return 1;
}

static int flame_draw_sprite(void *lua) {
    if (damage_lua.get_top(lua) != 5) {
        fail("Unexpected original flame sprite arguments\n");
    }
    ++flame_draw_output[1];
    for (size_t index = 0; index < 8; ++index) {
        if (damage_named(lua, 1, flame_draw_frames[index])) {
            flame_draw_output[3] = (uint32_t)index + 1;
        }
    }
    if (!flame_draw_output[3] && damage_lua.type(lua, 1) != 0) {
        fail("Unexpected original flame draw sprite\n");
    }
    for (int index = 2; index <= 5; ++index) {
        if (damage_lua.type(lua, index) != 3) {
            fail("Expected numeric original flame draw argument\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        flame_draw_output[index + 2] = value.bits;
    }
    return 0;
}

static void flame_draw_prepare(void *lua) {
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    if (flame_draw_input[0] == 0) {
        damage_lua.push_nil(lua);
    } else if (flame_draw_input[0] <= 2) {
        damage_lua.push_boolean(lua, flame_draw_input[0] == 2);
    } else {
        damage_lua.push_number(lua, 0);
    }
    damage_lua.set_field(lua, 3, "first");
    damage_lua.set_field(lua, 2, "world");
    damage_lua.set_field(lua, 1, "objects");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 2, "drawFlameEffect_first");
    damage_lua.set_field(lua, 1, "g_extra_draw_functions");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 4);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, 1, "name");
    damage_number(lua, 1, "x", flame_draw_input[2]);
    damage_number(lua, 1, "y", flame_draw_input[3]);
    damage_lua.create_table(lua, 0, 5);
    damage_number(lua, 2, "newAngle", flame_draw_input[6]);
    damage_number(lua, 2, "scale", flame_draw_input[7]);
    damage_number(lua, 2, "angle", flame_draw_input[8]);
    if (flame_draw_input[9]) {
        damage_lua.push_string(lua, flame_draw_frames[flame_draw_input[9] - 1]);
        damage_lua.set_field(lua, 2, "sprite");
    }
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 3, "x", flame_draw_input[4]);
    damage_number(lua, 3, "y", flame_draw_input[5]);
    damage_lua.set_field(lua, 2, "offset");
    damage_lua.set_field(lua, 1, "flameEffect");
    damage_lua.set_field(lua, -10002, "_ab_flame_draw_object");
}

static void probe_flame_draw(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original flame draw state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/block-event/flame-draw.lua",
                           "_ab_flame_draw");
    load_selected_function(lua, "/data/local/tmp/ab520/block-event/flame-rotate.lua",
                           "_ab_flame_rotate");
    damage_lua.create_table(lua, 0, 5);
    damage_function(lua, 1, "getScale", flame_get_scale);
    damage_function(lua, 1, "drawGameSprite", flame_draw_sprite);
    damage_lua.get_field(lua, -10002, "_ab_flame_rotate");
    damage_lua.set_field(lua, 1, "rotatePointAroundOrigin");
    damage_lua.set_field(lua, -10002, "gamelua");
    void (*seed)(int32_t) = (void (*)(int32_t))find_function("srand48");
    int32_t (*next)(void) = (int32_t (*)(void))find_function("lrand48");
    uint32_t random = UINT32_C(0x520f1a4d);
    semihost(4, "AB_ANDROID_FLAME_DRAW_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 11; ++field) {
            flame_draw_input[field] = random_float(&random, 118, 18, 1);
        }
        flame_draw_input[0] = (uint32_t)(item & 3);
        flame_draw_input[9] = (uint32_t)((item / 4) % 9);
        flame_draw_input[10] = random_word(&random);
        if (item < 512) {
            static const uint32_t edges[] = {0,
                                             UINT32_C(0x80000000),
                                             UINT32_C(0x3f800000),
                                             UINT32_C(0xbf800000),
                                             UINT32_C(0x40490fdb),
                                             UINT32_C(0xc0490fdb),
                                             UINT32_C(0x3fc90fdb),
                                             UINT32_C(0xbfc90fdb)};
            flame_draw_input[6] = edges[(item / 4) & 7];
            flame_draw_input[1] = edges[(item / 32) & 3];
            flame_draw_input[4] = edges[(item / 128) & 3];
            flame_draw_input[5] = edges[(item / 128 + 1) & 3];
        }
        if (item >= 512 && item < 768) {
            for (size_t field = 1; field < 9; ++field) {
                flame_draw_input[field] = random_word(&random) % UINT32_C(0x800000);
                if (item & 4) {
                    flame_draw_input[field] |= UINT32_C(0x80000000);
                }
            }
        }
        if (item % 8 < 2) {
            for (size_t field = 1; field < 9; ++field) {
                flame_draw_input[field] = UINT32_C(0x7fc12345);
            }
        }
        for (size_t field = 0; field < 9; ++field) {
            flame_draw_output[field] = 0;
        }
        flame_draw_prepare(lua);
        seed((int32_t)flame_draw_input[10]);
        damage_lua.get_field(lua, -10002, "_ab_flame_draw");
        damage_lua.get_field(lua, -10002, "_ab_flame_draw_object");
        if (damage_lua.call(lua, 1, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original flame draw failed\n");
        }
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, 1, "g_extra_draw_functions");
        damage_lua.get_field(lua, 2, "drawFlameEffect_first");
        flame_draw_output[2] = damage_lua.type(lua, 3) != 0;
        flame_draw_output[8] = (uint32_t)next();
        damage_lua.set_top(lua, 0);
        print_words(flame_draw_input, 11);
        print_words(flame_draw_output, 9);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB flame draw reference complete\n");
}

#endif
