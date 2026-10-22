#ifndef AB_ORACLE_BIRD_JUMP_H
#define AB_ORACLE_BIRD_JUMP_H

static uint32_t jump_input[13], jump_events[12][4], jump_random_state;
static size_t jump_event_count;
static const char *const jump_fields[] = {
    "x",         "y",         "angle",    "jumpTimer", "jumpHeight",
    "jumpSpeed", "jumpAngle", "jumpRoll", "oldY",      "oldAngle"};

static uint32_t *jump_event(uint32_t kind) {
    if (jump_event_count >= 12) {
        fail("Original jump event overflow\n");
    }
    uint32_t *event = jump_events[jump_event_count++];
    event[0] = kind;
    event[1] = event[2] = event[3] = 0;
    return event;
}

static int jump_random(void *lua) {
    int low = (int)damage_lua.to_number(lua, 1);
    int high = (int)damage_lua.to_number(lua, 2);
    if (damage_lua.get_top(lua) != 2 || low < 0 || high < low || high > 35) {
        fail("Unexpected original jump random bounds\n");
    }
    uint32_t value =
        (uint32_t)low + random_word(&jump_random_state) % ((uint32_t)(high - low) + 1);
#if defined(AB_ORACLE_BIRD_JUMP_OVERSHOOT)
    if (jump_event_count == (jump_input[12] & 3)) {
        value = (uint32_t)high + 1;
    }
#endif
    uint32_t *event = jump_event(3);
    event[1] = (uint32_t)low;
    event[2] = (uint32_t)high;
    event[3] = value;
    damage_lua.push_number(lua, (float)value);
    return 1;
}

static int jump_position(void *lua) {
    if (damage_lua.get_top(lua) != 3 || !damage_named(lua, 1, "first")) {
        fail("Unexpected original jump position boundary\n");
    }
    uint32_t *event = jump_event(1);
    FloatBits x = {.number = damage_lua.to_number(lua, 2)};
    FloatBits y = {.number = damage_lua.to_number(lua, 3)};
    event[1] = x.bits;
    event[2] = y.bits;
    damage_lua.get_field(lua, -10002, "_ab_jump_bird");
    damage_number(lua, -2, "x", x.bits);
    damage_number(lua, -2, "y", y.bits);
    return 0;
}

static int jump_rotation(void *lua) {
    if (damage_lua.get_top(lua) != 2 || !damage_named(lua, 1, "first")) {
        fail("Unexpected original jump rotation boundary\n");
    }
    uint32_t *event = jump_event(2);
    FloatBits angle = {.number = damage_lua.to_number(lua, 2)};
    event[1] = angle.bits;
    damage_lua.get_field(lua, -10002, "_ab_jump_bird");
    damage_number(lua, -2, "angle", angle.bits);
    return 0;
}

static void jump_prepare(void *lua) {
    damage_lua.create_table(lua, 0, 16);
    for (size_t field = 0; field < 10; ++field) {
        damage_number(lua, 1, jump_fields[field], jump_input[field]);
    }
    const char *names[] = {"name", "definition"};
    const char *values[] = {"first", "base"};
    for (size_t field = 0; field < 2; ++field) {
        damage_lua.push_string(lua, values[field]);
        damage_lua.set_field(lua, 1, names[field]);
    }
    damage_lua.push_boolean(lua, (jump_input[10] & 1) != 0);
    damage_lua.set_field(lua, 1, "jumpOn");
    damage_lua.push_boolean(lua, (jump_input[10] & 2) != 0);
    damage_lua.set_field(lua, 1, "jumpRebound");
    damage_lua.push_number(lua, 100000000);
    damage_lua.set_field(lua, 1, "animTimer");
    damage_lua.set_field(lua, -10002, "_ab_jump_bird");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_jump_bird");
    damage_lua.set_field(lua, 1, "first");
    damage_lua.set_field(lua, -10002, "birds");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_boolean(lua, (jump_input[10] & 4) != 0);
    damage_lua.set_field(lua, 3, "allowRoll");
    damage_lua.set_field(lua, 2, "base");
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.set_field(lua, -10002, "blockTable");
    damage_lua.push_boolean(lua, (jump_input[10] & 8) != 0);
    damage_lua.set_field(lua, -10002, "levelCompleted");
}

static void probe_bird_jump(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original jump reference state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/jump/bird-jump-oracle.lua",
                           "_ab_jump_update");
    damage_function(lua, -10002, "setPosition", jump_position);
    damage_function(lua, -10002, "setRotation", jump_rotation);
    damage_lua.get_field(lua, -10002, "math");
    damage_function(lua, 1, "random", jump_random);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "levelGoals");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "g_birdHats");
    damage_lua.push_string(lua, "held");
    damage_lua.set_field(lua, -10002, "currentBirdName");
    uint32_t random = UINT32_C(0x5201d1e5);
#if defined(AB_ORACLE_BIRD_JUMP_OVERSHOOT)
    semihost(4, "AB_ANDROID_BIRD_JUMP_OVERSHOOT_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_BIRD_JUMP_V1 4096\n");
#endif
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 10; ++field) {
            jump_input[field] = random_float(&random, 122, 8, 1);
        }
        FloatBits timer = {.number = (float)((int)(item % 9) - 4) * 0.1f};
        FloatBits height = {.number = 0.4f + (float)(item % 13) * 0.1f};
        FloatBits speed = {.number = (float)(item % 29 + 1)};
        FloatBits roll = {.number = (float)((int)(item % 5) - 1)};
        FloatBits seconds = {.number = (float)(item % 17) / 100.0f};
        jump_input[3] = timer.bits;
        jump_input[4] = height.bits;
        jump_input[5] = speed.bits;
        jump_input[7] = roll.bits;
        jump_input[10] = (uint32_t)item & 15;
        jump_input[11] = seconds.bits;
        jump_input[12] = random_word(&random);
        if (item < 32) {
            jump_input[3] = item & 16 ? UINT32_C(0x80000000) : 0;
            jump_input[6] = UINT32_C(0x40490fda) + (uint32_t)item % 3;
            jump_input[11] = 0;
        }
        jump_prepare(lua);
        jump_event_count = 0;
        jump_random_state = jump_input[12];
        print_words(jump_input, 13);
        damage_lua.get_field(lua, -10002, "_ab_jump_update");
        damage_lua.push_number(lua, seconds.number);
        if (item < 32) {
            damage_lua.set_top(lua, 1);
            damage_lua.push_number(lua, 0);
        }
        if (damage_lua.call(lua, 1, 0, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original jump update failed\n");
        }
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_jump_bird");
        uint32_t output[13];
        for (size_t field = 0; field < 10; ++field) {
            damage_lua.get_field(lua, 1, jump_fields[field]);
            if (damage_lua.type(lua, -1) != 3) {
                fail("Original jump numeric field changed type\n");
            }
            FloatBits value = {.number = damage_lua.to_number(lua, -1)};
            output[field] = value.bits;
            damage_lua.set_top(lua, 1);
        }
        const char *flags[] = {"jumpOn", "jumpRebound"};
        for (size_t field = 0; field < 2; ++field) {
            damage_lua.get_field(lua, 1, flags[field]);
            if (damage_lua.type(lua, -1) != 1) {
                fail("Original jump boolean field changed type\n");
            }
            output[field + 10] = (uint32_t)damage_lua.to_boolean(lua, -1);
            damage_lua.set_top(lua, 1);
        }
        output[12] = jump_random_state;
        damage_lua.set_top(lua, 0);
        print_words(output, 13);
        uint32_t count = (uint32_t)jump_event_count;
        print_words(&count, 1);
        for (size_t event = 0; event < jump_event_count; ++event) {
            print_words(jump_events[event], 4);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird jump reference complete\n");
}

#endif
