#include "ab/audio_mix.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Audio mix failed at %d: %s\n", __LINE__, #condition);     \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float floating(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
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

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file));
    size_t clip_count = strcmp(header, "AB_ANDROID_AUDIO_MIXER_V3 4096\n") == 0 ? 2 : 1;
    CHECK(clip_count == 2 || strcmp(header, "AB_ANDROID_AUDIO_MIXER_V2 4096\n") == 0);
    size_t samples = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t settings[2][6], input_words[128], expected[64], accumulator[64],
            state[8];
        int16_t input[2][128], output[64];
        for (size_t clip = 0; clip < clip_count; ++clip) {
            CHECK(words(file, settings[clip], 6) && settings[clip][0] >= 1 &&
                  settings[clip][0] <= 2 && settings[clip][1] >= 1 &&
                  settings[clip][1] <= 2 && settings[clip][5] < 8 &&
                  words(file, input_words, 128));
            CHECK(!clip || (settings[clip][1] == settings[0][1] &&
                            settings[clip][4] == settings[0][4] &&
                            settings[clip][5] != settings[0][5]));
            for (size_t index = 0; index < 128; ++index) {
                CHECK(input_words[index] <= UINT16_MAX);
                int32_t value = (int32_t)input_words[index];
                input[clip][index] = (int16_t)(value >= 32768 ? value - 65536 : value);
            }
        }
        size_t count = 32 * settings[0][1];
        CHECK(words(file, expected, count) && words(file, accumulator, count) &&
              words(file, state, clip_count * 4));
        float mixed[64] = {0};
        AbAudioMixBuffer destination = {mixed, 32, (uint16_t)settings[0][1]};
        for (size_t clip = 0; clip < clip_count; ++clip) {
            AbPcm16View source = {input[clip], 128 / settings[clip][0],
                                  (uint16_t)settings[clip][0]};
            AbAudioMixGains gains = {floating(settings[clip][2]),
                                     floating(settings[clip][3]),
                                     floating(settings[clip][4])};
            CHECK(ab_audio_pcm16_accumulate(source, gains, destination, NULL));
            CHECK(state[clip * 4] == clip &&
                  state[clip * 4 + 1] == 32 * settings[clip][0] * 2 &&
                  !state[clip * 4 + 2] && state[clip * 4 + 3] == clip);
        }
        CHECK(ab_audio_pcm16_store(destination, output, 64, NULL));
        for (size_t index = 0; index < count; ++index) {
            if (bits(mixed[index]) != accumulator[index] ||
                (uint16_t)output[index] != expected[index]) {
                fprintf(stderr, "Original mixer differs at case %zu sample %zu\n", item,
                        index);
                return false;
            }
        }
        samples += count;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB audio mixer reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original equal-rate PCM16 mixer cases with %zu clip(s) match: "
           "%zu float accumulators and %zu signed-16 outputs\n",
           clip_count, samples, samples);
    return true;
}

static bool edge_checks(void) {
    const int16_t input[] = {INT16_MIN, INT16_MAX, -1, 0, 1, 1234, -1234, 0};
    float samples[8] = {0};
    int16_t output[8];
    AbPcm16View source = {input, 8, 1};
    AbAudioMixBuffer mixed = {samples, 8, 1};
    AbAudioMixGains gains = {1, 1, 1};
    CHECK(ab_audio_pcm16_accumulate(source, gains, mixed, NULL) &&
          ab_audio_pcm16_store(mixed, output, 8, NULL) &&
          memcmp(input, output, sizeof(input)) == 0);
    float before[8];
    memcpy(before, samples, sizeof(before));
    source.frames = 7;
    CHECK(!ab_audio_pcm16_accumulate(source, gains, mixed, NULL) &&
          memcmp(before, samples, sizeof(before)) == 0);
    source.frames = 8;
    gains.clip = NAN;
    CHECK(!ab_audio_pcm16_accumulate(source, gains, mixed, NULL) &&
          memcmp(before, samples, sizeof(before)) == 0);
    gains.clip = 1;
    mixed.channels = 3;
    CHECK(!ab_audio_pcm16_accumulate(source, gains, mixed, NULL));
    mixed.channels = 1;
    for (size_t index = 0; index < 8; ++index) {
        output[index] = 123;
    }
    CHECK(!ab_audio_pcm16_store(mixed, output, 7, NULL));
    samples[7] = NAN;
    CHECK(!ab_audio_pcm16_store(mixed, output, 8, NULL));
    for (size_t index = 0; index < 8; ++index) {
        CHECK(output[index] == 123);
    }
    samples[0] = INFINITY;
    samples[1] = -INFINITY;
    samples[7] = 0;
    CHECK(ab_audio_pcm16_store(mixed, output, 8, NULL) && output[0] == INT16_MAX &&
          output[1] == INT16_MIN);
    source = (AbPcm16View){NULL, 0, 1};
    mixed = (AbAudioMixBuffer){NULL, 0, 2};
    CHECK(ab_audio_pcm16_accumulate(source, gains, mixed, NULL) &&
          ab_audio_pcm16_store(mixed, NULL, 0, NULL));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: ab_audio_mix_tests [NATIVE_CORPUS]\n");
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
