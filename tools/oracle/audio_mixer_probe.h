#ifndef AB_ORACLE_AUDIO_MIXER_H
#define AB_ORACLE_AUDIO_MIXER_H

static void probe_audio_mixer(void) {
    void *(*allocate)(size_t) = (void *(*)(size_t))find_function("_Znwj");
    void (*construct_mixer)(void *) =
        (void (*)(void *))find_function("_ZN5audio10AudioMixerC1Ev");
    void (*destroy_mixer)(void *) =
        (void (*)(void *))find_function("_ZN5audio10AudioMixerD0Ev");
    void (*start)(void *) =
        (void (*)(void *))find_function("_ZN5audio10AudioMixer5startEv");
    void (*construct_clip)(void *, const void *, int, const int32_t *) =
        (void (*)(void *, const void *, int, const int32_t *))find_function(
            "_ZN5audio9AudioClipC1EPKviRKNS_18AudioConfigurationE");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    void (*release)(void *) =
        (void (*)(void *))find_function("_ZN4lang6Object7releaseEv");
    int (*play)(void *, void *, float, int, int, float, float, float, float) =
        (int (*)(void *, void *, float, int, int, float, float, float, float))
            find_function("_ZN5audio10AudioMixer8playClipEPNS_9AudioClipEfbiffff");
    void (*mix)(void *, const int32_t *, void *, int) =
        (void (*)(void *, const int32_t *, void *, int))find_function(
            "_ZN5audio10AudioMixer3mixERKNS_18AudioConfigurationEPvi");
    void (*set_master)(void *, float) = (void (*)(void *, float))find_function(
        "_ZN5audio10AudioMixer15setMasterVolumeEf");
    void (*set_track)(void *, float, int) = (void (*)(void *, float, int))find_function(
        "_ZN5audio10AudioMixer14setTrackVolumeEfi");
#if defined(AB_ORACLE_AUDIO_MIXER_EXTENDED)
    static const float gains[] = {-2.0f, -1.0f, -0.5f, 0.0f,  0.1f, 0.25f,
                                  0.5f,  0.75f, 1.0f,  1.25f, 2.0f, 4.0f};
    size_t cases = 4096;
#if defined(AB_ORACLE_AUDIO_MIXER_MULTIPLE)
    semihost(4, "AB_ANDROID_AUDIO_MIXER_V3 4096\n");
#else
    semihost(4, "AB_ANDROID_AUDIO_MIXER_V2 4096\n");
#endif
#else
    size_t cases = 1;
    semihost(4, "AB_ANDROID_AUDIO_MIXER_V1 1\n");
#endif
    for (size_t item = 0; item < cases; ++item) {
        int16_t input[128];
        int16_t output[64];
        uint32_t input_words[128], output_words[64], accumulation[64];
        uint32_t random = UINT32_C(0x520a0d10) ^ (uint32_t)item * UINT32_C(0x51a75205);
        uint32_t source_channels = 1, output_channels = 1, track = 0;
        FloatBits clip_gain = {.number = 1}, track_gain = {.number = 1},
                  master_gain = {.number = 1};
#if defined(AB_ORACLE_AUDIO_MIXER_EXTENDED)
        source_channels = 1 + (uint32_t)item % 2;
        output_channels = 1 + (uint32_t)(item / 2) % 2;
        track = (uint32_t)(item / 4) % 8;
        clip_gain.number = gains[(item / 4) % 12];
        track_gain.number = gains[random_word(&random) % 12];
        master_gain.number = gains[random_word(&random) % 12];
#endif
        size_t output_count = 32 * output_channels;
        static const int16_t edges[] = {-32768, -32767, -1, 0, 1, 32767, 1234, -1234};
        for (size_t index = 0; index < 128; ++index) {
            int32_t sample = (int32_t)(random_word(&random) % 65536) - 32768;
            input[index] = index < 8 ? edges[index] : (int16_t)sample;
            input_words[index] = (uint16_t)input[index];
        }
        for (size_t index = 0; index < output_count; ++index) {
            output[index] = 0x5555;
        }
        int32_t configuration[] = {(int32_t)source_channels, 16, 16000};
        int32_t output_configuration[] = {(int32_t)output_channels, 16, 16000};
#if defined(AB_ORACLE_AUDIO_MIXER_MULTIPLE)
        uint32_t second_channels = 1 + (uint32_t)(item / 3) % 2;
        uint32_t second_track = (track + 1) % 8;
        FloatBits second_gain = {.number = gains[random_word(&random) % 12]},
                  second_track_gain = {.number = gains[random_word(&random) % 12]};
        int16_t second_input[128];
        uint32_t second_input_words[128];
        for (size_t index = 0; index < 128; ++index) {
            int32_t sample = (int32_t)(random_word(&random) % 65536) - 32768;
            second_input[index] = index < 8 ? edges[7 - index] : (int16_t)sample;
            second_input_words[index] = (uint16_t)second_input[index];
        }
        int32_t second_configuration[] = {(int32_t)second_channels, 16, 16000};
#endif
        void *mixer = allocate(0x88);
        void *clip = allocate(0x10);
        if (!mixer || !clip) {
            fail("Cannot allocate original mixer reference objects\n");
        }
        construct_mixer(mixer);
        construct_clip(clip, input, sizeof(input), configuration);
        claim(clip);
        start(mixer);
        set_master(mixer, master_gain.number);
        set_track(mixer, track_gain.number, (int)track);
#if defined(AB_ORACLE_AUDIO_MIXER_MULTIPLE)
        void *second_clip = allocate(0x10);
        if (!second_clip) {
            fail("Cannot allocate second original audio clip\n");
        }
        construct_clip(second_clip, second_input, sizeof(second_input),
                       second_configuration);
        claim(second_clip);
        set_track(mixer, second_track_gain.number, (int)second_track);
#endif
        int handle =
            play(mixer, clip, clip_gain.number, 0, (int)track, 1.0f, 0.0f, 0.0f, 0.0f);
#if defined(AB_ORACLE_AUDIO_MIXER_MULTIPLE)
        int second_handle = play(mixer, second_clip, second_gain.number, 0,
                                 (int)second_track, 1.0f, 0.0f, 0.0f, 0.0f);
        size_t clip_count = 2;
#else
        size_t clip_count = 1;
#endif
        mix(mixer, output_configuration, output,
            (int)(output_count * sizeof(output[0])));
        const uint32_t *words = mixer;
        uintptr_t active = words[0x68 / 4];
        uintptr_t end = words[0x6c / 4];
        uintptr_t buffer = words[0x5c / 4];
        if (!active || end - active != 40 * clip_count || !buffer ||
            words[0x60 / 4] - buffer != output_count * sizeof(accumulation[0])) {
            fail("Unexpected original active mixer layout\n");
        }
        const uint32_t *instance = (const uint32_t *)active;
        const uint32_t *cursor = (const uint32_t *)(uintptr_t)instance[1];
        uint32_t state[8] = {(uint32_t)handle, cursor[2], cursor[3], instance[7]};
#if defined(AB_ORACLE_AUDIO_MIXER_MULTIPLE)
        instance = (const uint32_t *)(active + 40);
        cursor = (const uint32_t *)(uintptr_t)instance[1];
        state[4] = (uint32_t)second_handle;
        state[5] = cursor[2];
        state[6] = cursor[3];
        state[7] = instance[7];
#endif
        for (size_t index = 0; index < output_count; ++index) {
            output_words[index] = (uint16_t)output[index];
            accumulation[index] = ((const uint32_t *)buffer)[index];
        }
        destroy_mixer(mixer);
        release(clip);
#if defined(AB_ORACLE_AUDIO_MIXER_MULTIPLE)
        release(second_clip);
#endif
#if defined(AB_ORACLE_AUDIO_MIXER_EXTENDED)
        uint32_t settings[] = {source_channels, output_channels,  clip_gain.bits,
                               track_gain.bits, master_gain.bits, track};
        print_words(settings, 6);
#endif
        print_words(input_words, 128);
#if defined(AB_ORACLE_AUDIO_MIXER_MULTIPLE)
        uint32_t second_settings[] = {second_channels,  output_channels,
                                      second_gain.bits, second_track_gain.bits,
                                      master_gain.bits, second_track};
        print_words(second_settings, 6);
        print_words(second_input_words, 128);
#endif
        print_words(output_words, output_count);
        print_words(accumulation, output_count);
        print_words(state, clip_count * 4);
    }
    semihost(4, "AB audio mixer reference complete\n");
}

#endif
