#ifndef AB_ORACLE_AUDIO_MIXER_STATE_H
#define AB_ORACLE_AUDIO_MIXER_STATE_H

typedef struct {
    void *(*allocate)(size_t);
    void (*construct)(void *);
    void (*destroy)(void *);
    void (*construct_clip)(void *, const void *, int, const int32_t *);
    void (*claim)(void *);
    void (*release)(void *);
    void (*start)(void *);
    void (*stop)(void *);
    int (*play)(void *, void *, float, int, int, float, float, float, float);
    void (*stop_clip)(void *, int);
    void (*pause_clip)(void *, int);
    void (*resume_clip)(void *, int);
    void (*stop_all)(void *);
    void (*pause_all)(void *);
    void (*resume_all)(void *);
    void (*set_limit)(void *, int, int);
    void (*set_clip_gain)(void *, int, float);
    void (*set_track_gain)(void *, float, int);
    void (*set_master_gain)(void *, float);
    int (*count)(void *, int);
    int (*is_playing)(void *, int);
    void (*mix)(void *, const int32_t *, void *, int);
    void (*stop_by_clip)(void *, void *);
    void (*pause_by_clip)(void *, void *);
    void (*gain_by_clip)(void *, void *, float);
    int (*playing_by_clip)(void *, void *);
} MixerStateApi;

static MixerStateApi mixer_state_api;

static void initialize_mixer_state_api(void) {
    mixer_state_api = (MixerStateApi){
        .allocate = (void *(*)(size_t))find_function("_Znwj"),
        .construct = (void (*)(void *))find_function("_ZN5audio10AudioMixerC1Ev"),
        .destroy = (void (*)(void *))find_function("_ZN5audio10AudioMixerD0Ev"),
        .construct_clip =
            (void (*)(void *, const void *, int, const int32_t *))find_function(
                "_ZN5audio9AudioClipC1EPKviRKNS_18AudioConfigurationE"),
        .claim = (void (*)(void *))find_function("_ZN4lang6Object5claimEv"),
        .release = (void (*)(void *))find_function("_ZN4lang6Object7releaseEv"),
        .start = (void (*)(void *))find_function("_ZN5audio10AudioMixer5startEv"),
        .stop = (void (*)(void *))find_function("_ZN5audio10AudioMixer4stopEv"),
        .play = (int (*)(void *, void *, float, int, int, float, float, float, float))
            find_function("_ZN5audio10AudioMixer8playClipEPNS_9AudioClipEfbiffff"),
        .stop_clip =
            (void (*)(void *, int))find_function("_ZN5audio10AudioMixer8stopClipEi"),
        .pause_clip =
            (void (*)(void *, int))find_function("_ZN5audio10AudioMixer9pauseClipEi"),
        .resume_clip =
            (void (*)(void *, int))find_function("_ZN5audio10AudioMixer10resumeClipEi"),
        .stop_all =
            (void (*)(void *))find_function("_ZN5audio10AudioMixer9stopClipsEv"),
        .pause_all =
            (void (*)(void *))find_function("_ZN5audio10AudioMixer10pauseClipsEv"),
        .resume_all =
            (void (*)(void *))find_function("_ZN5audio10AudioMixer11resumeClipsEv"),
        .set_limit = (void (*)(void *, int, int))find_function(
            "_ZN5audio10AudioMixer17setClipCountLimitEii"),
        .set_clip_gain = (void (*)(void *, int, float))find_function(
            "_ZN5audio10AudioMixer13setClipVolumeEif"),
        .set_track_gain = (void (*)(void *, float, int))find_function(
            "_ZN5audio10AudioMixer14setTrackVolumeEfi"),
        .set_master_gain = (void (*)(void *, float))find_function(
            "_ZN5audio10AudioMixer15setMasterVolumeEf"),
        .count = (int (*)(void *, int))find_function(
            "_ZN5audio10AudioMixer19getPlayingClipCountEi"),
        .is_playing = (int (*)(void *, int))find_function(
            "_ZN5audio10AudioMixer13isClipPlayingEi"),
        .mix = (void (*)(void *, const int32_t *, void *, int))find_function(
            "_ZN5audio10AudioMixer3mixERKNS_18AudioConfigurationEPvi"),
        .stop_by_clip = (void (*)(void *, void *))find_function(
            "_ZN5audio10AudioMixer8stopClipEPNS_9AudioClipE"),
        .pause_by_clip = (void (*)(void *, void *))find_function(
            "_ZN5audio10AudioMixer9pauseClipEPNS_9AudioClipE"),
        .gain_by_clip = (void (*)(void *, void *, float))find_function(
            "_ZN5audio10AudioMixer13setClipVolumeEPNS_9AudioClipEf"),
        .playing_by_clip = (int (*)(void *, void *))find_function(
            "_ZN5audio10AudioMixer13isClipPlayingEPNS_9AudioClipE"),
    };
}

static size_t mixer_voice_count(const uint32_t *mixer, size_t word) {
    uintptr_t begin = mixer[word], end = mixer[word + 1];
    if (end < begin || (end - begin) % 40 || (end - begin) / 40 > 64) {
        fail("Original mixer voice table exceeds capture bounds\n");
    }
    return (end - begin) / 40;
}

static int mixer_handle(uint32_t value) {
    return value == UINT32_MAX ? -1 : (int)value;
}

static void print_mixer_voices(const uint32_t *mixer, size_t word,
                               void *const clips[2]) {
    size_t count = mixer_voice_count(mixer, word);
    const uint32_t *voice = (const uint32_t *)(uintptr_t)mixer[word];
    for (size_t index = 0; index < count; ++index, voice += 10) {
        void *clip = (void *)(uintptr_t)voice[0];
        if (clip != clips[0] && clip != clips[1]) {
            fail("Unexpected original mixer clip pointer\n");
        }
        const uint8_t *flags = (const uint8_t *)voice + 0x24;
        const uint32_t *cursor = (const uint32_t *)(uintptr_t)voice[1];
        uint32_t values[] = {voice[7],
                             clip == clips[0] ? 0u : 1u,
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
}

static void print_mixer_state(void *object, void *const clips[2],
                              const uint32_t handles[8], uint32_t result,
                              const int16_t *pcm, size_t samples) {
    const uint32_t *mixer = object;
    uint32_t values[24] = {result, ((const uint8_t *)mixer)[0x84], mixer[0x80 / 4],
                           (uint32_t)mixer_voice_count(mixer, 0x68 / 4),
                           (uint32_t)mixer_voice_count(mixer, 0x74 / 4)};
    for (int track = 0; track < 3; ++track) {
        values[5 + (size_t)track] = (uint32_t)mixer_state_api.count(object, track);
        values[16 + (size_t)track] = mixer[0xc / 4 + (size_t)track];
        values[20 + (size_t)track] = mixer[0x30 / 4 + (size_t)track];
    }
    for (size_t index = 0; index < 8; ++index) {
        values[8 + index] =
            (uint32_t)mixer_state_api.is_playing(object, mixer_handle(handles[index]));
    }
    values[19] = mixer[0x2c / 4];
    values[23] = (uint32_t)samples;
    print_words(values, 24);
    if (samples) {
        uint32_t output[32];
        for (size_t index = 0; index < samples; ++index) {
            output[index] = (uint16_t)pcm[index];
        }
        print_words(output, samples);
    }
    print_mixer_voices(mixer, 0x68 / 4, clips);
    print_mixer_voices(mixer, 0x74 / 4, clips);
}

static uint32_t apply_mixer_command(void *mixer, void *const clips[2],
                                    uint32_t handles[8], const uint32_t command[8],
                                    int16_t output[32]) {
    int handle = mixer_handle(handles[command[6]]);
    FloatBits gain = {.bits = command[5]};
    switch (command[0]) {
        case 0:
            mixer_state_api.start(mixer);
            break;
        case 1:
            mixer_state_api.stop(mixer);
            break;
        case 2: {
            int result = mixer_state_api.play(mixer, clips[command[1]], gain.number,
                                              (int)command[4], (int)command[2], 1.0f,
                                              0.0f, 0.0f, 0.0f);
            handles[command[6]] = (uint32_t)result;
            return (uint32_t)result;
        }
        case 3:
            mixer_state_api.stop_clip(mixer, handle);
            break;
        case 4:
            mixer_state_api.pause_clip(mixer, handle);
            break;
        case 5:
            mixer_state_api.resume_clip(mixer, handle);
            break;
        case 6:
            mixer_state_api.stop_all(mixer);
            break;
        case 7:
            mixer_state_api.pause_all(mixer);
            break;
        case 8:
            mixer_state_api.resume_all(mixer);
            break;
        case 9:
            mixer_state_api.set_limit(mixer, (int)command[2], mixer_handle(command[3]));
            break;
        case 10: {
            const int32_t configuration[] = {1, 16, 16000};
            mixer_state_api.mix(mixer, configuration, output, (int)command[7] * 2);
            break;
        }
        case 11:
            mixer_state_api.set_clip_gain(mixer, handle, gain.number);
            break;
        case 12:
            mixer_state_api.set_track_gain(mixer, gain.number, (int)command[2]);
            break;
        case 13:
            mixer_state_api.set_master_gain(mixer, gain.number);
            break;
        case 14:
            mixer_state_api.stop_by_clip(mixer, clips[command[1]]);
            break;
        case 15:
            mixer_state_api.pause_by_clip(mixer, clips[command[1]]);
            break;
        case 16:
            mixer_state_api.gain_by_clip(mixer, clips[command[1]], gain.number);
            break;
        case 17:
            return (uint32_t)mixer_state_api.playing_by_clip(mixer, clips[command[1]]);
        default:
            fail("Unknown mixer state command\n");
    }
    return UINT32_MAX;
}

static void probe_audio_mixer_state(void) {
    static const float gains[] = {-0.5f, 0.0f, 0.25f, 0.5f, 1.0f, 1.5f, 2.0f};
    static const uint32_t frame_counts[] = {0, 1, 4, 8, 16, 32};
    static const uint32_t prefix[32][8] = {
#if defined(AB_ORACLE_AUDIO_MIXER_CLIPS)
        {0},
        {2, 0, 0, 0, 1, 0x3f800000, 0},
        {2, 0, 1, 0, 1, 0x3f000000, 1},
        {2, 1, 2, 0, 1, 0x3e800000, 2},
        {17, 0},
        {16, 0, 0, 0, 0, 0x3e800000},
        {15, 0},
        {10, 0, 0, 0, 0, 0, 0, 4},
        {2, 0, 2, 0, 1, 0x3f000000, 3},
        {17, 0},
        {16, 0, 0, 0, 0, 0x3f000000},
        {14, 0},
        {17, 0},
        {16, 0, 0, 0, 0, 0xbf000000},
        {10},
        {2, 0, 0, 0, 0, 0x3f800000, 4},
        {10, 0, 0, 0, 0, 0, 0, 16},
        {17, 0},
        {10, 0, 0, 0, 0, 0, 0, 1},
        {17, 0},
        {15, 0},
        {16, 0, 0, 0, 0, 0x80000000},
        {10},
        {2, 1, 0, 0, 1, 0x3f800000, 5},
        {15, 1},
        {17, 1},
        {14, 1},
        {17, 1},
        {10},
        {14, 0},
        {17, 0},
        {16, 0, 0, 0, 0, 0x3f000000},
#else
        {0},
        {2, 0, 0, 0, 0, 0x3f800000, 0, 0},
        {2, 1, 1, 0, 1, 0x3f000000, 1, 0},
        {10, 0, 0, 0, 0, 0, 0, 4},
        {4},
        {10, 0, 0, 0, 0, 0, 0, 8},
        {5},
        {9, 0, 0, 0, 0, 0, 0, 0},
        {2, 0, 0, 0, 0, 0x3f800000, 2, 0},
        {10, 0, 0, 0, 0, 0, 0, 16},
        {3, 0, 0, 0, 0, 0, 1, 0},
        {10},
        {2, 0, 2, 0, 0, 0x3f800000, 2, 0},
        {10, 0, 0, 0, 0, 0, 0, 32},
        {10, 0, 0, 0, 0, 0, 0, 32},
        {10},
        {0},
        {9, 0, 0, UINT32_MAX},
        {9, 0, 1, UINT32_MAX},
        {9, 0, 2, UINT32_MAX},
        {2, 0, 0, 0, 1, 0x3f000000, 0, 0},
        {2, 1, 1, 0, 1, 0x3e800000, 1, 0},
        {2, 0, 2, 0, 1, 0x3e800000, 2, 0},
        {10, 0, 0, 0, 0, 0, 0, 4},
        {3, 0, 0, 0, 0, 0, 1, 0},
        {10, 0, 0, 0, 0, 0, 0, 4},
        {2, 1, 1, 0, 1, 0x3f000000, 3, 0},
        {10, 0, 0, 0, 0, 0, 0, 4},
        {4, 0, 0, 0, 0, 0, 2, 0},
        {3, 0, 0, 0, 0, 0, 0, 0},
        {10, 0, 0, 0, 0, 0, 0, 4},
        {10, 0, 0, 0, 0, 0, 0, 16},
#endif
    };
    initialize_mixer_state_api();
#if defined(AB_ORACLE_AUDIO_MIXER_CLIPS)
    size_t fixed_steps = 32;
    semihost(4, "AB_ANDROID_AUDIO_MIXER_STATE_V3 128 64\n");
#elif defined(AB_ORACLE_AUDIO_MIXER_STATE_EXTENDED)
    size_t fixed_steps = 32;
    semihost(4, "AB_ANDROID_AUDIO_MIXER_STATE_V2 128 64\n");
#else
    size_t fixed_steps = 16;
    semihost(4, "AB_ANDROID_AUDIO_MIXER_STATE_V1 128 64\n");
#endif
    for (size_t item = 0; item < 128; ++item) {
        uint32_t random = UINT32_C(0x520c0f33) ^ (uint32_t)item * UINT32_C(0x5a17f005);
        uint32_t channels[2] = {1 + (uint32_t)item % 2, 2 - (uint32_t)item % 2};
        uint32_t sizes[2] = {9, 5};
        uint32_t header[] = {(uint32_t)item, channels[0], sizes[0], channels[1],
                             sizes[1]};
        print_words(header, 5);
        void *mixer = mixer_state_api.allocate(0x88);
        if (!mixer) {
            fail("Cannot allocate native state mixer\n");
        }
        mixer_state_api.construct(mixer);
        int16_t source[2][18];
        void *clips[2];
        for (size_t clip = 0; clip < 2; ++clip) {
            uint32_t input[18];
            size_t count = sizes[clip] * channels[clip];
            for (size_t index = 0; index < count; ++index) {
                int32_t value = (int32_t)(random_word(&random) % 65536) - 32768;
                source[clip][index] = (int16_t)value;
                input[index] = (uint16_t)value;
            }
            print_words(input, count);
            int32_t configuration[] = {(int32_t)channels[clip], 16, 16000};
            clips[clip] = mixer_state_api.allocate(0x10);
            if (!clips[clip]) {
                fail("Cannot allocate native state audio clip\n");
            }
            mixer_state_api.construct_clip(clips[clip], source[clip], (int)count * 2,
                                           configuration);
            mixer_state_api.claim(clips[clip]);
        }
        uint32_t handles[8];
        for (size_t index = 0; index < 8; ++index) {
            handles[index] = UINT32_MAX;
        }
        for (size_t step = 0; step < 64; ++step) {
            uint32_t command[8];
            if (step < fixed_steps) {
                for (size_t index = 0; index < 8; ++index) {
                    command[index] = prefix[step][index];
                }
            } else {
#if defined(AB_ORACLE_AUDIO_MIXER_CLIPS)
                command[0] = random_word(&random) % 18;
#else
                command[0] = random_word(&random) % 14;
#endif
                command[1] = random_word(&random) % 2;
                command[2] = random_word(&random) % 3;
                command[3] = random_word(&random) % 5 - 1;
                command[4] = random_word(&random) % 2;
                FloatBits gain = {.number = gains[random_word(&random) % 7]};
                command[5] = gain.bits;
                command[6] = random_word(&random) % 8;
                command[7] = frame_counts[random_word(&random) % 6];
            }
            int16_t output[32];
            for (size_t index = 0; index < 32; ++index) {
                output[index] = 0x5555;
            }
            print_words(command, 8);
            uint32_t result =
                apply_mixer_command(mixer, clips, handles, command, output);
            size_t count = command[0] == 10 ? command[7] : 0;
            print_mixer_state(mixer, clips, handles, result, output, count);
        }
        mixer_state_api.destroy(mixer);
        mixer_state_api.release(clips[0]);
        mixer_state_api.release(clips[1]);
    }
    semihost(4, "AB audio mixer state reference complete\n");
}

#endif
