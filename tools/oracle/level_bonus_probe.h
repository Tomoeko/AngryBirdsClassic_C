#ifndef AB_ORACLE_LEVEL_BONUS_H
#define AB_ORACLE_LEVEL_BONUS_H

static uint32_t bonus_input[9], bonus_events[8][2];
static size_t bonus_event_count;
static void (*bonus_set_table)(void *, int);

static void bonus_kind(void *lua, uint32_t kind) {
    if (kind == 0) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0);
    } else {
        damage_lua.push_string(lua, "first");
    }
}

static void bonus_event(uint32_t kind, uint32_t value) {
    if (bonus_event_count >= 8) {
        fail("Original bonus event overflow\n");
    }
    bonus_events[bonus_event_count][0] = kind;
    bonus_events[bonus_event_count++][1] = value;
}

static int bonus_query(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 3) {
        fail("Unexpected original bonus query\n");
    }
    FloatBits index = {.number = damage_lua.to_number(lua, 1)};
    bonus_event(1, index.bits);
    bonus_kind(lua, bonus_input[8]);
    return 1;
}

static int bonus_insert(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 5 ||
        damage_lua.type(lua, 2) != 3) {
        fail("Unexpected original bonus score insertion\n");
    }
    FloatBits increment = {.number = damage_lua.to_number(lua, 2)};
    bonus_event(2, increment.bits);
    return 0;
}

static int bonus_audio_name(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "birds")) {
        fail("Unexpected original bonus sound lookup\n");
    }
    bonus_event(3, 0);
    damage_lua.push_string(lua, "sound");
    return 1;
}

static int bonus_audio(void *lua) {
    if (damage_lua.get_top(lua) != 4 || !damage_named(lua, 1, "sound") ||
        damage_lua.to_number(lua, 2) != 1 || damage_lua.type(lua, 3) != 1 ||
        damage_lua.to_boolean(lua, 3) || damage_lua.to_number(lua, 4) != 0) {
        fail("Unexpected original bonus playback arguments\n");
    }
    bonus_event(4, 0);
    return 0;
}

static int bonus_definition(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) == 0) {
        fail("Unexpected original bonus definition lookup\n");
    }
    bonus_event(5, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "score-sprite");
    damage_lua.set_field(lua, -2, "spriteScore");
    return 1;
}

static int bonus_floating(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original bonus floating score\n");
    }
    const char *fields[] = {"x", "y", "score", "time", "lifetime", "maxScale", "xs"};
    uint32_t expected[] = {UINT32_C(0x3f800000),
                           UINT32_C(0x40000000),
                           bonus_input[3],
                           0,
                           UINT32_C(0x3f666666),
                           UINT32_C(0x40200000),
                           0};
    for (size_t field = 0; field < 7; ++field) {
        damage_lua.get_field(lua, 1, fields[field]);
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        if (damage_lua.type(lua, -1) != 3 || value.bits != expected[field]) {
            fail("Unexpected original bonus floating numeric field\n");
        }
        damage_lua.set_top(lua, 1);
    }
    damage_lua.get_field(lua, 1, "sprite");
    if (!damage_named(lua, -1, "score-sprite")) {
        fail("Unexpected original bonus floating sprite\n");
    }
    bonus_event(6, bonus_input[3]);
    return 0;
}

static int bonus_before(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 3) {
        fail("Unexpected original bonus before-ending arguments\n");
    }
    FloatBits seconds = {.number = damage_lua.to_number(lua, 1)};
    bonus_event(7, seconds.bits);
    return 0;
}

static int bonus_score(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 3) {
        fail("Unexpected original bonus update-score arguments\n");
    }
    FloatBits seconds = {.number = damage_lua.to_number(lua, 1)};
    bonus_event(8, seconds.bits);
    return 0;
}

static void bonus_prepare(void *lua) {
    const char *numbers[] = {"birdBuffTimer", "birdsLeftCounter",
                             "birdsLeftScoreIncrement", "levelCompleteTimer",
                             "floatingScoreScaling"};
    uint32_t values[] = {bonus_input[0], bonus_input[1], bonus_input[3],
                         UINT32_C(0x49742400), UINT32_C(0x40200000)};
    for (size_t field = 0; field < 5; ++field) {
        damage_number(lua, -10002, numbers[field], values[field]);
    }
    const char *flags[] = {"g_skip_to_level_end", "levelCompleted",
                           "eagleBaitLaunched"};
    for (size_t flag = 0; flag < 3; ++flag) {
        bonus_kind(lua, bonus_input[5 + flag]);
        damage_lua.set_field(lua, -10002, flags[flag]);
    }
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, -10002, "eagleDarkness");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 2, "score", bonus_input[2]);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 2, "individualScores");
    damage_lua.set_field(lua, 1, "birds");
    damage_lua.set_field(lua, -10002, "scoreTable");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 4);
    for (uint32_t kind = 1; kind <= 4; ++kind) {
        bonus_kind(lua, kind);
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_number(lua, 1);
        damage_lua.set_field(lua, 4, "x");
        damage_lua.push_number(lua, 2);
        damage_lua.set_field(lua, 4, "y");
        bonus_set_table(lua, 2);
    }
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_field(lua, -10002, "objects");
}

static void probe_level_bonus(void) {
    initialize_damage_lua();
    bonus_set_table =
        (void (*)(void *, int))find_function("_Z12lua_settableP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original bonus reference state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(
        lua, "/data/local/tmp/ab520/bonus/update-level-ending-oracle.lua", "_ab_bonus");
    damage_function(lua, -10002, "getNextBird", bonus_query);
    damage_function(lua, -10002, "getAudioName", bonus_audio_name);
    damage_function(lua, -10002, "playAudio", bonus_audio);
    damage_function(lua, -10002, "getObjectDefinition", bonus_definition);
    damage_function(lua, -10002, "addFloatingScore", bonus_floating);
    damage_function(lua, -10002, "beforeLevelEnding", bonus_before);
    damage_function(lua, -10002, "updateScore", bonus_score);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "insert", bonus_insert);
    damage_lua.set_field(lua, -10002, "table");
    damage_lua.push_string(lua, "birds");
    damage_lua.set_field(lua, -10002, "g_sound_birds");
    uint32_t random = UINT32_C(0x520b0a05);
    semihost(4, "AB_ANDROID_LEVEL_BONUS_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        FloatBits timer = {.number = (float)((int)(item % 19) - 9) / 10.0f};
        FloatBits seconds = {.number = (float)(item % 17) / 100.0f};
        bonus_input[0] = timer.bits;
        bonus_input[1] = random_float(&random, 0, 140, 1);
        bonus_input[2] = random_float(&random, 0, 140, 1);
        bonus_input[3] = random_float(&random, 0, 140, 1);
        bonus_input[4] = seconds.bits;
        bonus_input[5] = (uint32_t)(item / 1000) % 5;
        bonus_input[6] = (uint32_t)(item / 100) % 5;
        bonus_input[7] = (uint32_t)(item / 5) % 5;
        bonus_input[8] = (uint32_t)item % 5;
        if (item < 32) {
            bonus_input[0] = item & 1 ? UINT32_C(0x80000000) : 0;
            bonus_input[4] = 0;
        }
        bonus_prepare(lua);
        bonus_event_count = 0;
        print_words(bonus_input, 9);
        damage_lua.get_field(lua, -10002, "_ab_bonus");
        FloatBits elapsed = {.bits = bonus_input[4]};
        damage_lua.push_number(lua, elapsed.number);
        if (damage_lua.call(lua, 1, 0, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original bonus update failed\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t output[4];
        const char *numbers[] = {"birdBuffTimer", "birdsLeftCounter"};
        for (size_t field = 0; field < 2; ++field) {
            damage_lua.get_field(lua, -10002, numbers[field]);
            FloatBits value = {.number = damage_lua.to_number(lua, -1)};
            output[field] = value.bits;
            damage_lua.set_top(lua, 0);
        }
        damage_lua.get_field(lua, -10002, "scoreTable");
        damage_lua.get_field(lua, 1, "birds");
        damage_lua.get_field(lua, 2, "score");
        FloatBits score = {.number = damage_lua.to_number(lua, -1)};
        output[2] = score.bits;
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "eagleDarkness");
        output[3] = damage_lua.type(lua, -1) != 0;
        damage_lua.set_top(lua, 0);
        print_words(output, 4);
        uint32_t count = (uint32_t)bonus_event_count;
        print_words(&count, 1);
        for (size_t event = 0; event < bonus_event_count; ++event) {
            print_words(bonus_events[event], 2);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB level bonus reference complete\n");
}

#endif
