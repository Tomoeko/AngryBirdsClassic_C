#ifndef AB_ORACLE_DAMAGE_SPRITE_H
#define AB_ORACLE_DAMAGE_SPRITE_H

static const char *const damage_sprite_names[] = {"base", "rangeA", "rangeB", "rangeC"};

static void push_sprite_value(void *lua, uint32_t code) {
    if (code == 0) {
        damage_lua.push_nil(lua);
    } else if (code == 1) {
        damage_lua.push_boolean(lua, 0);
    } else if (code == 2) {
        damage_lua.push_number(lua, 0);
    } else if (code <= 6) {
        damage_lua.push_string(lua, damage_sprite_names[code - 3]);
    } else {
        fail("Unexpected controlled damage-sprite value\n");
    }
}

static uint32_t sprite_value_code(void *lua, int argument) {
    int tag = damage_lua.type(lua, argument);
    if (tag == 0) {
        return 0;
    }
    if (tag == 1 && !damage_lua.to_boolean(lua, argument)) {
        return 1;
    }
    if (tag == 3 && damage_lua.to_number(lua, argument) == 0) {
        return 2;
    }
    if (tag == 4) {
        for (uint32_t index = 0; index < 4; ++index) {
            if (damage_named(lua, argument, damage_sprite_names[index])) {
                return index + 3;
            }
        }
    }
    fail("Unexpected original damage-sprite selected value\n");
}

static void probe_damage_sprite(void) {
    initialize_damage_lua();
    int (*next)(void *, int) =
        (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original damage-sprite state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/damage-sprite-oracle.lua",
                           "_ab_damage_names");
    uint32_t random = UINT32_C(0x5205b17e);
    semihost(4, "AB_ANDROID_DAMAGE_SPRITE_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        FloatBits strength = {.number = (float)((int)(item % 41) - 10)};
        FloatBits initial = {.number = (float)(item % 13)};
        FloatBits base = {.number = item % 17 ? 10.0f : 0};
        uint32_t input[24] = {(uint32_t)item % 7, (uint32_t)(item % 19 != 0),
                              strength.bits,      (uint32_t)(item / 7) % 3,
                              initial.bits,       base.bits};
        damage_lua.create_table(lua, 0, 0);
        for (size_t index = 0; index < 3; ++index) {
            size_t offset = 6 + index * 6;
            FloatBits minimum = {
                .number = (float)((int)(random_word(&random) % 21) - 10) * 10.0f};
            FloatBits maximum = {.number = minimum.number +
                                           (float)(random_word(&random) % 21) * 10.0f};
            if (item < 64) {
                minimum.number = (float)index * 25.0f;
                maximum.number = minimum.number + 50.0f;
            }
            input[offset] = minimum.bits;
            input[offset + 1] = maximum.bits;
            damage_lua.create_table(lua, 0, 6);
            damage_number(lua, 2, "min", minimum.bits);
            damage_number(lua, 2, "max", maximum.bits);
            const char *fields[] = {"sprite", "blinkSprite", "smileSprite",
                                    "frozenSprite"};
            for (size_t field = 0; field < 4; ++field) {
                input[offset + 2 + field] = random_word(&random) % 7;
                push_sprite_value(lua, input[offset + 2 + field]);
                damage_lua.set_field(lua, 2, fields[field]);
            }
            damage_lua.set_field(lua, 1, damage_sprite_names[index + 1]);
        }
        uint32_t order[3];
        size_t used = 0;
        damage_lua.push_nil(lua);
        while (next(lua, 1)) {
            if (used >= 3) {
                fail("Unexpected damage-sprite range membership\n");
            }
            uint32_t code = sprite_value_code(lua, -2);
            if (code < 4 || code > 6) {
                fail("Unexpected damage-sprite interval key\n");
            }
            order[used++] = code - 4;
            damage_lua.set_top(lua, 2);
        }
        if (used != 3) {
            fail("Incomplete damage-sprite interval table\n");
        }
        damage_lua.set_field(lua, -10002, "_ab_ranges");
        damage_lua.create_table(lua, 0, 4);
        damage_lua.push_string(lua, "base");
        damage_lua.set_field(lua, 1, "definition");
        push_sprite_value(lua, input[0]);
        damage_lua.set_field(lua, 1, "sprite");
        if (input[1]) {
            damage_number(lua, 1, "strength", input[2]);
        }
        if (input[3] == 1) {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 1, "initialStrength");
        } else if (input[3] == 2) {
            damage_number(lua, 1, "initialStrength", input[4]);
        }
        damage_lua.set_field(lua, -10002, "_ab_sprite_object");
        damage_lua.create_table(lua, 0, 1);
        damage_lua.create_table(lua, 0, 2);
        damage_number(lua, 2, "strength", input[5]);
        damage_lua.get_field(lua, -10002, "_ab_ranges");
        damage_lua.set_field(lua, 2, "damageSprites");
        damage_lua.set_field(lua, 1, "base");
        damage_lua.set_field(lua, -10002, "_ab_sprite_definitions");
        damage_lua.get_field(lua, -10002, "_ab_damage_names");
        damage_lua.get_field(lua, -10002, "_ab_sprite_object");
        damage_lua.get_field(lua, -10002, "_ab_sprite_definitions");
        if (damage_lua.call(lua, 2, 1, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original damage-sprite selection failed\n");
        }
        uint32_t output[4];
        const char *fields[] = {"sprite", "blink", "smile", "frozenSprite"};
        for (size_t field = 0; field < 4; ++field) {
            damage_lua.get_field(lua, 1, fields[field]);
            output[field] = sprite_value_code(lua, -1);
            damage_lua.set_top(lua, 1);
        }
        print_words(input, 24);
        print_words(order, 3);
        print_words(output, 4);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB damage sprite reference complete\n");
}

#endif
