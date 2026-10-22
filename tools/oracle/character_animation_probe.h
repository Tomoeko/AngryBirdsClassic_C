#ifndef AB_ORACLE_CHARACTER_ANIMATION_H
#define AB_ORACLE_CHARACTER_ANIMATION_H

static const char *const character_names[] = {"goalA", "goalB", "birdA", "birdB",
                                              "birdC"};
static const char *const character_fields[] = {"strength",
                                               "specialExpressionTimer",
                                               "blinkTimer",
                                               "oinkTimer",
                                               "specialExpression",
                                               "blinkOn",
                                               "animTimer",
                                               "animOn",
                                               "x",
                                               "y",
                                               "angle",
                                               "jumpTimer",
                                               "jumpHeight",
                                               "jumpSpeed",
                                               "jumpAngle",
                                               "jumpRoll",
                                               "oldY",
                                               "oldAngle",
                                               "jumpOn",
                                               "jumpRebound"};
static uint32_t character_controls[11], character_events[64][4];
static size_t character_event_count;
static int (*character_original_random)(void *);

static uint32_t character_id(void *lua, int argument) {
    for (uint32_t index = 0; index < 5; ++index) {
        if (damage_named(lua, argument, character_names[index])) {
            return index;
        }
    }
    fail("Unexpected character animation object name\n");
}

static uint32_t *character_event(uint32_t kind) {
    if (character_event_count >= 64) {
        fail("Character animation event budget exceeded\n");
    }
    uint32_t *event = character_events[character_event_count++];
    event[0] = kind;
    event[1] = event[2] = event[3] = 0;
    return event;
}

static int character_random(void *lua) {
    FloatBits minimum = {.number = damage_lua.to_number(lua, 1)};
    FloatBits maximum = {.number = damage_lua.to_number(lua, 2)};
    if (damage_lua.get_top(lua) != 2 || character_original_random(lua) != 1) {
        fail("Unexpected original character random boundary\n");
    }
    FloatBits result = {.number = damage_lua.to_number(lua, -1)};
    uint32_t *event = character_event(1);
    event[1] = minimum.bits;
    event[2] = maximum.bits;
    event[3] = result.bits;
    return 1;
}

static int character_lost(void *lua) {
    character_event(2);
    damage_lua.push_boolean(lua, character_controls[10] != 0);
    return 1;
}

static int character_sprite(void *lua) {
    uint32_t *event = character_event(3);
    event[1] = character_id(lua, 1);
    const char *faces[] = {"special", "smile", "damage", "blink", "base", "yell"};
    for (uint32_t index = 0; index < 6; ++index) {
        if (damage_named(lua, 2, faces[index])) {
            event[2] = index + 1;
            return 0;
        }
    }
    fail("Unexpected character face identifier\n");
}

static int character_distance(void *lua) {
    character_event(4);
    FloatBits result = {.bits = character_controls[9]};
    damage_lua.push_number(lua, result.number);
    return 1;
}

static int character_audio_name(void *lua) {
    uint32_t *event = character_event(5);
    event[1] = damage_named(lua, 1, "pigs")    ? 1u
               : damage_named(lua, 1, "birds") ? 2u
                                               : 0;
    if (!event[1]) {
        fail("Unexpected character sound group\n");
    }
    damage_lua.push_string(lua, "audio");
    return 1;
}

static int character_audio(void *lua) {
    if (damage_lua.get_top(lua) != 4 || !damage_named(lua, 1, "audio")) {
        fail("Unexpected character audio boundary\n");
    }
    uint32_t *event = character_event(6);
    FloatBits volume = {.number = damage_lua.to_number(lua, 2)};
    FloatBits delay = {.number = damage_lua.to_number(lua, 4)};
    event[1] = volume.bits;
    event[2] = (uint32_t)damage_lua.to_boolean(lua, 3);
    event[3] = delay.bits;
    return 0;
}

static int character_position(void *lua) {
    uint32_t index = character_id(lua, 1);
    FloatBits x = {.number = damage_lua.to_number(lua, 2)};
    FloatBits y = {.number = damage_lua.to_number(lua, 3)};
    uint32_t *event = character_event(7);
    event[1] = index;
    event[2] = x.bits;
    event[3] = y.bits;
    damage_lua.get_field(lua, -10002, "_ab_characters");
    damage_lua.get_field(lua, -1, character_names[index]);
    damage_number(lua, -2, "x", x.bits);
    damage_number(lua, -2, "y", y.bits);
    return 0;
}

static int character_rotation(void *lua) {
    uint32_t index = character_id(lua, 1);
    FloatBits angle = {.number = damage_lua.to_number(lua, 2)};
    uint32_t *event = character_event(8);
    event[1] = index;
    event[2] = angle.bits;
    damage_lua.get_field(lua, -10002, "_ab_characters");
    damage_lua.get_field(lua, -1, character_names[index]);
    damage_number(lua, -2, "angle", angle.bits);
    return 0;
}

static int character_transform(void *lua) {
    damage_lua.push_number(lua, 0);
    damage_lua.push_number(lua, 0);
    return 2;
}

static int character_zero(void *lua) {
    damage_lua.push_number(lua, 0);
    return 1;
}

static int character_start(void *lua) {
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, -2, "x", 0);
    damage_number(lua, -2, "y", 0);
    return 1;
}

static void character_bind_sling(void *lua) {
    damage_lua.get_field(lua, -10002, "_ab_character_update");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "getLevelStartPosition", character_start);
    const uint8_t *closure = damage_lua.to_pointer(lua, 1);
    const void *sling = damage_lua.to_pointer(lua, 2);
    if (!closure || !sling || closure[6] || closure[7] != 1) {
        fail("Unexpected character animation closure\n");
    }
    const uint32_t *upvalue =
        (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[5];
    if (!upvalue || upvalue[2] != (uint32_t)(uintptr_t)(upvalue + 3)) {
        fail("Expected closed character animation upvalue\n");
    }
    uint32_t *value = (uint32_t *)(uintptr_t)upvalue[2];
    if (value[1]) {
        fail("Expected initially nil character animation upvalue\n");
    }
    value[0] = (uint32_t)(uintptr_t)sling;
    value[1] = 5;
    damage_lua.set_top(lua, 0);
}

static void character_prepare(void *lua, size_t item) {
    damage_lua.create_table(lua, 0, 0);
    for (size_t index = 0; index < 5; ++index) {
        damage_lua.create_table(lua, 0, 24);
        for (size_t field = 0; field < 20; ++field) {
            damage_number(lua, 2, character_fields[field], 0);
        }
        damage_number(lua, 2, "strength", UINT32_C(0x41200000));
        FloatBits timer = {.number = (float)((int)((item + index) % 7) - 3) / 10.0f};
        const char *timers[] = {"specialExpressionTimer", "blinkTimer", "oinkTimer",
                                "animTimer", "jumpTimer"};
        for (size_t field = 0; field < 5; ++field) {
            damage_number(lua, 2, timers[field], timer.bits);
        }
        FloatBits x = {.number = (float)index * -2.0f};
        FloatBits y = {.number = (float)index + 1.0f};
        damage_number(lua, 2, "x", x.bits);
        damage_number(lua, 2, "y", y.bits);
        if ((item + index) & 1) {
            damage_lua.push_string(lua, "special");
        } else {
            damage_lua.push_nil(lua);
        }
        damage_lua.set_field(lua, 2, "specialExpression");
        const char *flags[] = {"blinkOn", "animOn", "jumpOn", "jumpRebound"};
        for (size_t field = 0; field < 4; ++field) {
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, 2, flags[field]);
        }
        const char *fields[] = {"definition", "smileSprite", "damageSprite",
                                "blinkSprite", "name"};
        const char *values[] = {"base", "smile", index < 2 ? "damage" : "base", "blink",
                                character_names[index]};
        for (size_t field = 0; field < 5; ++field) {
            damage_lua.push_string(lua, values[field]);
            damage_lua.set_field(lua, 2, fields[field]);
        }
        damage_lua.set_field(lua, 1, character_names[index]);
    }
    damage_lua.set_field(lua, -10002, "_ab_characters");
    const char *groups[] = {"levelGoals", "birds"};
    for (size_t group = 0; group < 2; ++group) {
        damage_lua.create_table(lua, 0, 0);
        damage_lua.get_field(lua, -10002, "_ab_characters");
        size_t begin = group ? 2 : 0, end = group ? 5 : 2;
        for (size_t index = begin; index < end; ++index) {
            damage_lua.get_field(lua, 2, character_names[index]);
            damage_lua.set_field(lua, 1, character_names[index]);
        }
        damage_lua.set_top(lua, 1);
        damage_lua.set_field(lua, -10002, groups[group]);
    }
}

static void character_states(void *lua) {
    damage_lua.get_field(lua, -10002, "_ab_characters");
    for (size_t index = 0; index < 5; ++index) {
        damage_lua.get_field(lua, 1, character_names[index]);
        uint32_t words[20];
        for (size_t field = 0; field < 20; ++field) {
            damage_lua.get_field(lua, 2, character_fields[field]);
            if (field == 4 || field == 5 || field == 7 || field >= 18) {
                words[field] = (uint32_t)damage_lua.to_boolean(lua, -1);
            } else {
                FloatBits value = {.number = damage_lua.to_number(lua, -1)};
                words[field] = value.bits;
            }
            damage_lua.set_top(lua, 2);
        }
        print_words(words, 20);
        damage_lua.set_top(lua, 1);
    }
    damage_lua.set_top(lua, 0);
}

static void character_frame_controls(void *lua) {
    damage_lua.push_string(lua, character_names[character_controls[1]]);
    damage_lua.set_field(lua, -10002, "currentBirdName");
    damage_lua.push_boolean(lua, (character_controls[2] & 1) != 0);
    damage_lua.set_field(lua, -10002, "eagleBaitLaunched");
    damage_lua.push_boolean(lua, (character_controls[2] & 2) != 0);
    damage_lua.set_field(lua, -10002, "levelCompleted");
    damage_lua.get_field(lua, -10002, "_ab_characters");
    const char *flags[] = {"overrideDrawing", "frozenSolid", "shot", "disableSmiling"};
    for (size_t index = 0; index < 5; ++index) {
        damage_lua.get_field(lua, 1, character_names[index]);
        for (size_t flag = 0; flag < 4; ++flag) {
            damage_lua.push_boolean(lua, (character_controls[3 + flag] >> index) & 1);
            damage_lua.set_field(lua, 2, flags[flag]);
        }
        damage_lua.set_top(lua, 1);
    }
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "blockTable");
    damage_lua.get_field(lua, 1, "blocks");
    damage_lua.get_field(lua, 2, "base");
    damage_lua.push_boolean(lua, character_controls[8] != 0);
    damage_lua.set_field(lua, 3, "allowRoll");
    damage_lua.set_top(lua, 0);
}

static void probe_character_animation(void) {
    typedef void (*Seed)(long);
    Seed seed = (Seed)find_function("srand48");
    unsigned short *(*seed48)(const unsigned short *) =
        (unsigned short *(*)(const unsigned short *))find_function("seed48");
    int (*next)(void *, int) =
        (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original character animation state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.open_math(lua);
    damage_lua.get_field(lua, -1, "random");
    const uint32_t *closure = damage_lua.to_pointer(lua, -1);
    if (!closure || ((closure[1] >> 16) & 255) != 1) {
        fail("Expected original math.random C closure\n");
    }
    character_original_random = (int (*)(void *))(uintptr_t)closure[4];
    damage_lua.set_top(lua, 1);
    damage_function(lua, 1, "random", character_random);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/jump/bird-jump-oracle.lua",
                           "_ab_character_update");
    character_bind_sling(lua);
    damage_function(lua, -10002, "g_game_lose_condition", character_lost);
    damage_function(lua, -10002, "setSprite", character_sprite);
    damage_function(lua, -10002, "physicsToWorldTransform", character_transform);
    damage_function(lua, -10002, "vLength", character_distance);
    damage_function(lua, -10002, "getAudioName", character_audio_name);
    damage_function(lua, -10002, "playAudio", character_audio);
    damage_function(lua, -10002, "setPosition", character_position);
    damage_function(lua, -10002, "setRotation", character_rotation);
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "getScreenX", character_zero);
    damage_function(lua, 1, "getScreenY", character_zero);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "g_birdHats");
    const char *sounds[] = {"g_sound_pigs", "g_sound_birds"};
    const char *groups[] = {"pigs", "birds"};
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.push_string(lua, groups[index]);
        damage_lua.set_field(lua, -10002, sounds[index]);
    }
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 5);
    damage_number(lua, 3, "strength", UINT32_C(0x41200000));
    damage_lua.push_string(lua, "yell");
    damage_lua.set_field(lua, 3, "spriteYell");
    damage_lua.push_string(lua, "blink");
    damage_lua.set_field(lua, 3, "spriteBlink");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_number(lua, 5, "min", UINT32_C(0x42480000));
    damage_lua.set_field(lua, 4, "damage1");
    damage_lua.set_field(lua, 3, "damageSprites");
    damage_lua.set_field(lua, 2, "base");
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.set_field(lua, -10002, "blockTable");
    semihost(4, "AB_ANDROID_CHARACTER_ANIMATION_V1 64 32 5\n");
    uint32_t random = UINT32_C(0x520c4a11);
    for (size_t item = 0; item < 64; ++item) {
        uint32_t initial_seed = random_word(&random);
        seed((long)initial_seed);
        print_words(&initial_seed, 1);
        character_prepare(lua, item);
        uint32_t order[5];
        size_t used = 0;
        const char *tables[] = {"levelGoals", "birds"};
        for (size_t table = 0; table < 2; ++table) {
            damage_lua.get_field(lua, -10002, tables[table]);
            damage_lua.push_nil(lua);
            while (next(lua, 1)) {
                if (used >= 5) {
                    fail("Unexpected character membership\n");
                }
                order[used++] = character_id(lua, -2);
                damage_lua.set_top(lua, 2);
            }
            if (used != (table ? 5u : 2u)) {
                fail("Incomplete character membership\n");
            }
            damage_lua.set_top(lua, 0);
        }
        print_words(order, 5);
        character_states(lua);
        for (size_t frame = 0; frame < 32; ++frame) {
            FloatBits seconds = {.number = frame % 7 == 0 ? 0 : 0.1f};
            FloatBits distance = {.number = (float)((item + frame) % 7) * 250.0f};
            uint32_t controls[] = {seconds.bits,
                                   (uint32_t)(2 + (item + frame / 8) % 3),
                                   (uint32_t)(item & 1) |
                                       (frame >= 16 && (item & 2) ? 2u : 0u),
                                   item & 4 ? 1u << (frame / 8) : 0,
                                   item & 8 ? 1u : 0,
                                   item & 16 ? 8u : 0,
                                   (uint32_t)(item & 3),
                                   31,
                                   (uint32_t)(item & 32),
                                   distance.bits,
                                   (uint32_t)(item % 5 == 0)};
            for (size_t index = 0; index < 11; ++index) {
                character_controls[index] = controls[index];
            }
            character_frame_controls(lua);
            character_event_count = 0;
            damage_lua.get_field(lua, -10002, "_ab_character_update");
            damage_lua.push_number(lua, seconds.number);
            if (damage_lua.call(lua, 1, 0, 0)) {
                const char *message = damage_lua.to_string(lua, -1, NULL);
                fail(message ? message : "Original character animation call failed\n");
            }
            print_words(character_controls, 11);
            const unsigned short zero[3] = {0};
            const unsigned short *previous = seed48(zero);
            unsigned short state[3] = {previous[0], previous[1], previous[2]};
            uint32_t output[] = {(uint32_t)state[0] | (uint32_t)state[1] << 16,
                                 state[2], (uint32_t)character_event_count};
            seed48(state);
            print_words(output, 3);
            character_states(lua);
            for (size_t event = 0; event < character_event_count; ++event) {
                print_words(character_events[event], 4);
            }
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB character animation reference complete\n");
}

#endif
