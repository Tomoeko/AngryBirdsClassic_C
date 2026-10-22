#ifndef AB_ORACLE_SCORE_H
#define AB_ORACLE_SCORE_H

static uint32_t score_input[8];

static void score_push_kind(void *lua, uint32_t kind, uint32_t number) {
    if (kind == 0) {
        damage_lua.push_nil(lua);
    } else if (kind == 1 || kind == 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        FloatBits value = {.bits = number};
        damage_lua.push_number(lua, value.number);
    } else {
        damage_lua.push_string(lua, "present");
    }
}

static int score_definition(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "first")) {
        fail("Unexpected original score definition lookup\n");
    }
    damage_lua.create_table(lua, 0, 1);
    score_push_kind(lua, score_input[5], score_input[3]);
    damage_lua.set_field(lua, -2, "destroyedScoreInc");
    return 1;
}

static void score_prepare(void *lua) {
    damage_number(lua, -10002, "blockDestroyedScoreIncrement", score_input[0]);
    damage_number(lua, -10002, "pigletteDestroyedScoreIncrement", score_input[1]);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    if (score_input[6]) {
        damage_number(lua, 3, "destroyedScoreInc", score_input[2]);
    }
    damage_lua.set_field(lua, 2, "base");
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.set_field(lua, -10002, "blockTable");
    damage_lua.create_table(lua, 0, 3);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, 1, "name");
    damage_lua.push_string(lua, "base");
    damage_lua.set_field(lua, 1, "definition");
    score_push_kind(lua, score_input[7], 0);
    damage_lua.set_field(lua, 1, "levelGoal");
    damage_lua.set_field(lua, -10002, "_ab_score_object");
}

static void score_call(void *lua) {
    if (damage_lua.call(lua, 1, 1, 0)) {
        const char *error = damage_lua.to_string(lua, -1, NULL);
        fail(error ? error : "Original scoring function failed\n");
    }
}

static void probe_score(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original scoring reference state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/score/break-score-oracle.lua",
                           "_ab_break_score");
    load_selected_function(lua, "/data/local/tmp/ab520/score/damage-score-oracle.lua",
                           "_ab_damage_score");
    damage_function(lua, -10002, "getObjectDefinition", score_definition);
    uint32_t state = UINT32_C(0x5205c0ae);
    static const uint32_t boundaries[] = {
        0,          0x80000000, 1,          0x80000001, 0x007fffff,
        0x807fffff, 0x3f7fffff, 0xbf7fffff, 0x3f800000, 0xbf800000,
        0x3f800001, 0xbf800001, 0x4b7fffff, 0xcb7fffff, 0x4b800000,
        0xcb800000, 0x4b800001, 0xcb800001, 0x7bffffff, 0xfbffffff,
    };
    semihost(4, "AB_ANDROID_SCORE_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 4; ++field) {
            score_input[field] = random_float(&state, 0, 255, 1);
        }
        score_input[4] = random_float(&state, 0, 250, 1);
        score_input[5] = (uint32_t)item % 5;
        score_input[6] = (uint32_t)(item / 5) % 2;
        score_input[7] = (uint32_t)(item / 10) % 5;
        if (item < 1000) {
            score_input[4] = boundaries[item / 50];
        }
        score_prepare(lua);
        print_words(score_input, 8);
        damage_lua.get_field(lua, -10002, "_ab_break_score");
        damage_lua.get_field(lua, -10002, "_ab_score_object");
        score_call(lua);
        int tag = damage_lua.type(lua, -1);
        if (tag != 0 && tag != 3) {
            fail("Original break score returned an unsupported type\n");
        }
        FloatBits breaking = {.number = damage_lua.to_number(lua, -1)};
        uint32_t output[3] = {(uint32_t)(tag == 3), tag == 3 ? breaking.bits : 0, 0};
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_damage_score");
        FloatBits damage = {.bits = score_input[4]};
        damage_lua.push_number(lua, damage.number);
        score_call(lua);
        if (damage_lua.type(lua, -1) != 3) {
            fail("Original damage score is not numeric\n");
        }
        FloatBits damaging = {.number = damage_lua.to_number(lua, -1)};
        output[2] = damaging.bits;
        print_words(output, 3);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB score reference complete\n");
}

#endif
