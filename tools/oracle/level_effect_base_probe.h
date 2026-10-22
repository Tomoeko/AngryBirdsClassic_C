#ifndef AB_ORACLE_LEVEL_EFFECT_BASE_H
#define AB_ORACLE_LEVEL_EFFECT_BASE_H

#include "../../tests/level_effect_cases.h"

static const char *const level_effect_native_methods[] = {
    "birdRemovalVolume", "collisionSound", "damageSound", "destroyedSound"};

static int level_effect_native_class(void *lua) {
    if (damage_lua.get_top(lua)) {
        fail("Unexpected base level-effect class creation arguments\n");
    }
    damage_lua.create_table(lua, 0, 8);
    return 1;
}

static void level_effect_native_scalar(void *lua, uint32_t kind) {
    if (!kind) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind <= 5) {
        FloatBits number = {.bits = kind == 3   ? 0u
                                    : kind == 4 ? UINT32_C(0x80000000)
                                                : UINT32_C(0x7fc12345)};
        damage_lua.push_number(lua, number.number);
    } else if (kind == 6) {
        damage_lua.push_string(lua, "sound");
    } else {
        damage_lua.get_field(lua, -10002, "_ab_base_effect_table");
    }
}

static void level_effect_native_result(void *lua, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 1) {
        output[1] = damage_lua.to_boolean(lua, -1) != 0;
    } else if (output[0] == 3) {
        FloatBits number = {.number = damage_lua.to_number(lua, -1)};
        output[1] = number.bits;
    } else if (output[0] == 4) {
        if (!damage_named(lua, -1, "sound")) {
            fail("Unexpected original base level-effect string\n");
        }
        output[1] = 1;
    } else if (output[0] == 5) {
        damage_lua.get_field(lua, -1, "_id");
        if (damage_lua.to_number(lua, -1) != 2) {
            fail("Unexpected original base level-effect table\n");
        }
        damage_lua.set_top(lua, -2);
        output[1] = 2;
    } else if (output[0] != 0) {
        fail("Unexpected original base level-effect result\n");
    }
}

static void probe_level_effect_base(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original base level-effect state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "requireFile", damage_require);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, -10002, "level_effects");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "create", level_effect_native_class);
    damage_lua.set_field(lua, 1, "Class");
    damage_lua.set_field(lua, -10002, "lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/base-level-effect.lua");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_number(lua, 2);
    damage_lua.set_field(lua, 1, "_id");
    damage_lua.set_field(lua, -10002, "_ab_base_effect_table");
    semihost(4, "AB_ANDROID_LEVEL_EFFECT_BASE_V1 64\n");
    for (uint32_t sample = 0; sample < 64; ++sample) {
        uint32_t input[5];
        level_effect_case(sample, input);
        print_words(input, 5);
        damage_lua.create_table(lua, 0, 3);
        for (uint32_t field = 1; field < 4; ++field) {
            level_effect_native_scalar(lua, (input[2] + field - 1u) % 8u);
            damage_lua.set_field(lua, 1, level_effect_native_methods[field]);
        }
        damage_lua.set_field(lua, -10002, "_ab_base_effect_definition");
        damage_lua.get_field(lua, -10002, "level_effects");
        damage_lua.get_field(lua, 1, "LevelEffect");
        damage_lua.get_field(lua, 2, level_effect_native_methods[input[1]]);
        damage_lua.set_field(lua, -10002, "_ab_base_effect_selected");
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_base_effect_selected");
        level_effect_native_scalar(lua, input[3]);
        level_effect_native_scalar(lua, input[4]);
        if (input[1]) {
            if (input[2] < 8) {
                damage_lua.get_field(lua, -10002, "_ab_base_effect_definition");
            } else {
                level_effect_native_scalar(lua, input[2] - 8u);
            }
        }
        uint32_t failed = damage_lua.call(lua, input[1] ? 3 : 2, -1, 0) != 0;
        uint32_t output[4] = {failed, 0, 0, 0};
        if (!failed) {
            output[1] = (uint32_t)damage_lua.get_top(lua);
            if (output[1] != 1) {
                fail("Unexpected original base level-effect return count\n");
            }
            level_effect_native_result(lua, output + 2);
        }
        print_words(output, 4);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB level effect base reference complete\n");
}

#endif
