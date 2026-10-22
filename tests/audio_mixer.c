#include "ab/audio_mixer.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Audio mixer failed at %d: %s\n", __LINE__, #condition);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float floating(uint32_t value) {
    float output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static uint32_t bits(float value) {
    uint32_t output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool voice_checks(FILE *file, const AbAudioVoice *voices, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected[11];
        CHECK(words(file, expected, 11));
        const AbAudioVoice *voice = voices + index;
        CHECK(voice->id == expected[0] && voice->clip_id == expected[1] &&
              voice->flags == expected[2] && voice->track == expected[3] &&
              bits(voice->gain) == expected[4] &&
              voice->cursor_frames * voice->source.channels * 2 == expected[5] &&
              !expected[6] && !expected[7] && expected[8] == UINT32_C(0x3f800000) &&
              !expected[9] && !expected[10]);
    }
    return true;
}

static bool state_checks(FILE *file, const AbAudioMixer *mixer,
                         const uint32_t handles[8], uint32_t result,
                         const int16_t output[32], size_t samples) {
    uint32_t expected[24];
    CHECK(words(file, expected, 24));
    AbAudioMixerView view = ab_audio_mixer_view(mixer);
    CHECK(expected[0] == result && expected[1] == view.started &&
          expected[2] == view.next_id && expected[3] == view.active_count &&
          expected[4] == view.pending_count && expected[19] == bits(view.master_gain) &&
          expected[23] == samples);
    for (uint32_t track = 0; track < 3; ++track) {
        size_t count;
        CHECK(ab_audio_mixer_track_count(mixer, track, &count, NULL) &&
              expected[5 + track] == count &&
              expected[16 + track] == bits(view.track_gains[track]) &&
              expected[20 + track] == (uint32_t)view.track_limits[track]);
    }
    for (size_t index = 0; index < 8; ++index) {
        CHECK(expected[8 + index] == ab_audio_mixer_is_playing(mixer, handles[index]));
    }
    if (samples) {
        uint32_t pcm[32];
        CHECK(words(file, pcm, samples));
        for (size_t index = 0; index < samples; ++index) {
            CHECK(pcm[index] == (uint16_t)output[index]);
        }
    }
    CHECK(voice_checks(file, view.active, view.active_count) &&
          voice_checks(file, view.pending, view.pending_count));
    return true;
}

static bool apply_command(AbAudioMixer *mixer, const AbPcm16View source[2],
                          uint32_t handles[8], const uint32_t command[8],
                          uint32_t *result, int16_t output[32], AbError *error) {
    CHECK(command[0] < 18 && command[1] < 2 && command[2] < 3 &&
          (command[3] == UINT32_MAX || command[3] <= 3) && command[4] <= 1 &&
          command[6] < 8 && command[7] <= 32);
    uint32_t handle = handles[command[6]];
    float gain = floating(command[5]);
    *result = UINT32_MAX;
    switch (command[0]) {
        case 0:
        case 1:
            return ab_audio_mixer_start(mixer, command[0] == 0, error);
        case 2:
            if (!ab_audio_mixer_play(mixer, command[1], source[command[1]], gain,
                                     command[4] != 0, command[2], result, error)) {
                return false;
            }
            handles[command[6]] = *result;
            return true;
        case 3:
        case 4:
        case 5:
            return ab_audio_mixer_voice_action(mixer, handle,
                                               command[0] == 3   ? AB_AUDIO_STOP
                                               : command[0] == 4 ? AB_AUDIO_PAUSE
                                                                 : AB_AUDIO_RESUME,
                                               error);
        case 6:
        case 7:
        case 8:
            return ab_audio_mixer_all_action(mixer,
                                             command[0] == 6   ? AB_AUDIO_STOP
                                             : command[0] == 7 ? AB_AUDIO_PAUSE
                                                               : AB_AUDIO_RESUME,
                                             error);
        case 9:
            return ab_audio_mixer_track_limit(
                mixer, command[2], command[3] == UINT32_MAX ? -1 : (int32_t)command[3],
                error);
        case 10:
            return ab_audio_mixer_mix(mixer, command[7], output, 32, error);
        case 11:
            return ab_audio_mixer_voice_gain(mixer, handle, gain, error);
        case 12:
            return ab_audio_mixer_track_gain(mixer, command[2], gain, error);
        case 13:
            return ab_audio_mixer_master_gain(mixer, gain, error);
        case 14:
        case 15:
            return ab_audio_mixer_clip_action(
                mixer, command[1], command[0] == 14 ? AB_AUDIO_STOP : AB_AUDIO_PAUSE,
                error);
        case 16:
            return ab_audio_mixer_clip_gain(mixer, command[1], gain, error);
        case 17:
            *result = ab_audio_mixer_clip_is_playing(mixer, command[1]);
            return true;
    }
    return false;
}

static bool case_checks(FILE *file, size_t item, AbAudioMixer *mixer,
                        const AbPcm16View source[2], size_t *sample_count) {
    uint32_t handles[8];
    for (size_t index = 0; index < 8; ++index) {
        handles[index] = UINT32_MAX;
    }
    for (size_t step = 0; step < 64; ++step) {
        uint32_t command[8], result;
        CHECK(words(file, command, 8));
        int16_t output[32];
        for (size_t index = 0; index < 32; ++index) {
            output[index] = 0x5555;
        }
        AbError error = {0};
        size_t samples = command[0] == 10 ? command[7] : 0;
        if (!apply_command(mixer, source, handles, command, &result, output, &error) ||
            !state_checks(file, mixer, handles, result, output, samples)) {
            fprintf(stderr,
                    "Original audio mixer differs at case %zu step %zu "
                    "operation %" PRIu32 ": %s\n",
                    item, step, command[0], error.message);
            return false;
        }
        *sample_count += samples;
    }
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          (strcmp(header, "AB_ANDROID_AUDIO_MIXER_STATE_V1 128 64\n") == 0 ||
           strcmp(header, "AB_ANDROID_AUDIO_MIXER_STATE_V2 128 64\n") == 0 ||
           strcmp(header, "AB_ANDROID_AUDIO_MIXER_STATE_V3 128 64\n") == 0));
    size_t samples = 0;
    for (size_t item = 0; item < 128; ++item) {
        uint32_t settings[5];
        CHECK(words(file, settings, 5) && settings[0] == item && settings[1] >= 1 &&
              settings[1] <= 2 && settings[2] == 9 && settings[3] >= 1 &&
              settings[3] <= 2 && settings[4] == 5);
        int16_t pcm[2][18];
        AbPcm16View source[2];
        for (size_t clip = 0; clip < 2; ++clip) {
            uint32_t values[18];
            size_t channels = settings[1 + clip * 2];
            size_t frames = settings[2 + clip * 2];
            CHECK(words(file, values, channels * frames));
            for (size_t index = 0; index < channels * frames; ++index) {
                CHECK(values[index] <= UINT16_MAX);
                int32_t value = (int32_t)values[index];
                pcm[clip][index] = (int16_t)(value >= 32768 ? value - 65536 : value);
            }
            source[clip] = (AbPcm16View){pcm[clip], frames, (uint16_t)channels};
        }
        AbAudioMixer *mixer = NULL;
        AbError error = {0};
        CHECK(ab_audio_mixer_open((AbAudioMixerLimits){64, 32, 1, 65536}, &mixer,
                                  &error));
        bool okay = case_checks(file, item, mixer, source, &samples);
        ab_audio_mixer_close(mixer);
        CHECK(okay);
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB audio mixer state reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 8192 original mixer state transitions and %zu PCM samples match: "
           "queue order, limits, gains, pauses, loops, cursors and retirement\n",
           samples);
    return true;
}

static bool edge_checks(void) {
    AbAudioMixer *mixer = NULL;
    AbError error = {0};
    CHECK(!ab_audio_mixer_open((AbAudioMixerLimits){1, 8, 1, 1}, &mixer, &error) &&
          !mixer);
    CHECK(!ab_audio_mixer_open((AbAudioMixerLimits){1, 8, 1, 65536}, NULL, NULL));
    CHECK(ab_audio_mixer_open((AbAudioMixerLimits){1, 8, 1, 65536}, &mixer, &error));
    const int16_t pcm[] = {-1, 0, 1, 1234};
    AbPcm16View source = {pcm, 4, 1};
    uint32_t handle = 99;
    bool okay = ab_audio_mixer_play(mixer, 0, (AbPcm16View){0}, NAN, false, 99, &handle,
                                    &error) &&
                handle == UINT32_MAX && ab_audio_mixer_start(mixer, true, &error) &&
                ab_audio_mixer_play(mixer, 0, source, 1, false, 0, &handle, &error) &&
                handle == 0 && ab_audio_mixer_is_playing(mixer, 0);
    okay = okay && ab_audio_mixer_clip_is_playing(mixer, 0) &&
           !ab_audio_mixer_clip_is_playing(mixer, 1) &&
           !ab_audio_mixer_clip_action(mixer, 0, AB_AUDIO_RESUME, &error) &&
           !ab_audio_mixer_clip_gain(mixer, 0, NAN, &error) &&
           ab_audio_mixer_clip_action(mixer, 1, AB_AUDIO_STOP, &error) &&
           ab_audio_mixer_view(mixer).pending[0].gain == 1 &&
           ab_audio_mixer_view(mixer).pending[0].flags == 0;
    uint32_t other = 99;
    okay = okay &&
           !ab_audio_mixer_play(mixer, 0, source, 1, false, 0, &other, &error) &&
           other == 99 && ab_audio_mixer_view(mixer).next_id == 1;
    int16_t output[8];
    for (size_t index = 0; index < 8; ++index) {
        output[index] = 123;
    }
    okay = okay && !ab_audio_mixer_mix(mixer, 8, output, 7, &error) &&
           ab_audio_mixer_view(mixer).pending_count == 1 && output[0] == 123 &&
           ab_audio_mixer_mix(mixer, 8, output, 8, &error) &&
           memcmp(output, pcm, sizeof(pcm)) == 0 && output[4] == 0 &&
           ab_audio_mixer_is_playing(mixer, 0) &&
           ab_audio_mixer_mix(mixer, 8, output, 8, &error) &&
           !ab_audio_mixer_is_playing(mixer, 0) &&
           ab_audio_mixer_view(mixer).active_count == 1 &&
           ab_audio_mixer_mix(mixer, 0, NULL, 0, &error) &&
           ab_audio_mixer_view(mixer).active_count == 0 &&
           !ab_audio_mixer_master_gain(mixer, NAN, &error) &&
           !ab_audio_mixer_track_gain(mixer, 8, 1, &error) &&
           !ab_audio_mixer_voice_action(mixer, 0, (AbAudioVoiceAction)99, &error);
    ab_audio_mixer_close(mixer);
    CHECK(okay);
    CHECK(!ab_audio_mixer_start(NULL, true, NULL));
    CHECK(!ab_audio_mixer_is_playing(NULL, 0));
    CHECK(!ab_audio_mixer_clip_is_playing(NULL, 0) &&
          !ab_audio_mixer_clip_action(NULL, 0, AB_AUDIO_STOP, NULL) &&
          !ab_audio_mixer_clip_gain(NULL, 0, 1, NULL));
    CHECK(!ab_audio_mixer_view(NULL).active);
    ab_audio_mixer_close(NULL);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: ab_audio_mixer_tests [NATIVE_CORPUS]\n");
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
