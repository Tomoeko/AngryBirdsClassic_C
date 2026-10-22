#include "ab/mp3.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "MP3 synthesis failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool pcm_conversion(void) {
    static const struct {
        float value;
        int16_t expected;
        uint8_t clipped;
    } cases[] = {{32767.0f, 32767, 0},   {32767.9f, 32767, 1}, {-32768.0f, -32768, 0},
                 {-32768.9f, -32768, 1}, {123.9f, 123, 0},     {-123.9f, -123, 0}};
    AbMp3SynthesisTransform transform;
    CHECK(ab_mp3_synthesis_transform_init(&transform, NULL));
    for (size_t index = 0; index < sizeof(cases) / sizeof(*cases); ++index) {
        AbMp3SynthesisWindow window = {0};
        AbMp3SynthesisHistory history;
        CHECK(ab_mp3_synthesis_history_init(&history, NULL));
        history.values[1][256] = cases[index].value;
        window.values[527] = 1.0f;
        float frequency[32] = {0};
        int16_t output[32];
        uint8_t clipped;
        CHECK(ab_mp3_synthesize(&transform, &window, frequency, &history, output,
                                &clipped, NULL) &&
              clipped == cases[index].clipped);
        for (size_t sample = 0; sample < 32; ++sample) {
            CHECK(output[sample] == (sample == 16 ? cases[index].expected : 0));
        }
    }
    return true;
}

static bool edges(void) {
    AbMp3SynthesisTransform transform;
    AbMp3SynthesisWindow window, previous_window;
    AbMp3SynthesisHistory history, previous_history;
    CHECK(ab_mp3_synthesis_transform_init(&transform, NULL) &&
          ab_mp3_synthesis_window_init(1.0, &window, NULL) &&
          ab_mp3_synthesis_history_init(&history, NULL));
    previous_window = window;
    CHECK(!ab_mp3_synthesis_window_init(1.0, NULL, NULL) &&
          !ab_mp3_synthesis_window_init(-1.0, &window, NULL) &&
          !ab_mp3_synthesis_window_init(NAN, &window, NULL) &&
          !ab_mp3_synthesis_window_init(INFINITY, &window, NULL) &&
          !ab_mp3_synthesis_window_init(DBL_MAX, &window, NULL) &&
          memcmp(&window, &previous_window, sizeof(window)) == 0 &&
          !ab_mp3_synthesis_history_init(NULL, NULL));
    float frequency[32] = {0};
    int16_t output[32], previous_output[32];
    uint8_t clipped = 123;
    CHECK(history.phase == 1 &&
          ab_mp3_synthesize(&transform, &window, frequency, &history, output, &clipped,
                            NULL) &&
          clipped == 0 && history.phase == 0);
    for (size_t index = 0; index < 32; ++index) {
        CHECK(output[index] == 0);
        output[index] = 123;
    }
    memcpy(previous_output, output, sizeof(output));
    previous_history = history;
    clipped = 123;
    CHECK(!ab_mp3_synthesize(NULL, &window, frequency, &history, output, &clipped,
                             NULL) &&
          !ab_mp3_synthesize(&transform, NULL, frequency, &history, output, &clipped,
                             NULL) &&
          !ab_mp3_synthesize(&transform, &window, NULL, &history, output, &clipped,
                             NULL) &&
          !ab_mp3_synthesize(&transform, &window, frequency, NULL, output, &clipped,
                             NULL) &&
          !ab_mp3_synthesize(&transform, &window, frequency, &history, NULL, &clipped,
                             NULL) &&
          !ab_mp3_synthesize(&transform, &window, frequency, &history, output, NULL,
                             NULL));
    frequency[15] = NAN;
    CHECK(!ab_mp3_synthesize(&transform, &window, frequency, &history, output, &clipped,
                             NULL));
    frequency[15] = 0.0f;
    window.values[543] = NAN;
    CHECK(!ab_mp3_synthesize(&transform, &window, frequency, &history, output, &clipped,
                             NULL));
    window = previous_window;
    history.values[1][271] = INFINITY;
    CHECK(!ab_mp3_synthesize(&transform, &window, frequency, &history, output, &clipped,
                             NULL));
    history = previous_history;
    history.phase = 16;
    CHECK(!ab_mp3_synthesize(&transform, &window, frequency, &history, output, &clipped,
                             NULL));
    history = previous_history;
    frequency[0] = frequency[31] = FLT_MAX;
    CHECK(!ab_mp3_synthesize(&transform, &window, frequency, &history, output, &clipped,
                             NULL) &&
          memcmp(&history, &previous_history, sizeof(history)) == 0 &&
          memcmp(output, previous_output, sizeof(output)) == 0 && clipped == 123);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
        bool end = (index + 1) % 136 == 0 || index + 1 == count;
        CHECK(fgetc(file) == (end ? '\n' : ' '));
    }
    return true;
}

static bool history_matches(FILE *file, const AbMp3SynthesisHistory *history) {
    for (size_t bank = 0; bank < 2; ++bank) {
        uint32_t expected[272];
        CHECK(words(file, expected, 272));
        for (size_t index = 0; index < 272; ++index) {
            if (bits(history->values[bank][index]) != expected[index]) {
                fprintf(stderr, "Synthesis history bank %zu index %zu differs\n", bank,
                        index);
                return false;
            }
        }
    }
    return true;
}

static bool native_checks(FILE *file) {
    char line[160];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_MP3_SYNTHESIS_V1\n") == 0);
    uint32_t profile[4], scales[4], expected_window[544];
    CHECK(words(file, profile, 4) && words(file, scales, 4) &&
          words(file, expected_window, 544) && (profile[0] == 4 || profile[0] == 5) &&
          (profile[1] == 1 || profile[1] == 2) && profile[2] == 1 &&
          profile[3] == (profile[1] == 1 ? 3 : UINT32_MAX) && scales[0] == 0 &&
          scales[1] == UINT32_C(0x3ff00000) && scales[2] == 0 &&
          scales[3] == scales[1]);
    AbMp3SynthesisTransform transform;
    AbMp3SynthesisWindow window;
    AbMp3SynthesisHistory history[2];
    CHECK(ab_mp3_synthesis_transform_init(&transform, NULL) &&
          ab_mp3_synthesis_window_init(1.0, &window, NULL) &&
          ab_mp3_synthesis_history_init(history, NULL) &&
          ab_mp3_synthesis_history_init(history + 1, NULL));
    for (size_t index = 0; index < 544; ++index) {
        CHECK(bits(window.values[index]) == expected_window[index]);
    }
    for (size_t channel = 0; channel < profile[1]; ++channel) {
        CHECK(history_matches(file, history + channel));
    }
    size_t calls = 0;
    for (;;) {
        uint32_t metadata[7];
        CHECK(fgets(line, sizeof(line), file));
        int count = sscanf(line,
                           "%" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32
                           " %" SCNx32 " %" SCNx32,
                           metadata, metadata + 1, metadata + 2, metadata + 3,
                           metadata + 4, metadata + 5, metadata + 6);
        if (count == 1) {
            CHECK(metadata[0] == calls && calls);
            break;
        }
        size_t channel = calls % profile[1];
        CHECK(count == 7 && metadata[0] == calls &&
              metadata[1] == calls / (18 * profile[1]) && metadata[2] == channel &&
              metadata[3] == (profile[1] == 1 ? 0 : channel) &&
              metadata[4] == history[0].phase);
        uint32_t encoded[32], expected_pcm[32];
        float frequency[32];
        CHECK(words(file, encoded, 32) && words(file, expected_pcm, 32));
        for (size_t index = 0; index < 32; ++index) {
            memcpy(frequency + index, encoded + index, sizeof(float));
        }
        int16_t pcm[32];
        uint8_t clipped;
        CHECK(ab_mp3_synthesize(&transform, &window, frequency, history + channel, pcm,
                                &clipped, NULL) &&
              clipped == metadata[6] && history[channel].phase == metadata[5]);
        for (size_t index = 0; index < 32; ++index) {
            if ((uint16_t)pcm[index] != expected_pcm[index]) {
                fprintf(stderr, "Synthesis call %zu sample %zu: %d != %04" PRIx32 "\n",
                        calls, index, (int)pcm[index], expected_pcm[index]);
                return false;
            }
        }
        CHECK(history_matches(file, history + channel));
        ++calls;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB MP3 synthesis reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf(
        "Compared %zu native synthesis calls, %zu PCM samples and %zu history words\n",
        calls, calls * 32, calls * 544);
    return true;
}

int main(int argc, char **argv) {
    if (!edges() || !pcm_conversion()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if (argc != 2) {
        fprintf(stderr, "Usage: ab_mp3_synthesis_tests [NATIVE_CORPUS]\n");
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && native_checks(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
