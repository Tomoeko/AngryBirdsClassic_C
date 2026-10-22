#ifndef AB_ORACLE_AUDIO_PLAY_H
#define AB_ORACLE_AUDIO_PLAY_H

static void push_play_value(void *lua, uint32_t code) {
    static const float numbers[] = {0.0f, 0.5f, 1.0f, 2.0f};
    if (!code) {
        damage_lua.push_nil(lua);
    } else if (code < 3) {
        damage_lua.push_boolean(lua, code == 2);
    } else if (code < 7) {
        damage_lua.push_number(lua, numbers[code - 3]);
    } else if (code < 9) {
        damage_lua.push_string(lua, code == 7 ? "" : "value");
    } else {
        fail("Unexpected controlled audio-play value\n");
    }
}

static uint32_t play_value_code(void *lua, int index) {
    static const float numbers[] = {0.0f, 0.5f, 1.0f, 2.0f};
    int type = damage_lua.type(lua, index);
    if (type == 0) {
        return 0;
    }
    if (type == 1) {
        return damage_lua.to_boolean(lua, index) ? 2u : 1u;
    }
    if (type == 3) {
        float number = damage_lua.to_number(lua, index);
        for (uint32_t item = 0; item < 4; ++item) {
            if (number == numbers[item]) {
                return item + 3;
            }
        }
    }
    if (type == 4 && damage_named(lua, index, "")) {
        return 7;
    }
    if (type == 4 && damage_named(lua, index, "value")) {
        return 8;
    }
    fail("Unexpected original audio-play value\n");
}

static void push_play_name(void *lua, uint32_t code) {
    if (code < 2) {
        push_play_value(lua, code);
    } else if (code == 2) {
        damage_lua.push_number(lua, 0);
    } else {
        damage_lua.push_string(lua, code == 3 ? "" : code == 4 ? "clip" : "missing");
    }
}

static uint32_t play_name_code(void *lua, int index) {
    int type = damage_lua.type(lua, index);
    if (type == 4 && damage_named(lua, index, "clip")) {
        return 4;
    }
    if (type == 4 && damage_named(lua, index, "missing")) {
        return 5;
    }
    if (type == 1 && !damage_lua.to_boolean(lua, index)) {
        return 1;
    }
    if (type == 3 && damage_lua.to_number(lua, index) == 0) {
        return 2;
    }
    fail("Unexpected original audio-play name\n");
}

static uint32_t audio_play_output[6];

static int capture_audio_play(void *lua) {
    if (damage_lua.get_top(lua) != 4 || ++audio_play_output[1] != 1) {
        fail("Unexpected original playback resource call\n");
    }
    audio_play_output[2] = play_name_code(lua, 1);
    for (int argument = 2; argument <= 4; ++argument) {
        audio_play_output[(size_t)argument + 1] = play_value_code(lua, argument);
    }
    return 0;
}

static void probe_audio_play(void) {
    static const char *const fields[] = {"channel", "volume", "loop"};
    initialize_damage_lua();
    void (*set_table)(void *, int) =
        (void (*)(void *, int))find_function("_Z12lua_settableP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original audio-play state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/audio-play-oracle.lua",
                           "_ab_play_audio");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "playAudio", capture_audio_play);
    damage_lua.set_field(lua, -10002, "res");
    uint32_t random = UINT32_C(0x52051a77);
    semihost(4, "AB_ANDROID_AUDIO_PLAY_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[10] = {(uint32_t)item % 6, (uint32_t)(item / 6) % 4};
        for (size_t field = 2; field < 10; ++field) {
            input[field] = random_word(&random) % 9;
        }
        damage_lua.create_table(lua, 0, 2);
        damage_lua.create_table(lua, 0, 1);
        if (input[1] == 2) {
            damage_lua.create_table(lua, 0, 3);
            for (size_t field = 0; field < 3; ++field) {
                push_play_value(lua, input[field + 6]);
                damage_lua.set_field(lua, 3, fields[field]);
            }
        } else {
            push_play_value(lua, input[1] == 3 ? 2u : input[1]);
        }
        damage_lua.set_field(lua, 2, "clip");
        damage_lua.set_field(lua, 1, "sounds");
        damage_lua.create_table(lua, 0, 8);
        for (uint32_t key = 1; key < 9; ++key) {
            push_play_value(lua, key);
            push_play_value(lua, input[9]);
            set_table(lua, 2);
        }
        damage_lua.set_field(lua, 1, "channelVolume");
        damage_lua.set_field(lua, -10002, "audioData");
        for (size_t word = 0; word < 6; ++word) {
            audio_play_output[word] = 0;
        }
        damage_lua.get_field(lua, -10002, "_ab_play_audio");
        push_play_name(lua, input[0]);
        for (size_t argument = 2; argument < 6; ++argument) {
            push_play_value(lua, input[argument]);
        }
        audio_play_output[0] = damage_lua.call(lua, 5, 0, 0) != 0;
        damage_lua.set_top(lua, 0);
        print_words(input, 10);
        print_words(audio_play_output, 6);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB audio play reference complete\n");
}

#endif
