#ifndef AB_ORACLE_AUDIO_GROUP_H
#define AB_ORACLE_AUDIO_GROUP_H

static const char *const audio_variant_names[] = {
    "variant0",  "variant1",  "variant2",  "variant3", "variant4",  "variant5",
    "variant6",  "variant7",  "variant8",  "variant9", "variant10", "variant11",
    "variant12", "variant13", "variant14", "variant15"};

static void audio_requested(void *lua, uint32_t code) {
    if (code == 0) {
        damage_lua.push_nil(lua);
    } else if (code == 1) {
        damage_lua.push_boolean(lua, 0);
    } else if (code == 2) {
        damage_lua.push_number(lua, 0);
    } else {
        damage_lua.push_string(lua, code == 3 ? "request" : "absent");
    }
}

static void audio_value(void *lua, size_t index, uint32_t pattern) {
    uint32_t code = ((uint32_t)index + pattern) % 5;
    if (code == 0) {
        damage_lua.push_boolean(lua, 0);
    } else if (code == 1) {
        damage_lua.push_number(lua, 0);
    } else {
        damage_lua.push_string(lua, audio_variant_names[index]);
    }
}

static void audio_selected(void *lua, uint32_t words[2]) {
    int tag = damage_lua.type(lua, -1);
    words[0] = (uint32_t)tag;
    words[1] = 0;
    if (tag == 0 || tag == 1) {
        if (tag == 1 && damage_lua.to_boolean(lua, -1)) {
            fail("Unexpected true audio-group result\n");
        }
        return;
    }
    if (tag == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        words[1] = value.bits;
        return;
    }
    if (tag == 4) {
        if (damage_named(lua, -1, "request")) {
            words[1] = 256;
            return;
        }
        if (damage_named(lua, -1, "absent")) {
            words[1] = 257;
            return;
        }
        for (uint32_t index = 0; index < 16; ++index) {
            if (damage_named(lua, -1, audio_variant_names[index])) {
                words[1] = index + 1;
                return;
            }
        }
    }
    fail("Unexpected typed audio-group result\n");
}

static void probe_audio_group(void) {
    unsigned short *(*seed48)(const unsigned short *) =
        (unsigned short *(*)(const unsigned short *))find_function("seed48");
    void (*raw_set_integer)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original audio-group state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    load_selected_function(lua, "/data/local/tmp/ab520/audio-name-oracle.lua",
                           "_ab_audio_name");
    uint32_t random = UINT32_C(0x520a0d10);
    semihost(4, "AB_ANDROID_AUDIO_GROUP_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t seed = random_word(&random);
        uint32_t input[] = {(seed << 16) | UINT32_C(0x330e),
                            seed >> 16,
                            (uint32_t)(item / 34) % 5,
                            (uint32_t)(item / 17) & 1,
                            (uint32_t)item % 17,
                            (uint32_t)item % 31};
        if (item >= 3072) {
            input[0] = UINT32_C(0xc0b22aa9);
            input[1] = UINT32_C(0xf22f);
            input[2] = 3;
            input[3] = 1;
        }
        unsigned short state[] = {(unsigned short)input[0],
                                  (unsigned short)(input[0] >> 16),
                                  (unsigned short)input[1]};
        seed48(state);
        damage_lua.create_table(lua, 0, 1);
        if (input[3]) {
            damage_lua.create_table(lua, (int)input[4], 0);
            for (uint32_t index = 0; index < input[4]; ++index) {
                audio_value(lua, index, input[5]);
                raw_set_integer(lua, 2, (int)index + 1);
            }
            damage_lua.set_field(lua, 1, "request");
        }
        damage_lua.set_field(lua, -10002, "audioGroups");
        damage_lua.get_field(lua, -10002, "_ab_audio_name");
        audio_requested(lua, input[2]);
        int status = damage_lua.call(lua, 1, 1, 0);
        if (status != 0 && status != 2) {
            fail("Unexpected original audio-group call status\n");
        }
        const unsigned short zero[3] = {0};
        const unsigned short *previous = seed48(zero);
        unsigned short after[] = {previous[0], previous[1], previous[2]};
        uint32_t output[] = {(uint32_t)status,
                             (uint32_t)after[0] | (uint32_t)after[1] << 16, after[2], 0,
                             0};
        if (!status) {
            audio_selected(lua, output + 3);
        }
        print_words(input, 6);
        print_words(output, 5);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB audio group reference complete\n");
}

#endif
