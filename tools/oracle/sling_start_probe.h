#ifndef AB_ORACLE_SLING_START_H
#define AB_ORACLE_SLING_START_H

static int sling_empty_object(void *lua) {
    damage_lua.create_table(lua, 0, 0);
    return 1;
}

static int sling_default_class(void *lua) {
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, -2, "new", sling_empty_object);
    return 1;
}

static void prepare_sling_start(void *lua, const uint32_t input[7]) {
    sling_input[0] = input[2];
    sling_input[1] = input[3];
    sling_input[10] = 1;
    prepare_sling_launch(lua);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, -2, "getDefaultSlingshotClass", sling_default_class);
    damage_lua.set_field(lua, -10002, "modeSystem");
    damage_lua.get_field(lua, -10002, "_ab_sling_self");
    int self = damage_lua.get_top(lua);
    damage_function(lua, self, "setBirdReloadFunction", damage_require);
    damage_function(lua, self, "setBirdReloadDelay", damage_require);
    damage_function(lua, self, "resetRubberBand", damage_require);
    damage_function(lua, self, "resetCooldown", damage_require);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, -1, "levelLoadReset");
    damage_lua.get_field(lua, -10002, "_ab_sling_self");
    if (damage_lua.call(lua, 1, 0, 0)) {
        const char *error = damage_lua.to_string(lua, -1, NULL);
        fail(error ? error : "Original sling reset failed\n");
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "objects");
    damage_lua.get_field(lua, -1, "world");
    damage_lua.get_field(lua, -1, "first");
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, -2, "name");
    damage_number(lua, -2, "x", input[0]);
    damage_number(lua, -2, "y", input[1]);
    damage_number(lua, -2, "oldY", input[4]);
    damage_number(lua, -2, "angle", input[5]);
    damage_lua.push_boolean(lua, (int)input[6]);
    damage_lua.set_field(lua, -2, "jumpOn");
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_sling_self");
    damage_lua.push_number(lua, 0);
    damage_lua.set_field(lua, -2, "nextBirdTimer");
    damage_lua.set_top(lua, 0);
    damage_lua.push_number(lua, 0);
    damage_lua.set_field(lua, -10002, "currentBirdIndex");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "g_birdHats");
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, -10002, "fillInNextBird");
}

static void print_sling_start_results(void *lua) {
    uint32_t result[10];
    const char *fields[] = {"startX", "startY", "height", "angle", "timer"};
    damage_lua.get_field(lua, -10002, "_ab_sling_self");
    damage_lua.get_field(lua, -1, "birdReloadAnimation");
    for (size_t field = 0; field < 5; ++field) {
        damage_lua.get_field(lua, 2, fields[field]);
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        result[field] = value.bits;
        damage_lua.set_top(lua, 2);
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "objects");
    damage_lua.get_field(lua, -1, "world");
    damage_lua.get_field(lua, -1, "first");
    damage_lua.get_field(lua, 3, "oldY");
    FloatBits old_y = {.number = damage_lua.to_number(lua, -1)};
    result[5] = old_y.bits;
    damage_lua.set_top(lua, 3);
    damage_lua.get_field(lua, 3, "jumpOn");
    result[6] = (uint32_t)damage_lua.type(lua, -1);
    result[7] = (uint32_t)(damage_lua.to_boolean(lua, -1) != 0);
    damage_lua.set_top(lua, 3);
    damage_lua.get_field(lua, 3, "jumpTimer");
    FloatBits jump_timer = {.number = damage_lua.to_number(lua, -1)};
    result[8] = jump_timer.bits;
    damage_lua.set_top(lua, 3);
    damage_lua.get_field(lua, 3, "y");
    FloatBits current_y = {.number = damage_lua.to_number(lua, -1)};
    result[9] = current_y.bits;
    damage_lua.set_top(lua, 0);
    print_words(result, 10);
    uint32_t count = (uint32_t)transfer_event_count;
    print_words(&count, 1);
    for (size_t event = 0; event < transfer_event_count; ++event) {
        print_words(transfer_events[event], 3);
    }
}

static void probe_sling_start(void) {
    void *lua = open_sling_reference();
    prepare_transfer_boundaries(lua);
    uint32_t state = UINT32_C(0x52051aa0);
    semihost(4, "AB_ANDROID_SLING_START_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t input[7];
        for (size_t field = 0; field < 6; ++field) {
            input[field] = random_float(&state, 122, 8, 1);
        }
        if (index % 17 == 0) {
            input[5] = index & 2 ? UINT32_C(0x80000000) : 0;
        }
        input[6] = (uint32_t)index & 1;
        prepare_sling_start(lua, input);
        transfer_event_count = 0;
        print_words(input, 7);
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, -1, "defaultAnimateBirdToSlingShot");
        damage_lua.get_field(lua, -10002, "_ab_sling_self");
        damage_lua.push_number(lua, 0);
        if (damage_lua.call(lua, 2, 0, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original sling initialization failed\n");
        }
        damage_lua.set_top(lua, 0);
        print_sling_start_results(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB sling start reference complete\n");
}

#endif
