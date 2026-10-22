#ifndef AB_ORACLE_FLAME_APPLY_H
#define AB_ORACLE_FLAME_APPLY_H

#include "flame_effect_probe.h"

static uint32_t flame_apply_output[20];
static unsigned flame_apply_phase;

static int flame_capture_script(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original flame script registration\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_flame_script");
    return 0;
}

static uint32_t flame_sprite_code(void *lua, int index) {
    for (size_t item = 0; item < 8; ++item) {
        if (damage_named(lua, index, flame_frames[item])) {
            return (uint32_t)item + 1;
        }
    }
    if (damage_lua.type(lua, index) != 0) {
        fail("Unexpected original constructed flame sprite\n");
    }
    return 0;
}

static int flame_capture_update(void *lua) {
    if (flame_apply_phase || damage_lua.get_top(lua) != 2 ||
        damage_lua.type(lua, 1) != 5 || damage_lua.type(lua, 2) != 6) {
        fail("Unexpected original flame update registration\n");
    }
    flame_apply_phase = 1;
    flame_apply_output[19] = 1;
    damage_lua.get_field(lua, 1, "flameEffect");
    flame_apply_output[0] = flame_field(lua, 3, "timer");
    flame_apply_output[1] = flame_field(lua, 3, "timerSpeed");
    flame_apply_output[2] = flame_field(lua, 3, "scale");
    damage_lua.get_field(lua, 3, "sprite");
    flame_apply_output[3] = flame_sprite_code(lua, 4);
    damage_lua.set_top(lua, 3);
    const char *fields[] = {"newAngle", "oldAngle", "angle", "offset"};
    for (size_t index = 0; index < 4; ++index) {
        damage_lua.get_field(lua, 3, fields[index]);
        if (damage_lua.type(lua, 4) != 0) {
            flame_apply_output[4] |= 1u << index;
        }
        damage_lua.set_top(lua, 3);
    }
    return 0;
}

static int flame_apply_angle(void *lua) {
    if (flame_apply_phase != 1) {
        fail("Original flame angle query precedes registration\n");
    }
    flame_apply_phase = 2;
    return flame_get_angle(lua);
}

static int flame_apply_velocity(void *lua) {
    if (flame_apply_phase != 2) {
        fail("Original flame velocity query precedes angle query\n");
    }
    flame_apply_phase = 3;
    return flame_get_velocity(lua);
}

static void flame_optional_number(void *lua, int table, const char *field,
                                  uint32_t kind, uint32_t bits) {
    if (kind == 2) {
        damage_number(lua, table, field, bits);
    } else {
        if (kind) {
            damage_lua.push_boolean(lua, 0);
        } else {
            damage_lua.push_nil(lua);
        }
        damage_lua.set_field(lua, table, field);
    }
}

static void probe_flame_apply(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original flame construction state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "registerScript", flame_capture_script);
    damage_lua.set_field(lua, -10002, "BlockScriptManager");
    load_selected_function(lua, "/data/local/tmp/ab520/block-event/flame-module.lua",
                           "_ab_flame_module");
    damage_lua.get_field(lua, -10002, "_ab_flame_module");
    if (damage_lua.call(lua, 0, 0, 0)) {
        fail("Original whole flame module failed\n");
    }
    load_selected_function(lua, "/data/local/tmp/ab520/block-event/flame-clamp.lua",
                           "_ab_flame_clamp");
    damage_lua.create_table(lua, 0, 5);
    damage_function(lua, 1, "getAngle", flame_apply_angle);
    damage_function(lua, 1, "getVelocity", flame_apply_velocity);
    damage_function(lua, 1, "addObjectUpdateFunction", flame_capture_update);
    damage_lua.get_field(lua, -10002, "_ab_flame_clamp");
    damage_lua.set_field(lua, 1, "clampAngle");
    damage_lua.set_field(lua, -10002, "gamelua");
    void (*raw_set)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    void (*seed)(int32_t) = (void (*)(int32_t))find_function("srand48");
    int32_t (*next)(void) = (int32_t (*)(void))find_function("lrand48");
    uint32_t random = UINT32_C(0x520f1a4a);
    semihost(4, "AB_ANDROID_FLAME_APPLY_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[23];
        for (size_t field = 0; field < 20; ++field) {
            flame_input[field] = random_float(&random, 118, 16, 1);
        }
        flame_input[4] = (uint32_t)(item % 3);
        flame_input[8] = (uint32_t)((item / 3) % 9);
        flame_input[9] = random_word(&random);
        flame_input[10] = (uint32_t)((item / 27) % 3);
        flame_input[11] = (uint32_t)((item / 81) & 3);
        flame_input[19] = (uint32_t)(item & 7);
        if (item < 256) {
            flame_input[2] = item & 1 ? UINT32_C(0x80000000) : 0;
            flame_input[16] = item & 2 ? UINT32_C(0x80000000) : 0;
        }
        if (item % 31 == 0) {
            flame_input[16] = UINT32_C(0x7fc12345);
        }
        for (size_t field = 0; field < 20; ++field) {
            input[field] = flame_input[field];
            flame_apply_output[field] = 0;
        }
        input[20] = (uint32_t)(item % 3);
        input[21] = (uint32_t)((item / 3) % 3);
        input[22] = random_float(&random, 118, 16, 1);
        flame_prepare(lua, raw_set);
        damage_lua.get_field(lua, -10002, "_ab_flame_definition");
        flame_optional_number(lua, 1, "speed", input[20], input[2]);
        flame_optional_number(lua, 1, "scale", input[21], input[16]);
        damage_lua.set_top(lua, 0);
        damage_lua.create_table(lua, 0, 2);
        damage_number(lua, 1, "z_order", input[22]);
        damage_lua.create_table(lua, 0, 1);
        damage_lua.get_field(lua, -10002, "_ab_flame_definition");
        damage_lua.set_field(lua, 2, "flame");
        damage_lua.set_field(lua, 1, "components");
        damage_lua.set_field(lua, -10002, "_ab_flame_raw_definition");
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, 1, "g_extra_draw_functions");
        damage_lua.set_top(lua, 0);
        flame_apply_phase = 0;
        seed((int32_t)input[9]);
        damage_lua.get_field(lua, -10002, "_ab_flame_script");
        damage_lua.get_field(lua, 1, "apply");
        damage_lua.get_field(lua, -10002, "_ab_flame_script");
        damage_lua.get_field(lua, -10002, "_ab_flame_object");
        damage_lua.get_field(lua, -10002, "_ab_flame_raw_definition");
        if (damage_lua.call(lua, 3, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original flame construction failed\n");
        }
        if (flame_apply_phase != 3) {
            fail("Original flame construction omitted its immediate update\n");
        }
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_flame_object");
        damage_lua.get_field(lua, 1, "flameEffect");
        flame_apply_output[5] = flame_field(lua, 2, "timer");
        damage_lua.get_field(lua, 2, "sprite");
        flame_apply_output[6] = flame_sprite_code(lua, 3);
        damage_lua.set_top(lua, 2);
        flame_apply_output[7] = flame_field(lua, 2, "newAngle");
        flame_apply_output[8] = flame_field(lua, 2, "oldAngle");
        flame_apply_output[9] = flame_field(lua, 2, "angle");
        damage_lua.get_field(lua, 2, "offset");
        flame_apply_output[10] = flame_field(lua, 3, "x");
        flame_apply_output[11] = flame_field(lua, 3, "y");
        damage_lua.set_top(lua, 2);
        flame_apply_output[12] = flame_field(lua, 2, "scale");
        flame_apply_output[13] = flame_field(lua, 2, "timerSpeed");
        flame_apply_output[14] = (uint32_t)next();
        damage_lua.get_field(lua, 2, "definition");
        damage_lua.get_field(lua, -10002, "_ab_flame_definition");
        flame_apply_output[18] =
            damage_lua.to_pointer(lua, 3) == damage_lua.to_pointer(lua, 4);
        damage_lua.set_top(lua, 1);
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, 2, "g_extra_draw_functions");
        damage_lua.get_field(lua, 3, "drawFlameEffect_first");
        flame_apply_output[15] = flame_field(lua, 4, "z");
        damage_lua.get_field(lua, 4, "param");
        flame_apply_output[16] =
            damage_lua.to_pointer(lua, 5) == damage_lua.to_pointer(lua, 1);
        damage_lua.set_top(lua, 4);
        damage_lua.get_field(lua, 4, "func");
        flame_apply_output[17] = damage_lua.type(lua, 5) == 6;
        damage_lua.set_top(lua, 0);
        print_words(input, 23);
        print_words(flame_apply_output, 20);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB flame apply reference complete\n");
}

#endif
