#ifndef AB_ORACLE_AUDIO_RESOURCES_H
#define AB_ORACLE_AUDIO_RESOURCES_H

typedef struct {
    char name[128];
    char path[256];
    uint32_t stream;
} OriginalAudioResource;

static OriginalAudioResource original_audio_resources[340];
static size_t original_audio_resource_count;

static void copy_audio_label(void *lua, int argument, char *output, unsigned capacity) {
    unsigned size = 0;
    const char *text = damage_lua.to_string(lua, argument, &size);
    if (damage_lua.type(lua, argument) != 4 || !text || !size || size >= capacity) {
        fail("Unexpected original audio resource label\n");
    }
    for (unsigned index = 0; index < size; ++index) {
        if ((unsigned char)text[index] < 32 || (unsigned char)text[index] > 126) {
            fail("Unexpected original audio resource encoding\n");
        }
        output[index] = text[index];
    }
    output[size] = 0;
}

static int capture_audio_resource(void *lua) {
    if (damage_lua.get_top(lua) != 3 || original_audio_resource_count >= 340 ||
        damage_lua.type(lua, 3) != 1) {
        fail("Unexpected original resolved audio arguments\n");
    }
    OriginalAudioResource *resource =
        original_audio_resources + original_audio_resource_count++;
    copy_audio_label(lua, 1, resource->path, sizeof(resource->path));
    copy_audio_label(lua, 2, resource->name, sizeof(resource->name));
    resource->stream = (uint32_t)damage_lua.to_boolean(lua, 3);
    return 0;
}

static void print_audio_label(const char *text) {
    uint32_t size = 0;
    while (text[size]) {
        ++size;
    }
    print_words(&size, 1);
    semihost(4, text);
    semihost(4, "\n");
}

static void probe_audio_resources(void) {
    static const char *const fields[] = {"channel", "volume", "canBeOverWritten",
                                         "loop"};
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original resolved audio state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.push_string(lua, "android");
    damage_lua.set_field(lua, -10002, "deviceModel");
    load_damage_chunk(lua, "/data/local/tmp/ab520/audio-catalog.lua");
    load_selected_function(lua, "/data/local/tmp/ab520/audio-definition-oracle.lua",
                           "createAudio");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "createAudio", capture_audio_resource);
    damage_lua.set_field(lua, -10002, "res");
    damage_function(lua, -10002, "setChannelCountLimit", damage_require);
    damage_lua.push_string(lua, "audio");
    damage_lua.set_field(lua, -10002, "audioPath");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.create_table(lua, 7, 0);
    damage_lua.set_field(lua, 1, "channelVolume");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "sounds");
    damage_lua.set_field(lua, -10002, "audioData");
    damage_lua.get_field(lua, -10002, "loadAudioAssets");
    if (damage_lua.call(lua, 0, 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original resolved audio load failed\n");
    }
    if (original_audio_resource_count != 340) {
        fail("Incomplete original resolved audio catalog\n");
    }
    damage_lua.get_field(lua, -10002, "audioData");
    damage_lua.get_field(lua, 1, "sounds");
    semihost(4, "AB_ANDROID_AUDIO_RESOURCES_V1 340\n");
    for (size_t index = 0; index < 340; ++index) {
        const OriginalAudioResource *resource = original_audio_resources + index;
        damage_lua.get_field(lua, 2, resource->name);
        if (damage_lua.type(lua, 3) != 5) {
            fail("Missing original stored audio metadata\n");
        }
        damage_lua.get_field(lua, 3, "name");
        if (!damage_named(lua, 4, resource->name)) {
            fail("Different original stored audio name\n");
        }
        damage_lua.set_top(lua, 3);
        uint32_t values[9] = {resource->stream};
        for (size_t field = 0; field < 4; ++field) {
            damage_lua.get_field(lua, 3, fields[field]);
            int type = damage_lua.type(lua, 4);
            values[field * 2 + 1] = (uint32_t)type;
            if (type == 3) {
                FloatBits number = {.number = damage_lua.to_number(lua, 4)};
                values[field * 2 + 2] = number.bits;
            } else if (type == 1) {
                values[field * 2 + 2] = (uint32_t)damage_lua.to_boolean(lua, 4);
            } else if (type != 0) {
                fail("Unexpected original stored audio scalar\n");
            }
            damage_lua.set_top(lua, 3);
        }
        print_audio_label(resource->name);
        print_audio_label(resource->path);
        print_words(values, 9);
        damage_lua.set_top(lua, 2);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB audio resources reference complete\n");
}

#endif
