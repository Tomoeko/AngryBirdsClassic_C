#ifndef AB_ORACLE_OBJECT_FORCE_AUDIO_CONTEXT_H
#define AB_ORACLE_OBJECT_FORCE_AUDIO_CONTEXT_H

/* Original Resources -> AudioOutput -> in-memory mixer only. No platform audio
 * output is constructed and no mix/device/write function is called. */
static struct {
    void (*construct)(void *);
    void (*destroy)(void *);
    void (*start)(void *);
    void (*release)(void *);
    void (*seed)(int32_t);
    int32_t (*next)(void);
    void (*set_index)(void *, int, int);
    uint32_t implementation[0x9c / 4];
    uint32_t resources[0x4d0 / 4];
    uint32_t output[3];
    uint32_t nodes[3][6];
    void *clips[3];
} force_audio;

static const struct {
    uint32_t length, capacity, references;
    char bytes[8];
} force_audio_names[] = {{6, 6, UINT32_MAX, "sound1"},
                         {6, 6, UINT32_MAX, "sound2"},
                         {6, 6, UINT32_MAX, "sound3"}};

static void force_audio_open(uint32_t *game) {
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*construct_clip)(void *, const void *, int, const int32_t *) =
        (void (*)(void *, const void *, int, const int32_t *))find_function(
            "_ZN5audio9AudioClipC1EPKviRKNS_18AudioConfigurationE");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    force_audio.construct =
        (void (*)(void *))find_function("_ZN5audio10AudioMixerC1Ev");
    force_audio.destroy = (void (*)(void *))find_function("_ZN5audio10AudioMixerD1Ev");
    force_audio.start =
        (void (*)(void *))find_function("_ZN5audio10AudioMixer5startEv");
    force_audio.release = (void (*)(void *))find_function("_ZN4lang6Object7releaseEv");
    force_audio.seed = (void (*)(int32_t))find_function("srand48");
    force_audio.next = (int32_t (*)(void))find_function("lrand48");
    force_audio.set_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    static const int16_t silence[2] = {0, 0};
    const int32_t configuration[] = {1, 16, 16000};
    for (size_t index = 0; index < 3; ++index) {
        force_audio.clips[index] = allocate(0x10);
        if (!force_audio.clips[index]) {
            fail("Cannot allocate native force-pulse clip metadata\n");
        }
        construct_clip(force_audio.clips[index], silence, 4, configuration);
        claim(force_audio.clips[index]);
        force_audio.nodes[index][0] = index == 1 ? 1u : 0u;
        force_audio.nodes[index][1] =
            (uint32_t)(uintptr_t)(index == 1 ? force_audio.resources + 0x4b0 / 4
                                             : force_audio.nodes[1]);
        force_audio.nodes[index][4] =
            (uint32_t)(uintptr_t)force_audio_names[index].bytes;
        force_audio.nodes[index][5] = (uint32_t)(uintptr_t)force_audio.clips[index];
    }
    force_audio.nodes[1][2] = (uint32_t)(uintptr_t)force_audio.nodes[0];
    force_audio.nodes[1][3] = (uint32_t)(uintptr_t)force_audio.nodes[2];
    force_audio.resources[0x4b4 / 4] = (uint32_t)(uintptr_t)force_audio.nodes[1];
    force_audio.resources[0x4b8 / 4] = (uint32_t)(uintptr_t)force_audio.nodes[0];
    force_audio.resources[0x4bc / 4] = (uint32_t)(uintptr_t)force_audio.nodes[2];
    force_audio.resources[0x4c0 / 4] = 3;
    force_audio.output[2] = (uint32_t)(uintptr_t)force_audio.implementation;
    force_audio.resources[0x20 / 4] = (uint32_t)(uintptr_t)force_audio.output;
    game[0x50 / 4] = (uint32_t)(uintptr_t)force_audio.resources;
}

static void force_audio_begin(void *lua, const uint32_t input[36]) {
    uint32_t *mixer = force_audio.implementation + 0x14 / 4;
    force_audio.construct(mixer);
    force_audio.start(mixer);
    force_audio.seed((int32_t)input[32]);
    if (input[30] == 1) {
        damage_lua.push_string(lua, force_audio_names[input[35] - 1].bytes);
    } else if (input[30] == 2) {
        damage_lua.create_table(lua, (int)input[31], 0);
        for (uint32_t index = 0; index < input[31]; ++index) {
            damage_lua.push_string(lua, force_audio_names[index].bytes);
            force_audio.set_index(lua, -2, (int)index + 1);
        }
    } else if (input[30] == 3) {
        damage_lua.push_boolean(lua, 0);
    } else if (input[30] == 4) {
        damage_lua.push_number(lua, 1);
    }
    if (input[30]) {
        damage_lua.set_field(lua, -2, "soundWhenForceApplied");
    }
    if (input[34]) {
        damage_number(lua, -2, "soundWhenForceAppliedVolume", input[33]);
    }
}

static void force_audio_snapshot(void) {
    uint32_t *mixer = force_audio.implementation + 0x14 / 4;
    uintptr_t begin = mixer[0x74 / 4], end = mixer[0x78 / 4];
    if (end < begin || end - begin > 40 || (end - begin) % 40 ||
        mixer[0x68 / 4] != mixer[0x6c / 4]) {
        fail("Unexpected force-pulse mixer voice count\n");
    }
    uint32_t summary[] = {(uint32_t)force_audio.next(), (uint32_t)((end - begin) / 40),
                          0};
    print_words(summary, 3);
    if (begin != end) {
        const uint32_t *voice = (const uint32_t *)begin;
        const uint32_t *cursor = (const uint32_t *)(uintptr_t)voice[1];
        const uint8_t *flags = (const uint8_t *)voice + 0x24;
        uint32_t identifier = 0;
        for (uint32_t index = 0; index < 3; ++index) {
            if (voice[0] == (uint32_t)(uintptr_t)force_audio.clips[index]) {
                identifier = index + 1;
            }
        }
        if (!identifier) {
            fail("Unexpected force-pulse audio clip identity\n");
        }
        uint32_t values[] = {voice[7],
                             identifier,
                             (uint32_t)flags[0] | (uint32_t)flags[1] << 1 |
                                 (uint32_t)flags[2] << 2,
                             voice[8],
                             voice[2],
                             cursor[2],
                             cursor[3],
                             voice[5],
                             voice[6],
                             voice[3],
                             voice[4]};
        print_words(values, 11);
    }
    force_audio.destroy(mixer);
}

static void force_audio_close(void) {
    for (size_t index = 0; index < 3; ++index) {
        force_audio.release(force_audio.clips[index]);
    }
}

#endif
