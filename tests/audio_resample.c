#include "ab/audio_mixer.h"
#include "ab/audio_resample.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Audio quality check failed at line %d\n", __LINE__);      \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool signal_checks(void) {
    AbAudioResampler resampler;
    CHECK(ab_audio_resampler_init(&resampler, NULL));
    CHECK(!ab_audio_resampler_init(NULL, NULL));
    int16_t constant[] = {12000, -7000};
    AbPcm16View dc = {constant, 1, 2};
    for (unsigned ratio = 2; ratio <= 3; ++ratio) {
        for (unsigned phase = 0; phase < ratio; ++phase) {
            for (unsigned channel = 0; channel < 2; ++channel) {
                int16_t result;
                CHECK(ab_audio_resampler_sample(&resampler, dc, 0, phase, ratio,
                                                channel, true, &result, NULL));
                CHECK(result == constant[channel]);
            }
        }
    }
    for (unsigned ratio = 2; ratio <= 3; ++ratio) {
        int16_t samples[192];
        double rate = 48000.0 / ratio;
        double frequency = ratio == 2 ? 9000 : 6000;
        const double pi = 3.14159265358979323846;
        for (size_t frame = 0; frame < 96; ++frame) {
            double wave = sin(2 * pi * frequency * (double)frame / rate);
            samples[frame * 2] = (int16_t)round(wave * 12000);
            samples[frame * 2 + 1] = (int16_t)round(wave * -7000);
        }
        AbPcm16View source = {samples, 96, 2};
        for (size_t output_frame = 0; output_frame < 96 * ratio; ++output_frame) {
            for (unsigned channel = 0; channel < 2; ++channel) {
                int16_t actual;
                CHECK(ab_audio_resampler_sample(&resampler, source,
                                                output_frame / ratio,
                                                (unsigned)(output_frame % ratio), ratio,
                                                channel, true, &actual, NULL));
                double wave = sin(2 * pi * frequency * (double)output_frame / 48000);
                double expected = wave * (channel ? -7000 : 12000);
                CHECK(fabs((double)actual - expected) <= 8);
                if (output_frame % ratio == 0)
                    CHECK(actual == samples[(output_frame / ratio) * 2 + channel]);
            }
        }
    }
    int16_t unchanged = 123;
    CHECK(!ab_audio_resampler_sample(&resampler, dc, 1, 0, 2, 0, false, &unchanged,
                                     NULL) &&
          unchanged == 123);
    CHECK(
        !ab_audio_resampler_sample(&resampler, dc, 0, 2, 2, 0, true, &unchanged, NULL));
    return true;
}

static bool mix_checks(void) {
    AbAudioMixer *whole = NULL, *split = NULL;
    AbAudioMixerLimits limits = {4, 512, 1, 65536};
    CHECK(ab_audio_mixer_open(limits, &whole, NULL));
    CHECK(ab_audio_mixer_open(limits, &split, NULL));
    CHECK(ab_audio_mixer_configure_output(whole, 48000, 2, NULL));
    CHECK(ab_audio_mixer_configure_output(split, 48000, 2, NULL));
    CHECK(!ab_audio_mixer_configure_output(split, 44100, 2, NULL));
    CHECK(ab_audio_mixer_view(split).sample_rate == 48000 &&
          ab_audio_mixer_view(split).channels == 2);
    const int16_t samples[] = {0, 0, 12000, -6000, 0, 0, -12000, 6000};
    AbPcm16View source = {samples, 4, 2};
    for (unsigned ratio = 2; ratio <= 3; ++ratio) {
        CHECK(ab_audio_mixer_start(whole, true, NULL));
        CHECK(ab_audio_mixer_start(split, true, NULL));
        uint32_t first, second;
        uint32_t rate = 48000 / ratio;
        CHECK(ab_audio_mixer_play_rate(whole, 1, source, rate, .5f, false, 0, &first,
                                       NULL));
        CHECK(ab_audio_mixer_play_rate(split, 1, source, rate, .5f, false, 0, &second,
                                       NULL));
        CHECK(!ab_audio_mixer_configure_output(split, 48000, 2, NULL));
        int16_t expected[32], actual[32];
        size_t frames = source.frames * ratio;
        CHECK(ab_audio_mixer_mix(whole, frames, expected, 32, NULL));
        for (size_t frame = 0; frame < frames; ++frame)
            CHECK(ab_audio_mixer_mix(split, 1, actual + frame * 2, 2, NULL));
        CHECK(!memcmp(expected, actual, frames * 2 * sizeof(int16_t)));
        CHECK(ab_audio_mixer_view(split).active[0].cursor_frames == source.frames);
        CHECK(ab_audio_mixer_view(split).active[0].rate_phase == 0);
        CHECK(ab_audio_mixer_mix(whole, 1, expected, 32, NULL));
        CHECK(expected[0] == 0 && expected[1] == 0);
        CHECK(ab_audio_mixer_mix(split, 1, actual, 32, NULL));
        CHECK(ab_audio_mixer_mix(whole, 0, NULL, 0, NULL));
        CHECK(ab_audio_mixer_mix(split, 0, NULL, 0, NULL));
        CHECK(!ab_audio_mixer_view(whole).active_count &&
              !ab_audio_mixer_view(split).active_count);
    }
    ab_audio_mixer_close(split);
    ab_audio_mixer_close(whole);
    return true;
}

int main(void) {
    if (!signal_checks() || !mix_checks())
        return 1;
    puts("Audio quality: exact source phases, independent stereo channels, 16/24-kHz "
         "high-frequency signals, loop boundaries, playback duration and chunk "
         "invariance.");
    return 0;
}
