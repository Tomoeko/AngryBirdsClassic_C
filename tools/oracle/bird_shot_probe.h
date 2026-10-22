#ifndef AB_ORACLE_BIRD_SHOT_H
#define AB_ORACLE_BIRD_SHOT_H

static uint32_t shot_input[19], shot_events[24][23];
static size_t shot_event_count;
static uint32_t *shot_dragged, *shot_selection;

static void shot_call(void *lua, int arguments, int results) {
    if (damage_lua.call(lua, arguments, results, 0)) {
        const char *error = damage_lua.to_string(lua, -1, NULL);
        fail(error ? error : "Original bird shot call failed\n");
    }
}

static uint32_t shot_numeric(void *lua, int table, const char *name) {
    damage_lua.get_field(lua, table, name);
    if (damage_lua.type(lua, -1) != 3) {
        fail("Original bird shot field is not numeric\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.set_top(lua, damage_lua.get_top(lua) - 1);
    return value.bits;
}

static uint32_t shot_truth(void *lua, int table, const char *name) {
    damage_lua.get_field(lua, table, name);
    uint32_t value = (uint32_t)(damage_lua.to_boolean(lua, -1) != 0);
    damage_lua.set_top(lua, damage_lua.get_top(lua) - 1);
    return value;
}

static uint32_t shot_text(void *lua, int index) {
    return damage_named(lua, index, "hook")       ? 1u
           : damage_named(lua, index, "flying")   ? 2u
           : damage_named(lua, index, "sound")    ? 3u
           : damage_named(lua, index, "resolved") ? 4u
           : damage_named(lua, index, "idle")     ? 5u
                                                  : 0;
}

static void shot_snapshot(void *lua, uint32_t output[15]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_shot_bird");
    int bird = top + 1;
    uint32_t flags = shot_truth(lua, bird, "shot") |
                     (shot_truth(lua, bird, "hasCollided") << 1) |
                     (shot_truth(lua, -10002, "birdSpecialtyAvailable") << 2) |
                     (shot_truth(lua, -10002, "hasMovingObjectsAboveTolerance") << 3) |
                     ((uint32_t)(shot_selection[0] != 0) << 4) |
                     (shot_truth(lua, -10002, "birdReady") << 5) |
                     (shot_truth(lua, -10002, "birdFired") << 6) |
                     ((uint32_t)(shot_dragged[1] == 5) << 7) |
                     (shot_truth(lua, -10002, "flyingBird") << 8) |
                     (shot_truth(lua, -10002, "currentBirdName") << 9);
    damage_lua.get_field(lua, -10002, "otherBirds");
    flags |= shot_truth(lua, -1, "old") << 10;
    damage_lua.set_top(lua, bird);
    output[0] = flags;
    const char *globals[] = {"birdsShot", "extraTutorialTimer", "lastScoreTime",
                             "oldScore"};
    output[1] = shot_numeric(lua, -10002, globals[0]);
    output[2] = shot_numeric(lua, -10002, globals[1]);
    damage_lua.get_field(lua, -10002, "_ab_shot_self");
    output[3] = shot_numeric(lua, -1, "nextBirdTimer");
    damage_lua.set_top(lua, bird);
    output[4] = shot_numeric(lua, -10002, globals[2]);
    output[5] = shot_numeric(lua, -10002, globals[3]);
    const char *fields[] = {"mass", "startImpulseX", "startImpulseY", "x", "y"};
    for (size_t index = 0; index < 5; ++index) {
        output[6 + index] = shot_numeric(lua, bird, fields[index]);
    }
    damage_lua.get_field(lua, bird, "launchVelocity");
    output[11] = shot_numeric(lua, -1, "x");
    output[12] = shot_numeric(lua, -1, "y");
    damage_lua.set_top(lua, bird);
    damage_lua.get_field(lua, bird, "sprite");
    output[13] = shot_text(lua, -1);
    damage_lua.set_top(lua, bird);
    output[14] = shot_numeric(lua, -10002, "score");
    damage_lua.set_top(lua, top);
}

static uint32_t *shot_event(void *lua, uint32_t operation) {
    if (shot_event_count >= 24) {
        fail("Original bird shot boundary overflow\n");
    }
    uint32_t *event = shot_events[shot_event_count++];
    for (size_t field = 0; field < 23; ++field) {
        event[field] = 0;
    }
    event[0] = operation;
    shot_snapshot(lua, event + 7);
    return event;
}

static void shot_argument(void *lua, uint32_t *event, size_t field, int argument) {
    if (damage_lua.type(lua, argument) != 3) {
        fail("Original shot boundary argument is not numeric\n");
    }
    FloatBits value = {.number = damage_lua.to_number(lua, argument)};
    event[field] = value.bits;
}

static void shot_change(void *lua, const char *global, const char *field, float scale,
                        float offset) {
    damage_lua.get_field(lua, -10002, global);
    FloatBits value = {.bits = shot_numeric(lua, -1, field)};
    value.number = value.number * scale;
    value.number = value.number + offset;
    damage_number(lua, -2, field, value.bits);
    damage_lua.set_top(lua, damage_lua.get_top(lua) - 1);
}

static void shot_change_global(void *lua, const char *field, float offset) {
    FloatBits value = {.bits = shot_numeric(lua, -10002, field)};
    value.number = value.number + offset;
    damage_number(lua, -10002, field, value.bits);
}

static int shot_position(void *lua) {
    uint32_t *event = shot_event(lua, 1);
    shot_argument(lua, event, 1, 2);
    shot_argument(lua, event, 2, 3);
    damage_lua.get_field(lua, -10002, "_ab_shot_bird");
    damage_number(lua, -2, "x", event[1]);
    damage_number(lua, -2, "y", event[2]);
    return 0;
}

static int shot_velocity(void *lua) {
    uint32_t *event = shot_event(lua, 2);
    shot_argument(lua, event, 1, 2);
    shot_argument(lua, event, 2, 3);
    return 0;
}

static int shot_count(void *lua) {
    (void)shot_event(lua, 3);
    FloatBits count = {.bits = shot_input[16]};
    damage_lua.push_number(lua, count.number);
    return 1;
}

static int shot_backwards(void *lua) {
    (void)shot_event(lua, 4);
    FloatBits count = {.bits = shot_input[17]};
    damage_lua.push_number(lua, count.number);
    return 1;
}

static int shot_notify(void *lua) {
    uint32_t id = shot_numeric(lua, 2, "id");
    bool bird = id == UINT32_C(0x3f800000);
    if (!bird && id != UINT32_C(0x40000000)) {
        fail("Unexpected original shot event identifier\n");
    }
    uint32_t *event = shot_event(lua, bird ? 5u : 10u);
    if (bird) {
        event[5] = shot_numeric(lua, 2, "birdsShooted");
        event[6] = shot_numeric(lua, 2, "backwardsBirdCount");
        damage_lua.get_field(lua, 2, "launchPosition");
        event[3] = shot_numeric(lua, -1, "x");
        event[4] = shot_numeric(lua, -1, "y");
        damage_lua.get_field(lua, 2, "launchImpulse");
        event[1] = shot_numeric(lua, -1, "x");
        event[2] = shot_numeric(lua, -1, "y");
        if (shot_input[18] & 128) {
            shot_change(lua, "_ab_shot_impulse", "x", -1, 0);
            shot_change(lua, "_ab_shot_impulse", "y", 1, 0.25f);
            shot_change(lua, "_ab_shot_position", "x", 1, 2);
            shot_change(lua, "_ab_shot_position", "y", 1, -3);
            shot_change(lua, "_ab_shot_bird", "x", 1, 0.5f);
            shot_change(lua, "_ab_shot_bird", "y", 1, -0.75f);
            shot_change(lua, "_ab_shot_bird", "mass", 2, 0);
        }
    }
    return 0;
}

static int shot_apply_impulse(void *lua) {
    uint32_t *event = shot_event(lua, 6);
    for (int argument = 2; argument <= 5; ++argument) {
        shot_argument(lua, event, (size_t)argument - 1, argument);
    }
    return 0;
}

static int shot_level(void *lua) {
    (void)shot_event(lua, 7);
    if (shot_input[18] & 256) {
        shot_change(lua, "_ab_shot_bird", "mass", 0.5f, 0);
        shot_change_global(lua, "score", 125);
        shot_change(lua, "_ab_shot_self", "birdReloadDelay", 1, 0.125f);
        shot_change(lua, "_ab_shot_impulse", "x", 1, 1.25f);
    }
    return 0;
}

static int shot_definition(void *lua) {
    (void)shot_event(lua, 8);
    damage_lua.create_table(lua, 0, 3);
    const char *fields[] = {"onBirdShot", "spriteFlying", "launchSound"};
    const char *values[] = {"hook", "flying", "sound"};
    const uint32_t flags[] = {2, 4, 16};
    for (size_t index = 0; index < 3; ++index) {
        if (shot_input[18] & flags[index]) {
            damage_lua.push_string(lua, values[index]);
            damage_lua.set_field(lua, -2, fields[index]);
        }
    }
    return 1;
}

static int shot_hook(void *lua) {
    shot_event(lua, 9)[22] = 1;
    damage_number(lua, -10002, "extraTutorialTimer", UINT32_C(0x4479c000));
    return 0;
}

static int shot_sprite(void *lua) {
    shot_event(lua, 11)[22] = shot_text(lua, 2);
    return 0;
}

static int shot_particles(void *lua) {
    (void)shot_event(lua, 12);
    return 0;
}

static int shot_resolve_audio(void *lua) {
    shot_event(lua, 13)[22] = shot_text(lua, 1);
    damage_lua.push_string(lua, "resolved");
    return 1;
}

static int shot_play_audio(void *lua) {
    uint32_t *event = shot_event(lua, 14);
    event[22] = shot_text(lua, 1);
    shot_argument(lua, event, 5, 2);
    if (damage_lua.type(lua, 3) != 1 || damage_lua.to_boolean(lua, 3)) {
        fail("Unexpected original shot audio loop argument\n");
    }
    return 0;
}

static int shot_sling_sound(void *lua) {
    (void)shot_event(lua, 15);
    return 0;
}

static int shot_script(void *lua) {
    uint32_t *event = shot_event(lua, 16);
    event[1] = shot_numeric(lua, 2, "x");
    event[2] = shot_numeric(lua, 2, "y");
    if (shot_input[18] & 512) {
        shot_change(lua, "_ab_shot_bird", "mass", 4, 0);
        shot_change(lua, "_ab_shot_impulse", "y", 1, -0.5f);
    }
    return 0;
}

static void shot_vector(void *lua, const char *name, const uint32_t bits[2]) {
    damage_lua.get_field(lua, -10002, "lua");
    damage_lua.get_field(lua, 1, "Vector2");
    damage_lua.get_field(lua, 2, "new");
    damage_lua.get_field(lua, 1, "Vector2");
    for (size_t field = 0; field < 2; ++field) {
        FloatBits value = {.bits = bits[field]};
        damage_lua.push_number(lua, value.number);
    }
    shot_call(lua, 3, 1);
    damage_lua.set_field(lua, -10002, name);
    damage_lua.set_top(lua, 0);
}

static void prepare_shot(void *lua) {
    uint32_t zeros[2] = {0};
    shot_vector(lua, "_ab_shot_position", shot_input);
    shot_vector(lua, "_ab_shot_impulse", shot_input + 2);
    shot_vector(lua, "_ab_zero_vector", zeros);
    damage_lua.create_table(lua, 0, 12);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, 1, "name");
    damage_lua.push_string(lua, "idle");
    damage_lua.set_field(lua, 1, "sprite");
    damage_number(lua, 1, "x", shot_input[4]);
    damage_number(lua, 1, "y", shot_input[5]);
    damage_number(lua, 1, "mass", shot_input[6]);
    damage_number(lua, 1, "startImpulseX", 0);
    damage_number(lua, 1, "startImpulseY", 0);
    damage_lua.get_field(lua, -10002, "_ab_zero_vector");
    damage_lua.set_field(lua, 1, "launchVelocity");
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 1, "hasCollided");
    damage_lua.push_boolean(lua, (shot_input[18] & 8) != 0);
    damage_lua.set_field(lua, 1, "overrideDrawing");
    if (shot_input[18] & 32) {
        damage_lua.create_table(lua, 0, 1);
        damage_function(lua, -2, "onLaunch", shot_script);
        damage_lua.set_field(lua, 1, "gameObjectScript");
    }
    damage_lua.set_field(lua, -10002, "_ab_shot_bird");
    const char *globals[] = {
        "birdsShot", "birdCount",     "_unused",  "time",
        "score",     "lastScoreTime", "oldScore", "extraTutorialTimer"};
    for (size_t index = 0; index < 8; ++index) {
        damage_number(lua, -10002, globals[index], shot_input[index + 7]);
    }
    const char *booleans[] = {"birdSpecialtyAvailable",
                              "hasMovingObjectsAboveTolerance", "birdReady",
                              "birdFired"};
    for (size_t index = 0; index < 4; ++index) {
        damage_lua.push_boolean(lua, index == 2);
        damage_lua.set_field(lua, -10002, booleans[index]);
    }
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, -10002, "currentBirdName");
    damage_lua.push_nil(lua);
    damage_lua.set_field(lua, -10002, "flyingBird");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 1, "old");
    damage_lua.set_field(lua, -10002, "otherBirds");
    damage_lua.create_table(lua, 0, 1);
    if (shot_input[18] & 1) {
        damage_lua.create_table(lua, 0, 1);
        damage_function(lua, -2, "onBirdShot", shot_level);
        damage_lua.set_field(lua, 1, "level");
    }
    damage_lua.set_field(lua, -10002, "g_level_scripts");
    damage_lua.push_string(lua, "level");
    damage_lua.set_field(lua, -10002, "levelName");
    damage_lua.create_table(lua, 0, 4);
    damage_number(lua, 1, "birdReloadDelay", shot_input[9]);
    damage_number(lua, 1, "nextBirdTimer", shot_input[15]);
    damage_function(lua, 1, "attachParticleSpawnersToShotBird", shot_particles);
    damage_function(lua, 1, "playSlingshotLaunchSound", shot_sling_sound);
    damage_lua.set_field(lua, -10002, "_ab_shot_self");
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, "setDraggedBird");
    damage_lua.get_field(lua, -10002, "_ab_shot_self");
    damage_lua.get_field(lua, -10002, "_ab_shot_bird");
    shot_call(lua, 2, 0);
    damage_lua.set_top(lua, 0);
    shot_selection[0] = shot_selection[1] = 1;
}

static void probe_bird_shot(void) {
    void *lua = open_sling_reference();
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "stopMovement", damage_require);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    damage_function(lua, -10002, "setPosition", shot_position);
    damage_function(lua, -10002, "setVelocity", shot_velocity);
    damage_function(lua, -10002, "applyImpulse", shot_apply_impulse);
    damage_function(lua, -10002, "getObjectDefinition", shot_definition);
    damage_function(lua, -10002, "hook", shot_hook);
    damage_function(lua, -10002, "setSprite", shot_sprite);
    damage_function(lua, -10002, "getAudioName", shot_resolve_audio);
    damage_function(lua, -10002, "playAudio", shot_play_audio);
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 1, "EID_BIRD_SHOT", UINT32_C(0x3f800000));
    damage_number(lua, 1, "EID_ALL_BIRDS_SHOT", UINT32_C(0x40000000));
    damage_lua.set_field(lua, -10002, "events");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "notify", shot_notify);
    damage_lua.set_field(lua, -10002, "eventManager");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "getBirdsShot", shot_count);
    damage_function(lua, 1, "getBackwardsBirdCount", shot_backwards);
    damage_lua.set_field(lua, -10002, "settingsWrapper");
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, "launchBirdWithImpulse");
    const uint32_t *closure = damage_lua.to_pointer(lua, -1);
    const uint8_t *header = (const uint8_t *)closure;
    if (!closure || header[6] || header[7] != 2) {
        fail("Original bird shot closure layout mismatch\n");
    }
    shot_dragged = (uint32_t *)(uintptr_t)((uint32_t *)(uintptr_t)closure[5])[2];
    shot_selection = (uint32_t *)(uintptr_t)((uint32_t *)(uintptr_t)closure[6])[2];
    damage_lua.set_top(lua, 0);
    uint32_t state = UINT32_C(0x520b1d51);
    semihost(4, "AB_ANDROID_BIRD_SHOT_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        for (size_t field = 0; field < 18; ++field) {
            shot_input[field] = random_float(&state, 122, 8, field < 6);
        }
        FloatBits count = {.number = (float)(index % 8)};
        shot_input[7] = count.bits;
        count.number = count.number + (index & 64 ? 1.0f : 2.0f);
        shot_input[8] = count.bits;
        shot_input[18] = (uint32_t)index & 1023;
        prepare_shot(lua);
        print_words(shot_input, 19);
        shot_event_count = 0;
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, 1, "launchBirdWithImpulse");
        damage_lua.get_field(lua, -10002, "_ab_shot_self");
        damage_lua.get_field(lua, -10002, "_ab_shot_position");
        damage_lua.get_field(lua, -10002, "_ab_shot_impulse");
        shot_call(lua, 3, 0);
        damage_lua.set_top(lua, 0);
        uint32_t output[19];
        shot_snapshot(lua, output);
        damage_lua.get_field(lua, -10002, "_ab_shot_position");
        output[15] = shot_numeric(lua, 1, "x");
        output[16] = shot_numeric(lua, 1, "y");
        damage_lua.get_field(lua, -10002, "_ab_shot_impulse");
        output[17] = shot_numeric(lua, 2, "x");
        output[18] = shot_numeric(lua, 2, "y");
        damage_lua.set_top(lua, 0);
        print_words(output, 19);
        uint32_t count_word = (uint32_t)shot_event_count;
        print_words(&count_word, 1);
        for (size_t event = 0; event < shot_event_count; ++event) {
            print_words(shot_events[event], 23);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird shot reference complete\n");
}

#endif
