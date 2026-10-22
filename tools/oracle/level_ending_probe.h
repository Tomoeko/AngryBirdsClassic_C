#ifndef AB_ORACLE_LEVEL_ENDING_H
#define AB_ORACLE_LEVEL_ENDING_H

static uint32_t ending_input[14], ending_events[2], ending_payload;
static size_t ending_event_count;
static uint32_t ending_query_timeout;

static void ending_kind(void *lua, uint32_t kind) {
    if (!kind) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0);
    } else {
        damage_lua.push_string(lua, "truthy");
    }
}

static uint32_t ending_flag(void *lua, const char *field) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, field);
    uint32_t value = (uint32_t)damage_lua.to_boolean(lua, -1);
    damage_lua.set_top(lua, top);
    return value;
}

static void ending_event(uint32_t kind) {
    if (ending_event_count >= 2) {
        fail("Original ending event overflow\n");
    }
    ending_events[ending_event_count++] = kind;
}

static int ending_win(void *lua) {
    if (damage_lua.get_top(lua) != 0) {
        fail("Unexpected original ending win query arguments\n");
    }
    ending_event(1);
    ending_query_timeout = ending_flag(lua, "levelTimeout");
    damage_lua.push_boolean(lua, (int)ending_input[12]);
    return 1;
}

static int ending_queue(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 2) != 5) {
        fail("Unexpected original ending queue arguments\n");
    }
    const char *fields[] = {"id", "level"};
    const char *expected[] = {"ended", "first"};
    for (size_t field = 0; field < 2; ++field) {
        damage_lua.get_field(lua, 2, fields[field]);
        if (!damage_named(lua, -1, expected[field])) {
            fail("Unexpected original ending event identifier\n");
        }
        damage_lua.set_top(lua, 2);
    }
    damage_lua.get_field(lua, 2, "score");
    FloatBits score = {.number = damage_lua.to_number(lua, -1)};
    if (damage_lua.type(lua, -1) != 3 || score.bits != ending_input[13]) {
        fail("Unexpected original ending score payload\n");
    }
    damage_lua.set_top(lua, 2);
    damage_lua.get_field(lua, 2, "levelComplete");
    if (damage_lua.type(lua, -1) != 1) {
        fail("Unexpected original ending completion payload type\n");
    }
    ending_payload = (uint32_t)damage_lua.to_boolean(lua, -1);
    ending_event(2);
    return 0;
}

static void ending_prepare(void *lua) {
    damage_number(lua, -10002, "levelCompleteTimer", ending_input[0]);
    damage_number(lua, -10002, "g_do_not_wait_for_moving_objects_default_delay",
                  ending_input[8]);
    damage_number(lua, -10002, "g_level_complete_timer_delayed_times",
                  ending_input[10]);
    damage_number(lua, -10002, "score", ending_input[13]);
    damage_number(lua, -10002, "birdBuffTimer", UINT32_C(0x49742400));
    damage_lua.push_boolean(lua, (int)ending_input[2]);
    damage_lua.set_field(lua, -10002, "levelCompleted");
    const char *flags[] = {"g_skip_to_level_end", "g_do_not_wait_delay_set",
                           "hasMovingObjects", "levelTimeout"};
    const size_t indices[] = {3, 7, 9, 11};
    for (size_t field = 0; field < 4; ++field) {
        ending_kind(lua, ending_input[indices[field]]);
        damage_lua.set_field(lua, -10002, flags[field]);
    }
    damage_lua.create_table(lua, 0, 2);
    if (ending_input[4]) {
        damage_number(lua, 1, "doNotWaitForMovingObjectsAfterDelay", ending_input[5]);
    }
    ending_kind(lua, ending_input[6]);
    damage_lua.set_field(lua, 1, "doNotWaitForMovingObjects");
    damage_lua.set_field(lua, -10002, "objects");
}

static void probe_level_ending(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original ending reference state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(
        lua, "/data/local/tmp/ab520/ending/update-level-ending-oracle.lua",
        "_ab_ending");
    damage_function(lua, -10002, "g_game_win_condition", ending_win);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, -10002, "levelName");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "ended");
    damage_lua.set_field(lua, 1, "EID_LEVEL_ENDED");
    damage_lua.set_field(lua, -10002, "events");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "queueEvent", ending_queue);
    damage_lua.set_field(lua, -10002, "eventManager");
    uint32_t random = UINT32_C(0x520e0d05);
    semihost(4, "AB_ANDROID_LEVEL_ENDING_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        FloatBits timer = {.number = (float)((int)(item % 41) - 20) / 10.0f};
        FloatBits seconds = {.number = (float)(item % 17) / 100.0f};
        FloatBits delay = {.number = (float)((int)((item / 3) % 13) - 6) / 10.0f};
        FloatBits fallback = {.number = (float)((int)((item / 7) % 11) - 5) / 10.0f};
        FloatBits extensions = {.number = (float)((int)((item / 17) % 9) - 2)};
        ending_input[0] = timer.bits;
        ending_input[1] = seconds.bits;
        ending_input[2] = (uint32_t)item & 1;
        ending_input[3] = (uint32_t)(item / 1000) % 5;
        ending_input[4] = (uint32_t)(item / 2) & 1;
        ending_input[5] = delay.bits;
        ending_input[6] = random_word(&random) % 5;
        ending_input[7] = random_word(&random) % 5;
        ending_input[8] = fallback.bits;
        ending_input[9] = random_word(&random) % 5;
        ending_input[10] = extensions.bits;
        ending_input[11] = random_word(&random) % 5;
        ending_input[12] = (uint32_t)(item / 4) & 1;
        ending_input[13] = random_float(&random, 0, 150, 1);
        if (item < 32) {
            ending_input[0] = item & 2 ? UINT32_C(0x80000000) : 0;
            ending_input[1] = 0;
            ending_input[5] = ending_input[0];
        }
        ending_prepare(lua);
        ending_event_count = 0;
        ending_payload = ending_query_timeout = 0;
        print_words(ending_input, 14);
        damage_lua.get_field(lua, -10002, "_ab_ending");
        FloatBits elapsed = {.bits = ending_input[1]};
        damage_lua.push_number(lua, elapsed.number);
        if (damage_lua.call(lua, 1, 0, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original level ending failed\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t output[9] = {0};
        const char *numbers[] = {"levelCompleteTimer",
                                 "g_level_complete_timer_delayed_times"};
        const size_t destinations[] = {0, 5};
        for (size_t field = 0; field < 2; ++field) {
            damage_lua.get_field(lua, -10002, numbers[field]);
            FloatBits value = {.number = damage_lua.to_number(lua, -1)};
            output[destinations[field]] = value.bits;
            damage_lua.set_top(lua, 0);
        }
        damage_lua.get_field(lua, -10002, "objects");
        damage_lua.get_field(lua, 1, "doNotWaitForMovingObjectsAfterDelay");
        FloatBits delay_value = {.number = damage_lua.to_number(lua, -1)};
        output[1] = damage_lua.type(lua, -1) == 3;
        output[2] = output[1] ? delay_value.bits : 0;
        damage_lua.set_top(lua, 1);
        damage_lua.get_field(lua, 1, "doNotWaitForMovingObjects");
        output[3] = damage_lua.type(lua, -1) == 1 && damage_lua.to_boolean(lua, -1);
        damage_lua.set_top(lua, 0);
        output[4] = ending_flag(lua, "g_do_not_wait_delay_set");
        output[6] = ending_flag(lua, "levelTimeout");
        output[7] = ending_payload;
        output[8] = ending_query_timeout;
        print_words(output, 9);
        uint32_t count = (uint32_t)ending_event_count;
        print_words(&count, 1);
        for (size_t event = 0; event < ending_event_count; ++event) {
            print_words(ending_events + event, 1);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB level ending reference complete\n");
}

#endif
