#ifndef AB_ORACLE_AUDIO_DEFINITION_H
#define AB_ORACLE_AUDIO_DEFINITION_H

static void push_audio_value(void *lua, uint32_t code) {
    if (code == 0) {
        damage_lua.push_nil(lua);
    } else if (code <= 2) {
        damage_lua.push_boolean(lua, code == 2);
    } else if (code <= 4) {
        damage_lua.push_number(lua, code == 3 ? 0.0f : 0.5f);
    } else if (code <= 6) {
        damage_lua.push_string(lua, code == 5 ? "" : "value");
    } else {
        fail("Unexpected controlled audio value\n");
    }
}

static uint32_t audio_value_code(void *lua, int index) {
    int type = damage_lua.type(lua, index);
    if (type == 0) {
        return 0;
    }
    if (type == 1) {
        return damage_lua.to_boolean(lua, index) ? 2u : 1u;
    }
    if (type == 3) {
        float value = damage_lua.to_number(lua, index);
        if (value == 0 || value == 0.5f) {
            return value == 0 ? 3u : 4u;
        }
    }
    if (type == 4) {
        if (damage_named(lua, index, "")) {
            return 5;
        }
        if (damage_named(lua, index, "value")) {
            return 6;
        }
    }
    fail("Unexpected original audio metadata value\n");
}

static uint32_t audio_creation_words[6];
static char audio_creation_path[128];
static unsigned audio_creation_path_size;

static int capture_audio_creation(void *lua) {
    if (damage_lua.get_top(lua) != 3 || damage_lua.type(lua, 1) != 4 ||
        !damage_named(lua, 2, "clip") || ++audio_creation_words[0] != 1) {
        fail("Unexpected original audio resource request\n");
    }
    const char *path = damage_lua.to_string(lua, 1, &audio_creation_path_size);
    if (!path || audio_creation_path_size >= sizeof(audio_creation_path)) {
        fail("Unexpected original audio path size\n");
    }
    for (unsigned index = 0; index < audio_creation_path_size; ++index) {
        audio_creation_path[index] = path[index];
    }
    audio_creation_path[audio_creation_path_size] = 0;
    audio_creation_words[1] = audio_value_code(lua, 3);
    return 0;
}

static void probe_audio_definition(void) {
    static const char *const fields[] = {"channel", "volume", "canBeOverWritten",
                                         "loop"};
    static const char *const extensions[] = {"wav", "mp3", "", "custom"};
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original audio definition state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/audio-definition-oracle.lua",
                           "_ab_create_audio");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "createAudio", capture_audio_creation);
    damage_lua.set_field(lua, -10002, "res");
    damage_lua.push_string(lua, "data/audio");
    damage_lua.set_field(lua, -10002, "audioPath");
    uint32_t random = UINT32_C(0x520cab11);
    semihost(4, "AB_ANDROID_AUDIO_DEFINITION_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[8] = {(uint32_t)item % 2, (uint32_t)(item / 2) % 6,
                             (uint32_t)(item / 12) % 7, (uint32_t)(item / 84) % 4};
        for (size_t field = 0; field < 4; ++field) {
            input[field + 4] = random_word(&random) % 7;
        }
        damage_lua.push_boolean(lua, (int)input[0]);
        damage_lua.set_field(lua, -10002, "g_useMp3Sound");
        damage_lua.create_table(lua, 0, 1);
        damage_lua.create_table(lua, 0, 1);
        push_audio_value(lua, input[3]);
        damage_lua.set_field(lua, 2, "clip");
        damage_lua.set_field(lua, 1, "sounds");
        damage_lua.set_field(lua, -10002, "audioData");
        damage_lua.create_table(lua, 0, 8);
        damage_lua.push_string(lua, "/sfx/source");
        damage_lua.set_field(lua, 1, "fileName");
        damage_lua.push_string(lua, "clip");
        damage_lua.set_field(lua, 1, "clipName");
        if (input[1] < 2) {
            push_audio_value(lua, input[1]);
        } else {
            damage_lua.push_string(lua, extensions[input[1] - 2]);
        }
        damage_lua.set_field(lua, 1, "extension");
        push_audio_value(lua, input[2]);
        damage_lua.set_field(lua, 1, "stream");
        for (size_t field = 0; field < 4; ++field) {
            push_audio_value(lua, input[field + 4]);
            damage_lua.set_field(lua, 1, fields[field]);
        }
        damage_lua.set_field(lua, -10002, "_ab_audio_properties");
        for (size_t word = 0; word < 6; ++word) {
            audio_creation_words[word] = 0;
        }
        damage_lua.get_field(lua, -10002, "_ab_create_audio");
        damage_lua.get_field(lua, -10002, "_ab_audio_properties");
        if (damage_lua.call(lua, 1, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original audio definition failed\n");
        }
        if (audio_creation_words[0]) {
            damage_lua.get_field(lua, -10002, "audioData");
            damage_lua.get_field(lua, 1, "sounds");
            damage_lua.get_field(lua, 2, "clip");
            damage_lua.get_field(lua, 3, "name");
            if (!damage_named(lua, 4, "clip")) {
                fail("Unexpected stored original audio name\n");
            }
            damage_lua.set_top(lua, 3);
            for (size_t field = 0; field < 4; ++field) {
                damage_lua.get_field(lua, 3, fields[field]);
                audio_creation_words[field + 2] = audio_value_code(lua, 4);
                damage_lua.set_top(lua, 3);
            }
        }
        damage_lua.set_top(lua, 0);
        print_words(input, 8);
        print_words(audio_creation_words, 6);
        if (audio_creation_words[0]) {
            print_words(&audio_creation_path_size, 1);
            semihost(4, audio_creation_path);
            semihost(4, "\n");
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB audio definition reference complete\n");
}

#endif
