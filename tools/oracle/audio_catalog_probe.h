#ifndef AB_ORACLE_AUDIO_CATALOG_H
#define AB_ORACLE_AUDIO_CATALOG_H

static void catalog_label(void *lua, int index) {
    unsigned size = 0;
    const char *text = damage_lua.to_string(lua, index, &size);
    if (damage_lua.type(lua, index) != 4 || !text || !size || size > 255) {
        fail("Unexpected original audio catalog label\n");
    }
    for (unsigned cursor = 0; cursor < size; ++cursor) {
        if ((unsigned char)text[cursor] < 32 || (unsigned char)text[cursor] > 126) {
            fail("Unexpected original audio catalog label encoding\n");
        }
    }
    uint32_t word = size;
    print_words(&word, 1);
    semihost(4, text);
    semihost(4, "\n");
}

#if defined(AB_ORACLE_AUDIO_CATALOG_ONLY)
static void probe_audio_catalog(void) {
    initialize_damage_lua();
    int (*next)(void *, int) =
        (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    unsigned (*length)(void *, int) =
        (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    void (*raw_get_integer)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate native audio catalog state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.push_string(lua, "android");
    damage_lua.set_field(lua, -10002, "deviceModel");
    load_damage_chunk(lua, "/data/local/tmp/ab520/audio-catalog.lua");
    damage_lua.get_field(lua, -10002, "audioGroups");
    if (damage_lua.type(lua, 1) != 5) {
        fail("Expected original audioGroups table\n");
    }
    semihost(4, "AB_ANDROID_AUDIO_CATALOG_V1 46 196\n");
    size_t groups = 0, variants = 0;
    damage_lua.push_nil(lua);
    while (next(lua, 1)) {
        if (++groups > 46 || damage_lua.type(lua, 3) != 5) {
            fail("Unexpected original audio-group catalog entry\n");
        }
        catalog_label(lua, 2);
        uint32_t count = length(lua, 3);
        if (!count || count > 12) {
            fail("Unexpected original audio-group catalog length\n");
        }
        print_words(&count, 1);
        for (uint32_t index = 0; index < count; ++index) {
            raw_get_integer(lua, 3, (int)index + 1);
            catalog_label(lua, 4);
            damage_lua.set_top(lua, 3);
        }
        variants += count;
        damage_lua.set_top(lua, 2);
    }
    if (groups != 46 || variants != 196) {
        fail("Incomplete original audio catalog\n");
    }
    damage_lua.close_state(lua);
    semihost(4, "AB audio catalog reference complete\n");
}
#endif

#if defined(AB_ORACLE_AUDIO_ASSETS_ONLY)
static size_t asset_registration_count;
static size_t asset_property_count;

static int capture_audio_registration(void *lua) {
    int (*next)(void *, int) =
        (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5 ||
        ++asset_registration_count > 340) {
        fail("Unexpected original createAudio arguments\n");
    }
    uint32_t fields = 0;
    damage_lua.push_nil(lua);
    while (next(lua, 1)) {
        ++fields;
        damage_lua.set_top(lua, 2);
    }
    if (fields < 2 || fields > 6) {
        fail("Unexpected original audio property count\n");
    }
    print_words(&fields, 1);
    damage_lua.push_nil(lua);
    while (next(lua, 1)) {
        catalog_label(lua, 2);
        int type = damage_lua.type(lua, 3);
        uint32_t words[2] = {(uint32_t)type, 0};
        if (type == 3) {
            FloatBits value = {.number = damage_lua.to_number(lua, 3)};
            words[1] = value.bits;
        } else if (type == 1) {
            words[1] = (uint32_t)damage_lua.to_boolean(lua, 3);
        } else if (type != 4) {
            fail("Unexpected original audio property type\n");
        }
        print_words(words, 2);
        if (type == 4) {
            catalog_label(lua, 3);
        }
        damage_lua.set_top(lua, 2);
    }
    asset_property_count += fields;
    return 0;
}

static void probe_audio_assets(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original audio registration state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.push_string(lua, "android");
    damage_lua.set_field(lua, -10002, "deviceModel");
    load_damage_chunk(lua, "/data/local/tmp/ab520/audio-catalog.lua");
    damage_function(lua, -10002, "createAudio", capture_audio_registration);
    damage_function(lua, -10002, "setChannelCountLimit", damage_require);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 7, 0);
    damage_lua.set_field(lua, 1, "channelVolume");
    damage_lua.set_field(lua, -10002, "audioData");
    semihost(4, "AB_ANDROID_AUDIO_ASSETS_V1 340\n");
    damage_lua.get_field(lua, -10002, "loadAudioAssets");
    if (damage_lua.call(lua, 0, 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original audio registration failed\n");
    }
    if (asset_registration_count != 340 || asset_property_count != 825) {
        fail("Incomplete original audio registration projection\n");
    }
    damage_lua.close_state(lua);
    semihost(4, "AB audio assets reference complete\n");
}
#endif

#endif
