#ifndef AB_ORACLE_GOAL_ANIMATION_H
#define AB_ORACLE_GOAL_ANIMATION_H

static uint32_t goal_input[12], goal_events[16][10], goal_random_state;
static size_t goal_event_count, goal_draw_count;

static void goal_snapshot(void *lua, uint32_t words[6]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_goal");
    const char *fields[] = {"specialExpressionTimer", "blinkTimer", "oinkTimer",
                            "specialExpression",      "blinkOn",    "strength"};
    for (size_t index = 0; index < 6; ++index) {
        damage_lua.get_field(lua, top + 1, fields[index]);
        if (index == 3 || index == 4) {
            words[index] = (uint32_t)damage_lua.to_boolean(lua, -1);
        } else {
            FloatBits value = {.number = damage_lua.to_number(lua, -1)};
            words[index] = value.bits;
        }
        damage_lua.set_top(lua, top + 1);
    }
    damage_lua.set_top(lua, top);
}

static uint32_t *goal_event(void *lua, uint32_t kind) {
    if (goal_event_count >= 16) {
        fail("Original goal animation event overflow\n");
    }
    uint32_t *event = goal_events[goal_event_count++];
    event[0] = kind;
    event[1] = event[2] = event[3] = 0;
    goal_snapshot(lua, event + 4);
    return event;
}

static void goal_mutate(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_goal");
    const char *fields[] = {"specialExpressionTimer", "blinkTimer", "oinkTimer"};
    for (size_t index = 0; index < 3; ++index) {
        damage_number(lua, top + 1, fields[index], goal_input[11]);
    }
    damage_lua.push_boolean(lua, !(goal_input[8] & 1));
    damage_lua.set_field(lua, top + 1, "specialExpression");
    damage_lua.push_boolean(lua, !(goal_input[8] & (2 | 256)));
    damage_lua.set_field(lua, top + 1, "blinkOn");
    if (goal_input[10] & 16) {
        damage_lua.push_boolean(lua, 1);
        damage_lua.set_field(lua, top + 1, "frozenSolid");
    }
    damage_lua.set_top(lua, top);
}

static int goal_random(void *lua) {
    int low = (int)damage_lua.to_number(lua, 1);
    int high = (int)damage_lua.to_number(lua, 2);
    if (damage_lua.get_top(lua) != 2 || low < 1 || high < low || high > 60) {
        fail("Unexpected goal animation random interval\n");
    }
    uint32_t value =
        (uint32_t)low + random_word(&goal_random_state) % ((uint32_t)(high - low) + 1);
    if ((goal_input[8] & 64) && !goal_draw_count) {
        value = (uint32_t)high + 1;
    }
    ++goal_draw_count;
    uint32_t *event = goal_event(lua, 1);
    event[1] = (uint32_t)low;
    event[2] = (uint32_t)high;
    event[3] = value;
    damage_lua.push_number(lua, (float)value);
    return 1;
}

static int goal_lose(void *lua) {
    goal_event(lua, 2);
    if (goal_input[10] & 1) {
        goal_mutate(lua);
    }
    if (goal_input[8] & 128) {
        damage_lua.push_number(lua, 0);
    } else {
        damage_lua.push_boolean(lua, goal_input[8] & 8);
    }
    return 1;
}

static int goal_sprite(void *lua) {
    if (damage_lua.get_top(lua) != 2 || !damage_named(lua, 1, "goal")) {
        fail("Unexpected goal animation sprite boundary\n");
    }
    uint32_t *event = goal_event(lua, 3);
    event[1] = damage_named(lua, 2, "special")  ? 1u
               : damage_named(lua, 2, "smile")  ? 2u
               : damage_named(lua, 2, "damage") ? 3u
               : damage_named(lua, 2, "blink")  ? 4u
                                                : 0;
    if (!event[1]) {
        fail("Unexpected goal animation sprite identifier\n");
    }
    if (goal_input[10] & 2) {
        goal_mutate(lua);
    }
    return 0;
}

static int goal_transform(void *lua) {
    damage_lua.push_number(lua, 0);
    damage_lua.push_number(lua, 0);
    return 2;
}

static int goal_zero(void *lua) {
    damage_lua.push_number(lua, 0);
    return 1;
}

static int goal_distance(void *lua) {
    goal_event(lua, 4)[1] = goal_input[7];
    if (goal_input[10] & 4) {
        goal_mutate(lua);
    }
    FloatBits distance = {.bits = goal_input[7]};
    damage_lua.push_number(lua, distance.number);
    return 1;
}

static int goal_audio_name(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "pigs")) {
        fail("Unexpected goal animation sound group\n");
    }
    goal_event(lua, 5);
    damage_lua.push_string(lua, "audio");
    return 1;
}

static int goal_audio(void *lua) {
    if (damage_lua.get_top(lua) != 4 || !damage_named(lua, 1, "audio")) {
        fail("Unexpected goal animation audio boundary\n");
    }
    uint32_t *event = goal_event(lua, 6);
    FloatBits volume = {.number = damage_lua.to_number(lua, 2)};
    FloatBits delay = {.number = damage_lua.to_number(lua, 4)};
    event[1] = volume.bits;
    event[2] = (uint32_t)damage_lua.to_boolean(lua, 3);
    event[3] = delay.bits;
    if (goal_input[10] & 8) {
        goal_mutate(lua);
    }
    return 0;
}

static void goal_prepare(void *lua) {
    uint32_t flags = goal_input[8];
    damage_lua.create_table(lua, 0, 12);
    damage_number(lua, 1, "strength", goal_input[0]);
    damage_number(lua, 1, "specialExpressionTimer", goal_input[3]);
    damage_number(lua, 1, "blinkTimer", goal_input[4]);
    damage_number(lua, 1, "oinkTimer", goal_input[5]);
    damage_number(lua, 1, "x", 0);
    damage_number(lua, 1, "y", 0);
    if (flags & 1) {
        damage_lua.push_string(lua, "special");
    } else if (flags & 512) {
        damage_lua.push_boolean(lua, 0);
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, 1, "specialExpression");
    if (flags & 256) {
        damage_lua.push_number(lua, 0);
    } else {
        damage_lua.push_boolean(lua, flags & 2);
    }
    damage_lua.set_field(lua, 1, "blinkOn");
    const char *boolean_fields[] = {"disableSmiling", "overrideDrawing", "frozenSolid"};
    const uint32_t boolean_bits[] = {4, 16, 32};
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.push_boolean(lua, (flags & boolean_bits[index]) != 0);
        damage_lua.set_field(lua, 1, boolean_fields[index]);
    }
    const char *fields[] = {"definition", "smileSprite", "damageSprite", "blinkSprite"};
    const char *values[] = {"base", "smile", "damage", "blink"};
    for (size_t index = 0; index < 4; ++index) {
        damage_lua.push_string(lua, values[index]);
        damage_lua.set_field(lua, 1, fields[index]);
    }
    damage_lua.set_field(lua, -10002, "_ab_goal");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_goal");
    damage_lua.set_field(lua, 1, "goal");
    damage_lua.set_field(lua, -10002, "levelGoals");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 3, "strength", goal_input[1]);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_number(lua, 5, "min", goal_input[2]);
    damage_lua.set_field(lua, 4, "damage1");
    damage_lua.set_field(lua, 3, "damageSprites");
    damage_lua.set_field(lua, 2, "base");
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.set_field(lua, -10002, "blockTable");
}

static void probe_goal_animation(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original goal animation Lua state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/jump/bird-jump-oracle.lua",
                           "_ab_goal_update");
    damage_lua.get_field(lua, -10002, "math");
    damage_function(lua, 1, "random", goal_random);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "g_game_lose_condition", goal_lose);
    damage_function(lua, -10002, "setSprite", goal_sprite);
    damage_function(lua, -10002, "physicsToWorldTransform", goal_transform);
    damage_function(lua, -10002, "vLength", goal_distance);
    damage_function(lua, -10002, "getAudioName", goal_audio_name);
    damage_function(lua, -10002, "playAudio", goal_audio);
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "getScreenX", goal_zero);
    damage_function(lua, 1, "getScreenY", goal_zero);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    damage_lua.push_string(lua, "pigs");
    damage_lua.set_field(lua, -10002, "g_sound_pigs");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "birds");
    uint32_t random = UINT32_C(0x52090a11);
    semihost(4, "AB_ANDROID_GOAL_ANIMATION_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        FloatBits strength = {.number = (float)((int)(item % 19) - 3)};
        FloatBits base = {.number = item % 23 == 0 ? 0 : 10};
        FloatBits threshold = {.number = (float)(item % 11) * 10.0f};
        FloatBits special = {.number = (float)((int)(item % 7) - 3) / 10.0f};
        FloatBits blink = {.number = (float)((int)(item % 9) - 4) / 10.0f};
        FloatBits oink = {.number = (float)((int)(item % 13) - 6) / 10.0f};
        FloatBits seconds = {.number = (float)(item % 17) / 10.0f};
        FloatBits distance = {.number = (float)((int)(item % 11) - 2) * 250.0f};
        if (item < 64) {
            special.bits = blink.bits = oink.bits =
                (uint32_t)(item % 4) | (item & 16 ? UINT32_C(0x80000000) : 0);
            seconds.bits = 0;
            distance.bits = UINT32_C(0x447a0000) + (uint32_t)(item % 3) - 1;
        }
        uint32_t input[] = {strength.bits,
                            base.bits,
                            threshold.bits,
                            special.bits,
                            blink.bits,
                            oink.bits,
                            seconds.bits,
                            distance.bits,
                            (uint32_t)item & 1023,
                            random_word(&random),
                            (uint32_t)(item / 128) & 31,
                            random_float(&random, 123, 4, 1)};
        for (size_t index = 0; index < 12; ++index) {
            goal_input[index] = input[index];
        }
        goal_prepare(lua);
        goal_event_count = goal_draw_count = 0;
        goal_random_state = goal_input[9];
        damage_lua.get_field(lua, -10002, "_ab_goal_update");
        damage_lua.push_number(lua, seconds.number);
        if (damage_lua.call(lua, 1, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original goal animation call failed\n");
        }
        print_words(goal_input, 12);
        uint32_t output[8];
        goal_snapshot(lua, output);
        output[6] = goal_random_state;
        output[7] = (uint32_t)goal_event_count;
        print_words(output, 8);
        for (size_t event = 0; event < goal_event_count; ++event) {
            print_words(goal_events[event], 10);
        }
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB goal animation reference complete\n");
}

#endif
