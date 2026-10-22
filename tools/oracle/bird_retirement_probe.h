#ifndef AB_ORACLE_BIRD_RETIREMENT_H
#define AB_ORACLE_BIRD_RETIREMENT_H

static uint32_t retirement_input[16], retirement_events[4];
static size_t retirement_event_count;
static uint32_t retirement_removals;

static void retirement_kind(void *lua, uint32_t kind) {
    if (kind == 0) {
        damage_lua.push_nil(lua);
    } else if (kind <= 2) {
        damage_lua.push_boolean(lua, kind == 2);
    } else if (kind == 3) {
        damage_lua.push_number(lua, 0);
    } else {
        damage_lua.push_string(lua, "truthy");
    }
}

static void retirement_event(uint32_t kind) {
    if (retirement_event_count >= 4) {
        fail("Original retirement event overflow\n");
    }
    retirement_events[retirement_event_count++] = kind;
}

static int retirement_definition(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "first")) {
        fail("Unexpected original retirement definition lookup\n");
    }
    retirement_event(1);
    damage_lua.create_table(lua, 0, 1);
    retirement_kind(lua, retirement_input[12]);
    damage_lua.set_field(lua, -2, "manuallyRemovedAfterCollision");
    return 1;
}

static int retirement_speed(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.to_number(lua, 1) != 123 ||
        damage_lua.to_number(lua, 2) != -7) {
        fail("Unexpected original retirement speed arguments\n");
    }
    retirement_event(2);
    FloatBits speed = {.bits = retirement_input[5]};
    damage_lua.push_number(lua, speed.number);
    return 1;
}

static int retirement_camera(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original retirement camera query\n");
    }
    retirement_event(3);
    if (retirement_input[15]) {
        damage_lua.get_field(lua, -10002, "_ab_retirement_bird");
    } else {
        damage_lua.push_nil(lua);
    }
    return 1;
}

static int retirement_remove(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original retirement removal\n");
    }
    damage_lua.get_field(lua, 1, "name");
    if (!damage_named(lua, -1, "first")) {
        fail("Unexpected original retirement bird identity\n");
    }
    retirement_event(4);
    ++retirement_removals;
    return 0;
}

static void retirement_prepare(void *lua) {
    damage_lua.create_table(lua, 0, 12);
    const char *names[] = {"controllable", "isMightyBait",       "isMightyEagle",
                           "shot",         "waterTimerIsActive", "outsideBoundaries"};
    const size_t indices[] = {8, 9, 10, 11, 13, 14};
    for (size_t field = 0; field < 6; ++field) {
        retirement_kind(lua, retirement_input[indices[field]]);
        damage_lua.set_field(lua, 1, names[field]);
    }
    if (retirement_input[1]) {
        damage_number(lua, 1, "isReadyForRemoveTimer", retirement_input[0]);
    }
    if (retirement_input[3] == 2) {
        damage_number(lua, 1, "waterTimer", retirement_input[2]);
    } else {
        retirement_kind(lua, retirement_input[3]);
        damage_lua.set_field(lua, 1, "waterTimer");
    }
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, 1, "name");
    damage_lua.push_number(lua, 123);
    damage_lua.set_field(lua, 1, "xVel");
    damage_lua.push_number(lua, -7);
    damage_lua.set_field(lua, 1, "yVel");
    damage_lua.set_field(lua, -10002, "_ab_retirement_bird");
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_number(lua, 1, "g_bird_remove_velocity_treshold", retirement_input[6]);
    damage_number(lua, 1, "g_bird_remove_time", retirement_input[7]);
    damage_lua.set_top(lua, 0);
}

static void probe_bird_retirement(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original retirement reference state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(
        lua, "/data/local/tmp/ab520/retirement/bird-retirement-oracle.lua",
        "_ab_retirement");
    damage_lua.create_table(lua, 0, 5);
    damage_function(lua, 1, "getObjectDefinition", retirement_definition);
    damage_function(lua, 1, "vLengthsq", retirement_speed);
    damage_function(lua, 1, "removeBird", retirement_remove);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "getTargetBird", retirement_camera);
    damage_lua.set_field(lua, 1, "cameraSystem");
    damage_lua.set_field(lua, -10002, "gamelua");
    uint32_t random = UINT32_C(0x520ae710);
    semihost(4, "AB_ANDROID_BIRD_RETIREMENT_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 16; ++field) {
            retirement_input[field] = random_word(&random) % 5;
        }
        FloatBits timer = {.number = (float)((int)(item % 13) - 6) / 10.0f};
        FloatBits elapsed = {.number = (float)(item % 17) / 100.0f};
        FloatBits speed = {.number = (float)(item % 9) / 100.0f};
        FloatBits threshold = {.number = (float)((item / 9) % 9) / 100.0f};
        retirement_input[0] = timer.bits;
        retirement_input[1] &= 1;
        retirement_input[2] = timer.bits;
        retirement_input[3] %= 3;
        retirement_input[4] = elapsed.bits;
        retirement_input[5] = speed.bits;
        retirement_input[6] = threshold.bits;
        retirement_input[7] = UINT32_C(0x3f800000);
        retirement_input[8] = (uint32_t)(item / 1000) % 5;
        retirement_input[9] = (uint32_t)(item / 200) % 5;
        retirement_input[10] = (uint32_t)(item / 40) % 5;
        retirement_input[11] = (uint32_t)(item / 8) % 5;
        retirement_input[15] &= 1;
        if (item < 2048) {
            retirement_input[8] = 2;
            retirement_input[9] = 0;
            retirement_input[10] = 1;
            retirement_input[11] = 2;
            if (item < 1024) {
                retirement_input[12] = 1;
            }
        }
        /* False uses the fallback during an active water update, but the later
         * inactive comparison would reject it as a nonnumeric original state. */
        if (retirement_input[3] == 1 && retirement_input[13] < 2) {
            retirement_input[3] = 0;
        }
        if (item < 32) {
            retirement_input[0] = item & 1 ? UINT32_C(0x80000000) : 0;
            retirement_input[2] = retirement_input[0];
            retirement_input[4] = 0;
        }
        retirement_prepare(lua);
        retirement_event_count = 0;
        retirement_removals = 0;
        print_words(retirement_input, 16);
        damage_lua.get_field(lua, -10002, "_ab_retirement");
        damage_lua.create_table(lua, 0, 0);
        damage_lua.push_string(lua, "first");
        damage_lua.get_field(lua, -10002, "_ab_retirement_bird");
        FloatBits seconds = {.bits = retirement_input[4]};
        damage_lua.push_number(lua, seconds.number);
        if (damage_lua.call(lua, 4, 1, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original bird retirement update failed\n");
        }
        uint32_t output[6] = {0};
        output[4] = (uint32_t)damage_lua.to_boolean(lua, -1);
        output[5] = retirement_removals;
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_retirement_bird");
        const char *fields[] = {"isReadyForRemoveTimer", "waterTimer"};
        for (size_t field = 0; field < 2; ++field) {
            damage_lua.get_field(lua, 1, fields[field]);
            int numeric = damage_lua.type(lua, -1) == 3;
            FloatBits value = {.number = damage_lua.to_number(lua, -1)};
            output[2 * field] = numeric ? value.bits : 0;
            output[2 * field + 1] = (uint32_t)numeric;
            damage_lua.set_top(lua, 1);
        }
        damage_lua.set_top(lua, 0);
        print_words(output, 6);
        uint32_t count = (uint32_t)retirement_event_count;
        print_words(&count, 1);
        for (size_t event = 0; event < retirement_event_count; ++event) {
            print_words(retirement_events + event, 1);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird retirement reference complete\n");
}

#endif
