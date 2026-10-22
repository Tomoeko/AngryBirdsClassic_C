#ifndef AB_ORACLE_AUDIO_RAMP_H
#define AB_ORACLE_AUDIO_RAMP_H

static float ar_gains[8];

static int ar_track(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 3 ||
        damage_lua.type(lua, 2) != 3)
        fail("Unexpected original audio ramp track arguments\n");
    float gain = damage_lua.to_number(lua, 1), track = damage_lua.to_number(lua, 2);
    if (!(gain >= 0 && gain <= 1) || track < 0 || track > 7 ||
        track != (float)(unsigned)track)
        fail("Unsupported original audio ramp track domain\n");
    ar_gains[(unsigned)track] = gain;
    return 0;
}

static void ar_volume(void *lua, const char *name, unsigned profile) {
    static const float volumes[] = {0, 0, 0, 0.5f, 1, -0.25f, 1.25f, 0.9375f};
    if (!profile)
        damage_lua.push_nil(lua);
    else if (profile == 1)
        damage_lua.push_boolean(lua, 0);
    else
        damage_lua.push_number(lua, volumes[profile]);
    damage_lua.set_field(lua, -10002, name);
}

static void ar_value(void *lua, const char *name, uint32_t *output) {
    damage_lua.get_field(lua, -10002, name);
    output[0] = (uint32_t)damage_lua.type(lua, 1);
    output[1] = 0;
    if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, 1)};
        output[1] = value.bits;
    } else if (output[0] == 1)
        output[1] = (uint32_t)damage_lua.to_boolean(lua, 1);
    else if (output[0] != 0)
        fail("Unexpected original audio ramp field\n");
    damage_lua.set_top(lua, 0);
}

static void ar_snapshot(void *lua, unsigned case_id, unsigned step) {
    uint32_t header[] = {case_id, step}, state[4], gains[8];
    ar_value(lua, "musicRampVolume", state);
    ar_value(lua, "effectsRampVolume", state + 2);
    for (unsigned index = 0; index < 8; ++index) {
        FloatBits value = {.number = ar_gains[index]};
        gains[index] = value.bits;
    }
    print_words(header, 2);
    print_words(state, 4);
    print_words(gains, 4);
    print_words(gains + 4, 4);
}

static void probe_audio_ramp(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original audio ramp state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    static const char *paths[] = {"/data/local/tmp/ab520/audio-ramp/audio.lua",
                                  "/data/local/tmp/ab520/audio-ramp/music.lua",
                                  "/data/local/tmp/ab520/audio-ramp/effects.lua",
                                  "/data/local/tmp/ab520/audio-ramp/set-music.lua",
                                  "/data/local/tmp/ab520/audio-ramp/set-effects.lua"};
    static const char *names[] = {"updateAudioRamp", "updateMusicRamp",
                                  "updateEffectsRamp", "setMusicVolume",
                                  "setEffectsVolume"};
    for (unsigned index = 0; index < 5; ++index)
        load_selected_function(lua, paths[index], names[index]);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "setTrackVolume", ar_track);
    damage_lua.set_field(lua, -10002, "res");
    semihost(4, "AB_ANDROID_AUDIO_RAMP_V1 64 5\n");
    static const float steps[] = {0.125f, 0, 0.25f, -0.0625f};
    for (unsigned case_id = 0; case_id < 64; ++case_id) {
        ar_volume(lua, "musicRampVolume", case_id & 7);
        ar_volume(lua, "effectsRampVolume", case_id >> 3);
        damage_lua.push_number(lua, case_id & 16 ? 0 : case_id & 8 ? -2 : 2);
        damage_lua.set_field(lua, -10002, "musicRampLength");
        damage_lua.push_number(lua, case_id & 32 ? 0 : case_id & 4 ? -0.5f : 0.5f);
        damage_lua.set_field(lua, -10002, "effectsRampLength");
        for (unsigned track = 0; track < 8; ++track)
            ar_gains[track] = 1;
        ar_snapshot(lua, case_id, 0);
        for (unsigned step = 1; step <= 4; ++step) {
            damage_lua.get_field(lua, -10002, "updateAudioRamp");
            damage_lua.push_number(lua, steps[step - 1]);
            if (damage_lua.call(lua, 1, 0, 0)) {
                const char *error = damage_lua.to_string(lua, -1, NULL);
                fail(error ? error : "Original audio ramp update failed\n");
            }
            ar_snapshot(lua, case_id, step);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB audio ramp reference complete\n");
}

#endif
