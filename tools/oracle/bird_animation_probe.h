#ifndef AB_ORACLE_BIRD_ANIMATION_H
#define AB_ORACLE_BIRD_ANIMATION_H

static uint32_t face_input[7], face_events[10][6], face_random_state;
static size_t face_event_count;

static uint32_t *face_event(void *lua, uint32_t kind) {
    if (face_event_count >= 10) {
        fail("Original bird animation event overflow\n");
    }
    uint32_t *event = face_events[face_event_count++];
    event[0] = kind;
    event[1] = event[2] = event[3] = 0;
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_face_bird");
    damage_lua.get_field(lua, -1, "animTimer");
    FloatBits timer = {.number = damage_lua.to_number(lua, -1)};
    event[4] = timer.bits;
    damage_lua.set_top(lua, top + 1);
    damage_lua.get_field(lua, -1, "animOn");
    event[5] = damage_lua.type(lua, -1) == 1 && damage_lua.to_boolean(lua, -1);
    damage_lua.set_top(lua, top);
    return event;
}

static void face_mutate(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_face_bird");
    damage_number(lua, top + 1, "animTimer", face_input[6]);
    damage_lua.push_boolean(lua, !(face_input[2] & 1));
    damage_lua.set_field(lua, top + 1, "animOn");
    damage_lua.set_top(lua, top);
}

static int face_random(void *lua) {
    int low = (int)damage_lua.to_number(lua, 1);
    int high = (int)damage_lua.to_number(lua, 2);
    if (damage_lua.get_top(lua) != 2 || low < 1 || high < low || high > 150) {
        fail("Unexpected bird animation random interval\n");
    }
    uint32_t value =
        (uint32_t)low + random_word(&face_random_state) % ((uint32_t)(high - low) + 1);
    if ((face_input[2] & 128) && !face_event_count) {
        value = (uint32_t)high + 1;
    }
    uint32_t *event = face_event(lua, 1);
    event[1] = (uint32_t)low;
    event[2] = (uint32_t)high;
    event[3] = value;
    damage_lua.push_number(lua, (float)value);
    return 1;
}

static int face_distance(void *lua) {
    face_event(lua, 2)[1] = face_input[3];
    if (face_input[5] & 2) {
        damage_lua.push_boolean(lua, !(face_input[2] & 16));
        damage_lua.set_field(lua, -10002, "eagleBaitLaunched");
        damage_lua.get_field(lua, -10002, "blockTable");
        damage_lua.get_field(lua, -1, "blocks");
        damage_lua.get_field(lua, -1, "base");
        if (face_input[2] & 4) {
            damage_lua.push_nil(lua);
        } else {
            damage_lua.push_string(lua, "yell");
        }
        damage_lua.set_field(lua, -2, "spriteYell");
    }
    FloatBits value = {.bits = face_input[3]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

static int face_audio_name(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "birds")) {
        fail("Unexpected bird animation audio group\n");
    }
    face_event(lua, 3);
    damage_lua.push_string(lua, "audio");
    return 1;
}

static int face_audio(void *lua) {
    if (damage_lua.get_top(lua) != 4 || !damage_named(lua, 1, "audio")) {
        fail("Unexpected bird animation audio boundary\n");
    }
    uint32_t *event = face_event(lua, 4);
    FloatBits volume = {.number = damage_lua.to_number(lua, 2)};
    FloatBits time = {.number = damage_lua.to_number(lua, 4)};
    event[1] = volume.bits;
    event[2] = (uint32_t)damage_lua.to_boolean(lua, 3);
    event[3] = time.bits;
    if (face_input[5] & 4) {
        face_mutate(lua);
    }
    return 0;
}

static int face_sprite(void *lua) {
    if (damage_lua.get_top(lua) != 2 || !damage_named(lua, 1, "first")) {
        fail("Unexpected bird animation sprite boundary\n");
    }
    uint32_t *event = face_event(lua, 5);
    event[1] = damage_named(lua, 2, "base")    ? 1u
               : damage_named(lua, 2, "yell")  ? 2u
               : damage_named(lua, 2, "blink") ? 3u
                                               : 0;
    if (!event[1]) {
        fail("Unexpected bird animation sprite identifier\n");
    }
    if (face_input[5] & 1) {
        face_mutate(lua);
    }
    return 0;
}

static int face_zero(void *lua) {
    damage_lua.push_number(lua, 0);
    return 1;
}

static int face_transform(void *lua) {
    damage_lua.push_number(lua, 0);
    damage_lua.push_number(lua, 0);
    return 2;
}

static int face_start(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, top + 1, "x", 0);
    damage_number(lua, top + 1, "y", 0);
    return 1;
}

static void face_bind_sling(void *lua) {
    damage_lua.get_field(lua, -10002, "_ab_face_update");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "getLevelStartPosition", face_start);
    damage_lua.set_field(lua, -10002, "_ab_face_sling");
    damage_lua.get_field(lua, -10002, "_ab_face_sling");
    const uint8_t *closure = damage_lua.to_pointer(lua, 1);
    const void *sling = damage_lua.to_pointer(lua, 2);
    if (!closure || !sling || closure[6] || closure[7] != 1) {
        fail("Unexpected selected bird animation closure\n");
    }
    const uint32_t *upvalue =
        (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[5];
    if (!upvalue || upvalue[2] != (uint32_t)(uintptr_t)(upvalue + 3)) {
        fail("Expected fresh closed bird animation upvalue\n");
    }
    uint32_t *value = (uint32_t *)(uintptr_t)upvalue[2];
    if (value[1]) {
        fail("Expected initially nil bird animation upvalue\n");
    }
    value[0] = (uint32_t)(uintptr_t)sling;
    value[1] = 5;
    damage_lua.set_top(lua, 0);
}

static void face_prepare(void *lua) {
    uint32_t flags = face_input[2];
    damage_lua.create_table(lua, 0, 8);
    if (flags & 256) {
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 1, "animTimer");
    } else {
        damage_number(lua, 1, "animTimer", face_input[0]);
    }
    if (!(flags & 1) && (flags & 32)) {
        damage_lua.push_number(lua, 1);
    } else {
        damage_lua.push_boolean(lua, flags & 1);
    }
    damage_lua.set_field(lua, 1, "animOn");
    damage_lua.push_boolean(lua, flags & 64);
    damage_lua.set_field(lua, 1, "shot");
    damage_lua.push_boolean(lua, !(flags & 2));
    damage_lua.set_field(lua, 1, "overrideDrawing");
    const char *keys[] = {"name", "definition", "damageSprite"};
    const char *values[] = {"first", "base", "base"};
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.push_string(lua, values[index]);
        damage_lua.set_field(lua, 1, keys[index]);
    }
    damage_lua.set_field(lua, -10002, "_ab_face_bird");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "_ab_face_bird");
    damage_lua.set_field(lua, 1, "first");
    damage_lua.set_field(lua, -10002, "birds");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 2);
    if (flags & 4) {
        damage_lua.push_string(lua, "yell");
        damage_lua.set_field(lua, 3, "spriteYell");
    }
    if (flags & 8) {
        damage_lua.push_string(lua, "blink");
        damage_lua.set_field(lua, 3, "spriteBlink");
    }
    damage_lua.set_field(lua, 2, "base");
    damage_lua.set_field(lua, 1, "blocks");
    damage_lua.set_field(lua, -10002, "blockTable");
    damage_lua.push_boolean(lua, flags & 16);
    damage_lua.set_field(lua, -10002, "eagleBaitLaunched");
}

static void probe_bird_animation(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original bird animation state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/jump/bird-jump-oracle.lua",
                           "_ab_face_update");
    face_bind_sling(lua);
    damage_lua.get_field(lua, -10002, "math");
    damage_function(lua, 1, "random", face_random);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "vLength", face_distance);
    damage_function(lua, -10002, "physicsToWorldTransform", face_transform);
    damage_function(lua, -10002, "getAudioName", face_audio_name);
    damage_function(lua, -10002, "playAudio", face_audio);
    damage_function(lua, -10002, "setSprite", face_sprite);
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "getScreenX", face_zero);
    damage_function(lua, 1, "getScreenY", face_zero);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    const char *empty[] = {"levelGoals", "g_birdHats"};
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, -10002, empty[index]);
    }
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, -10002, "currentBirdName");
    damage_lua.push_string(lua, "birds");
    damage_lua.set_field(lua, -10002, "g_sound_birds");
    uint32_t random = UINT32_C(0x520face1);
    semihost(4, "AB_ANDROID_BIRD_ANIMATION_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        FloatBits timer = {.number = (float)((int)(item % 13) - 6) / 100.0f};
        FloatBits seconds = {.number = (float)(item % 17) / 100.0f};
        FloatBits distance = {.number = (float)((int)(item % 11) - 2) * 250.0f};
        if (item < 32) {
            timer.bits = (uint32_t)(item % 4) | (item & 16 ? 0x80000000u : 0);
            seconds.bits = 0;
            distance.bits = UINT32_C(0x447a0000) + (uint32_t)(item % 3) - 1;
        }
        uint32_t flags = (uint32_t)item & 511;
        face_input[0] = flags & 256 ? 0 : timer.bits;
        face_input[1] = seconds.bits;
        face_input[2] = flags;
        face_input[3] = distance.bits;
        face_input[4] = random_word(&random);
        face_input[5] = (uint32_t)(item / 512) & 7;
        face_input[6] = random_float(&random, 123, 4, 1);
        face_prepare(lua);
        face_event_count = 0;
        face_random_state = face_input[4];
        damage_lua.get_field(lua, -10002, "_ab_face_update");
        damage_lua.push_number(lua, seconds.number);
        if (damage_lua.call(lua, 1, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original bird animation call failed\n");
        }
        print_words(face_input, 7);
        damage_lua.get_field(lua, -10002, "_ab_face_bird");
        damage_lua.get_field(lua, -1, "animTimer");
        FloatBits output_timer = {.number = damage_lua.to_number(lua, -1)};
        damage_lua.set_top(lua, 1);
        damage_lua.get_field(lua, -1, "animOn");
        uint32_t output[] = {output_timer.bits,
                             damage_lua.type(lua, -1) == 1 &&
                                 damage_lua.to_boolean(lua, -1),
                             face_random_state, (uint32_t)face_event_count};
        print_words(output, 4);
        for (size_t event = 0; event < face_event_count; ++event) {
            print_words(face_events[event], 6);
        }
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird animation reference complete\n");
}

#endif
