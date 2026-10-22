#ifndef AB_ORACLE_LEVEL_COMPLETE_H
#define AB_ORACLE_LEVEL_COMPLETE_H

static uint32_t complete_input[18], complete_events[7][10];
static size_t complete_event_count;

static int complete_script(void *lua);

static void complete_kind(void *lua, uint32_t kind) {
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

static void complete_hook(void *lua, int present) {
    int top = damage_lua.get_top(lua);
    damage_lua.create_table(lua, 0, 1);
    if (present) {
        damage_lua.create_table(lua, 0, 1);
        damage_function(lua, top + 2, "onInitLevelComplete", complete_script);
        damage_lua.set_field(lua, top + 1, "first");
    }
    damage_lua.set_field(lua, -10002, "g_level_scripts");
}

static void complete_flag(void *lua, const char *name, int value) {
    damage_lua.push_boolean(lua, value);
    damage_lua.set_field(lua, -10002, name);
}

static void complete_snapshot(void *lua, uint32_t output[8]) {
    int top = damage_lua.get_top(lua);
    const char *numbers[] = {"currentBirdIndex", "birdsShot", "birdsLeftCounter",
                             "birdBuffTimer", "levelCompleteTimer"};
    for (size_t field = 0; field < 5; ++field) {
        damage_lua.get_field(lua, -10002, numbers[field]);
        if (damage_lua.type(lua, -1) != 3) {
            fail("Original completion numeric field changed type\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[field] = value.bits;
        damage_lua.set_top(lua, top);
    }
    const char *flags[] = {"levelCompleted", "g_level_completing", "eagleBaitLaunched"};
    for (size_t field = 0; field < 3; ++field) {
        damage_lua.get_field(lua, -10002, flags[field]);
        output[5 + field] = (uint32_t)damage_lua.to_boolean(lua, -1);
        damage_lua.set_top(lua, top);
    }
}

static void complete_event(void *lua, uint32_t operation, uint32_t argument) {
    if (complete_event_count >= 7) {
        fail("Original completion event overflow\n");
    }
    uint32_t *event = complete_events[complete_event_count++];
    event[0] = operation;
    event[1] = argument;
    complete_snapshot(lua, event + 2);
    if (!(complete_input[14] & (UINT32_C(1) << (operation - 1)))) {
        return;
    }
    uint32_t flags = complete_input[17];
    if (operation == 1 || operation == 2) {
        damage_number(lua, -10002, "currentBirdIndex", complete_input[15]);
        damage_number(lua, -10002, "birdsShot", complete_input[16]);
        if (flags & 1) {
            damage_lua.push_string(lua, "first");
        } else {
            damage_lua.push_nil(lua);
        }
        damage_lua.set_field(lua, -10002, "currentBirdName");
        int top = damage_lua.get_top(lua);
        damage_lua.get_field(lua, -10002, "_ab_completion_bird");
        damage_lua.push_boolean(lua, (flags & 2) != 0);
        damage_lua.set_field(lua, -2, "shot");
        damage_lua.set_top(lua, top);
        complete_flag(lua, "g_special_game_mode", (flags & 4) != 0);
        complete_flag(lua, "g_disable_bird_score", (flags & 8) != 0);
    } else if (operation == 3) {
        damage_number(lua, -10002, "birdsLeftCounter", complete_input[15]);
    } else if (operation == 4) {
        damage_number(lua, -10002, "birdBuffTimer", complete_input[16]);
        complete_flag(lua, "levelCompleted", 0);
    } else if (operation == 5) {
        complete_flag(lua, "eagleBaitLaunched", (flags & 16) != 0);
        complete_hook(lua, (flags & 32) != 0);
    } else if (operation == 6) {
        complete_flag(lua, "eagleBaitLaunched", (flags & 16) == 0);
        complete_flag(lua, "levelCompleted", 0);
    } else if (operation == 7) {
        damage_number(lua, -10002, "levelCompleteTimer", complete_input[15]);
        complete_flag(lua, "g_level_completing", 0);
    }
}

static int complete_notify(void *lua) {
    if (damage_lua.get_top(lua) != 2) {
        fail("Unexpected original completion notification arguments\n");
    }
    damage_lua.get_field(lua, 2, "id");
    if (!damage_named(lua, -1, "complete")) {
        fail("Unexpected original completion notification identifier\n");
    }
    damage_lua.set_top(lua, 2);
    complete_event(lua, 1, 0);
    return 0;
}

static int complete_cancel(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original completion drag cancellation\n");
    }
    complete_event(lua, 2, 0);
    return 0;
}

static int complete_query(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 3) {
        fail("Unexpected original completion next-bird query\n");
    }
    FloatBits index = {.number = damage_lua.to_number(lua, 1)};
    complete_event(lua, 3, index.bits);
    complete_kind(lua, complete_input[13]);
    return 1;
}

static int complete_camera(void *lua) {
    if (damage_lua.get_top(lua) != 2 || !damage_named(lua, 2, "LevelEndSequence")) {
        fail("Unexpected original completion camera sequence\n");
    }
    complete_event(lua, 4, 0);
    return 0;
}

static int complete_audio_name(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "clear")) {
        fail("Unexpected original completion sound lookup\n");
    }
    complete_event(lua, 5, 0);
    damage_lua.push_string(lua, "sound");
    return 1;
}

static int complete_audio(void *lua) {
    if (damage_lua.get_top(lua) != 3 || !damage_named(lua, 1, "sound") ||
        damage_lua.to_number(lua, 2) != 1 || damage_lua.type(lua, 3) != 1 ||
        damage_lua.to_boolean(lua, 3)) {
        fail("Unexpected original completion playback arguments\n");
    }
    complete_event(lua, 6, 0);
    return 0;
}

static int complete_script(void *lua) {
    if (damage_lua.get_top(lua) != 0) {
        fail("Unexpected original completion level-hook arguments\n");
    }
    complete_event(lua, 7, 0);
    return 0;
}

static void complete_prepare(void *lua) {
    const char *numbers[] = {"currentBirdIndex", "birdsShot", "birdsLeftCounter",
                             "birdBuffTimer", "levelCompleteTimer"};
    for (size_t field = 0; field < 5; ++field) {
        damage_number(lua, -10002, numbers[field], complete_input[field]);
    }
    const char *flags[] = {"g_special_game_mode", "g_disable_bird_score",
                           "eagleBaitLaunched", "levelCompleted", "g_level_completing"};
    const size_t indices[] = {7, 8, 9, 11, 12};
    for (size_t field = 0; field < 5; ++field) {
        complete_kind(lua, complete_input[indices[field]]);
        damage_lua.set_field(lua, -10002, flags[field]);
    }
    complete_hook(lua, (int)complete_input[10]);
    damage_lua.create_table(lua, 0, 1);
    complete_kind(lua, complete_input[6]);
    damage_lua.set_field(lua, 1, "shot");
    damage_lua.set_field(lua, -10002, "_ab_completion_bird");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_completion_bird");
    damage_lua.set_field(lua, 2, "first");
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_field(lua, -10002, "objects");
    if (complete_input[5]) {
        damage_lua.push_string(lua, "first");
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, -10002, "currentBirdName");
}

static void probe_level_complete(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original completion reference state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(
        lua, "/data/local/tmp/ab520/complete/init-level-complete-oracle.lua",
        "_ab_completion");
    damage_lua.get_field(lua, -10002, "_ab_completion");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "cancelBirdDrag", complete_cancel);
    damage_lua.set_field(lua, -10002, "_ab_completion_sling");
    damage_lua.get_field(lua, -10002, "_ab_completion_sling");
    /* Stripped debug names make lua_setupvalue reject this valid closure slot.
     * Bind only the fresh closed nil UpVal through the pinned ARM32 layout.
     * The controlled table is rooted globally before binding. No prototype or
     * original instruction is modified; this is an oracle environment input. */
    const uint8_t *closure = damage_lua.to_pointer(lua, 1);
    const void *sling = damage_lua.to_pointer(lua, 2);
    if (!closure || !sling || closure[6] || closure[7] != 1) {
        fail("Unexpected selected completion closure layout\n");
    }
    const uint32_t *upvalue =
        (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[5];
    if (!upvalue || upvalue[2] != (uint32_t)(uintptr_t)(upvalue + 3)) {
        fail("Expected a fresh closed completion upvalue\n");
    }
    uint32_t *value = (uint32_t *)(uintptr_t)upvalue[2];
    if (value[1] != 0) {
        fail("Expected an initially nil completion upvalue\n");
    }
    value[0] = (uint32_t)(uintptr_t)sling;
    value[1] = 5;
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "getNextBird", complete_query);
    damage_function(lua, -10002, "getAudioName", complete_audio_name);
    damage_function(lua, -10002, "playAudio", complete_audio);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, -10002, "levelName");
    damage_lua.push_string(lua, "clear");
    damage_lua.set_field(lua, -10002, "g_sound_level_clear");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "notify", complete_notify);
    damage_lua.set_field(lua, -10002, "eventManager");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "complete");
    damage_lua.set_field(lua, 1, "EID_LEVEL_COMPLETE_INIT");
    damage_lua.set_field(lua, -10002, "events");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "startCameraSequenceFromFile", complete_camera);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    uint32_t random = UINT32_C(0x520c0a05);
    semihost(4, "AB_ANDROID_LEVEL_COMPLETE_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 18; ++field) {
            complete_input[field] = random_word(&random) % 5;
        }
        for (size_t field = 0; field < 5; ++field) {
            complete_input[field] = random_float(&random, 0, 145, 1);
        }
        FloatBits shots = {.number = (float)((int)(item % 3) - 1)};
        complete_input[1] = shots.bits;
        complete_input[5] &= 1;
        complete_input[10] &= 1;
        complete_input[14] = item < 1024 ? 0 : random_word(&random) & 127;
        complete_input[15] = random_float(&random, 0, 145, 1);
        complete_input[16] = shots.bits;
        complete_input[17] = random_word(&random) & 63;
        complete_prepare(lua);
        complete_event_count = 0;
        print_words(complete_input, 18);
        damage_lua.get_field(lua, -10002, "_ab_completion");
        if (damage_lua.call(lua, 0, 0, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original completion initialization failed\n");
        }
        uint32_t output[8];
        complete_snapshot(lua, output);
        print_words(output, 8);
        uint32_t count = (uint32_t)complete_event_count;
        print_words(&count, 1);
        for (size_t event = 0; event < complete_event_count; ++event) {
            print_words(complete_events[event], 10);
        }
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB level complete reference complete\n");
}

#endif
