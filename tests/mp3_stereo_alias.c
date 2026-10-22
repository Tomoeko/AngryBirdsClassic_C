#include "ab/mp3.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "MP3 stereo/alias failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edges(void) {
    float left[576] = {0}, right[576] = {0}, saved[576];
    left[0] = 2.0f;
    right[0] = 0.5f;
    left[18] = -0.0f;
    right[18] = -3.0f;
    CHECK(ab_mp3_mid_side_spectrum(1, left, right, NULL) && left[0] == 2.5f &&
          right[0] == 1.5f && bits(left[18]) == UINT32_C(0x80000000) &&
          right[18] == -3.0f);
    memcpy(saved, left, sizeof(saved));
    CHECK(!ab_mp3_mid_side_spectrum(0, left, right, NULL) &&
          !ab_mp3_mid_side_spectrum(33, left, right, NULL) &&
          !ab_mp3_mid_side_spectrum(1, NULL, right, NULL) &&
          !ab_mp3_mid_side_spectrum(1, left, left, NULL) &&
          memcmp(saved, left, sizeof(saved)) == 0);
    left[0] = FLT_MAX;
    right[0] = FLT_MAX;
    memcpy(saved, left, sizeof(saved));
    CHECK(!ab_mp3_mid_side_spectrum(32, left, right, NULL) &&
          memcmp(saved, left, sizeof(saved)) == 0 && right[0] == FLT_MAX);
    left[0] = NAN;
    CHECK(!ab_mp3_mid_side_spectrum(32, left, right, NULL) && isnan(left[0]));

    AbMp3Granule granule = {0};
    memset(left, 0, sizeof(left));
    left[17] = -0.0f;
    left[18] = 4.0f;
    memcpy(saved, left, sizeof(saved));
    CHECK(ab_mp3_alias_spectrum(&granule, 1, left, NULL) &&
          memcmp(saved, left, sizeof(saved)) == 0);
    granule.window_switch = true;
    granule.block_type = 2;
    CHECK(ab_mp3_alias_spectrum(&granule, 32, left, NULL) &&
          memcmp(saved, left, sizeof(saved)) == 0);
    granule.mixed_block = true;
    CHECK(!ab_mp3_alias_spectrum(&granule, 32, left, NULL));
    granule = (AbMp3Granule){0};
    CHECK(!ab_mp3_alias_spectrum(NULL, 32, left, NULL) &&
          !ab_mp3_alias_spectrum(&granule, 32, NULL, NULL) &&
          !ab_mp3_alias_spectrum(&granule, 0, left, NULL) &&
          !ab_mp3_alias_spectrum(&granule, 33, left, NULL));
    granule.block_type = 1;
    CHECK(!ab_mp3_alias_spectrum(&granule, 32, left, NULL) &&
          memcmp(saved, left, sizeof(saved)) == 0);
    granule = (AbMp3Granule){0};
    left[17] = FLT_MAX;
    left[18] = FLT_MAX;
    memcpy(saved, left, sizeof(saved));
    CHECK(!ab_mp3_alias_spectrum(&granule, 2, left, NULL) &&
          memcmp(saved, left, sizeof(saved)) == 0);
    left[575] = INFINITY;
    CHECK(!ab_mp3_alias_spectrum(&granule, 1, left, NULL) && isinf(left[575]));
    int16_t quantized[576] = {0};
    AbMp3SpectralLimits limits = {0}, previous;
    CHECK(ab_mp3_spectral_limits(&granule, 16000, quantized, &limits, NULL) &&
          limits.active_subbands == 1 && limits.long_band == 0);
    quantized[575] = -8206;
    CHECK(ab_mp3_spectral_limits(&granule, 24000, quantized, &limits, NULL) &&
          limits.active_subbands == 32 && limits.long_band == 22);
    previous = limits;
    quantized[0] = 8207;
    CHECK(!ab_mp3_spectral_limits(&granule, 16000, quantized, &limits, NULL) &&
          memcmp(&previous, &limits, sizeof(limits)) == 0);
    quantized[0] = 0;
    CHECK(!ab_mp3_spectral_limits(&granule, 44100, quantized, &limits, NULL) &&
          !ab_mp3_spectral_limits(&granule, 16000, NULL, &limits, NULL) &&
          !ab_mp3_spectral_limits(NULL, 16000, quantized, &limits, NULL) &&
          !ab_mp3_spectral_limits(&granule, 16000, quantized, NULL, NULL));
    granule.window_switch = true;
    granule.block_type = 2;
    CHECK(ab_mp3_spectral_limits(&granule, 22050, quantized, &limits, NULL) &&
          limits.active_subbands == 32 && limits.short_bands[0] == 0 &&
          limits.short_bands[1] == 0 && limits.short_bands[2] == 13);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool coefficients(FILE *file) {
    uint32_t cosine[8], sine[8];
    CHECK(words(file, cosine, 8) && words(file, sine, 8));
    AbMp3Granule granule = {0};
    float frequency[576] = {0};
    for (size_t index = 0; index < 8; ++index) {
        frequency[17 - index] = 1.0f;
    }
    CHECK(ab_mp3_alias_spectrum(&granule, 2, frequency, NULL));
    for (size_t index = 0; index < 8; ++index) {
        CHECK(bits(frequency[17 - index]) == cosine[index] &&
              bits(frequency[18 + index]) == sine[index]);
    }
    return true;
}

static bool spectrum_matches(const float actual[576], const uint32_t expected[576],
                             uint32_t frame, size_t channel, const char *stage) {
    for (size_t index = 0; index < 576; ++index) {
        if (bits(actual[index]) != expected[index]) {
            fprintf(stderr,
                    "%s frame %" PRIu32 " channel %zu value %zu: "
                    "%08" PRIx32 " != %08" PRIx32 "\n",
                    stage, frame, channel, index, bits(actual[index]), expected[index]);
            return false;
        }
    }
    return true;
}

static bool compare_frame(FILE *file, const AbMp3FrameInfo *info, AbSpan window,
                          uint32_t frame) {
    float reconstructed[2][576];
    uint32_t before[2][576], after[2][576], active[2];
    AbMp3SpectralLimits limits[2];
    bool joint = info->channels == 2 && (info->header >> 6 & 3) == 1;
    CHECK(!joint || !(info->header & 16)); /* Intensity stereo remains open. */
    bool mid_side = joint && (info->header & 32) != 0;
    size_t part_offset = 0;
    for (size_t channel = 0; channel < info->channels; ++channel) {
        const AbMp3Granule *granule = info->side.channel + channel;
        uint32_t meta[8], expected[576];
        CHECK(words(file, meta, 8) && meta[0] == granule->block_type &&
              meta[1] == (uint32_t)granule->mixed_block && meta[3] >= 1 &&
              meta[3] <= 32);
        active[channel] = meta[3];
        for (size_t index = 0; index < 576; index += 192) {
            CHECK(words(file, expected + index, 192));
        }
        for (size_t index = 0; index < 576; index += 192) {
            CHECK(words(file, before[channel] + index, 192));
        }
        for (size_t index = 0; index < 576; index += 192) {
            CHECK(words(file, after[channel] + index, 192));
        }
        AbMp3Scalefactors scales;
        int16_t quantized[576];
        CHECK(ab_mp3_scalefactors(granule, false, window, part_offset, &scales, NULL) &&
              ab_mp3_quantized_spectrum(granule, info->sample_rate, window, part_offset,
                                        scales.bit_count, quantized, NULL) &&
              ab_mp3_requantize_spectrum(granule, &scales, info->sample_rate, mid_side,
                                         quantized, reconstructed[channel], NULL) &&
              ab_mp3_spectral_limits(granule, info->sample_rate, quantized,
                                     limits + channel, NULL) &&
              limits[channel].active_subbands == meta[2] &&
              limits[channel].long_band == meta[4] &&
              spectrum_matches(reconstructed[channel], expected, frame, channel,
                               "Requantization"));
        for (size_t window_index = 0; window_index < 3; ++window_index) {
            CHECK(limits[channel].short_bands[window_index] == meta[5 + window_index]);
        }
        part_offset += granule->part23_length;
    }
    if (mid_side) {
        uint8_t shared = limits[0].active_subbands > limits[1].active_subbands
                             ? limits[0].active_subbands
                             : limits[1].active_subbands;
        CHECK(
            ab_mp3_mid_side_spectrum(shared, reconstructed[0], reconstructed[1], NULL));
        limits[0].active_subbands = shared;
        limits[1].active_subbands = shared;
    }
    for (size_t channel = 0; channel < info->channels; ++channel) {
        CHECK(limits[channel].active_subbands == active[channel] &&
              spectrum_matches(reconstructed[channel], before[channel], frame, channel,
                               "Stereo") &&
              ab_mp3_alias_spectrum(info->side.channel + channel,
                                    (uint8_t)active[channel], reconstructed[channel],
                                    NULL) &&
              spectrum_matches(reconstructed[channel], after[channel], frame, channel,
                               "Alias"));
    }
    return true;
}

static bool native_checks(FILE *file, AbSpan source) {
    AbMp3 stream;
    CHECK(ab_mp3_read(source, &stream, NULL));
    char line[160];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_MP3_STEREO_ALIAS_V2\n") == 0 && coefficients(file));
    AbMp3Reservoir reservoir = {0};
    uint8_t joined[1024];
    size_t offset = 0, source_index = 0, frames = 0, granules = 0;
    for (;;) {
        uint32_t meta[3];
        CHECK(fgets(line, sizeof(line), file));
        int count =
            sscanf(line, "%" SCNx32 " %" SCNx32 " %" SCNx32, meta, meta + 1, meta + 2);
        if (count == 1) {
            CHECK(meta[0] == frames && frames);
            break;
        }
        CHECK(count == 3 && meta[1] == frames && meta[2] >= 1 && meta[2] <= 2);
        size_t target = meta[1] + (stream.has_xing ? 1u : 0u);
        CHECK(target < stream.frame_count);
        AbMp3FrameInfo info = {0};
        AbSpan window = {0};
        while (source_index <= target) {
            CHECK(offset < stream.frames.size &&
                  ab_mp3_frame_info((AbSpan){stream.frames.data + offset,
                                             stream.frames.size - offset},
                                    &info, NULL) &&
                  ab_mp3_reservoir_push(&reservoir, &info, joined, sizeof(joined),
                                        &window, NULL));
            offset += info.frame.size;
            ++source_index;
        }
        CHECK(info.header == meta[0] && info.channels == meta[2] &&
              compare_frame(file, &info, window, meta[1]));
        ++frames;
        granules += info.channels;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB MP3 stereo alias reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared %zu native frames, %zu granules, %zu stereo/alias float values\n",
           frames, granules, granules * 576 * 2);
    return true;
}

int main(int argc, char **argv) {
    if (!edges()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if (argc != 3) {
        fprintf(stderr, "Usage: ab_mp3_stereo_alias_tests [NATIVE_CORPUS MP3]\n");
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    AbBuffer source = {0};
    bool okay = file && ab_file_read(argv[2], 4 * 1024 * 1024, &source, NULL) &&
                native_checks(file, (AbSpan){source.data, source.size});
    if (file && fclose(file) != 0) {
        okay = false;
    }
    ab_buffer_free(&source);
    return okay ? 0 : 1;
}
