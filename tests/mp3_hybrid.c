#include "ab/mp3.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "MP3 hybrid failed at %d: %s\n", __LINE__, #condition);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbMp3TransformCoefficients coefficients;
    AbMp3HybridWindows windows;
    AbMp3HybridOverlap overlap[2];
} HybridFixture;

typedef struct {
    uint32_t metadata[8];
    uint32_t spectrum[576];
    uint32_t previous[576];
    uint32_t matrix[576];
    uint32_t following[576];
    float reconstructed[576];
    AbMp3SpectralLimits limits;
} NativeChannel;

static bool initialize(HybridFixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    CHECK(ab_mp3_transform_init(&fixture->coefficients, NULL) &&
          ab_mp3_hybrid_windows_init(&fixture->windows, NULL));
    return true;
}

static bool edges(void) {
    HybridFixture fixture;
    CHECK(initialize(&fixture));
    float frequency[576] = {0}, output[576], saved[576];
    AbMp3Granule granule = {0};
    for (size_t slot = 0; slot < 18; ++slot) {
        fixture.overlap[0].values[31 * 18 + slot] = (float)slot * 0.5f;
    }
    CHECK(ab_mp3_hybrid_channel(&fixture.coefficients, &fixture.windows, &granule, 1,
                                frequency, fixture.overlap, output, NULL));
    for (size_t slot = 0; slot < 18; ++slot) {
        CHECK(output[slot * 32 + 31] == (float)slot * 0.5f);
    }
    for (size_t index = 0; index < 576; ++index) {
        CHECK(fixture.overlap[0].values[index] == 0.0f);
        output[index] = -11.0f;
    }
    fixture.overlap[0].values[575] = 123.0f;
    AbMp3HybridOverlap previous = fixture.overlap[0];
    memcpy(saved, output, sizeof(saved));
    CHECK(!ab_mp3_hybrid_channel(&fixture.coefficients, &fixture.windows, &granule, 0,
                                 frequency, fixture.overlap, output, NULL) &&
          !ab_mp3_hybrid_channel(&fixture.coefficients, &fixture.windows, &granule, 33,
                                 frequency, fixture.overlap, output, NULL) &&
          !ab_mp3_hybrid_channel(NULL, &fixture.windows, &granule, 1, frequency,
                                 fixture.overlap, output, NULL) &&
          !ab_mp3_hybrid_channel(&fixture.coefficients, NULL, &granule, 1, frequency,
                                 fixture.overlap, output, NULL) &&
          !ab_mp3_hybrid_channel(&fixture.coefficients, &fixture.windows, NULL, 1,
                                 frequency, fixture.overlap, output, NULL) &&
          !ab_mp3_hybrid_channel(&fixture.coefficients, &fixture.windows, &granule, 1,
                                 NULL, fixture.overlap, output, NULL) &&
          !ab_mp3_hybrid_channel(&fixture.coefficients, &fixture.windows, &granule, 1,
                                 frequency, NULL, output, NULL) &&
          !ab_mp3_hybrid_channel(&fixture.coefficients, &fixture.windows, &granule, 1,
                                 frequency, fixture.overlap, NULL, NULL));
    granule.block_type = 2;
    granule.window_switch = true;
    granule.mixed_block = true;
    CHECK(!ab_mp3_hybrid_channel(&fixture.coefficients, &fixture.windows, &granule, 1,
                                 frequency, fixture.overlap, output, NULL));
    granule = (AbMp3Granule){0};
    frequency[575] = INFINITY;
    CHECK(!ab_mp3_hybrid_channel(&fixture.coefficients, &fixture.windows, &granule, 32,
                                 frequency, fixture.overlap, output, NULL));
    frequency[575] = FLT_MAX;
    frequency[574] = FLT_MAX;
    CHECK(!ab_mp3_hybrid_channel(&fixture.coefficients, &fixture.windows, &granule, 32,
                                 frequency, fixture.overlap, output, NULL) &&
          memcmp(saved, output, sizeof(saved)) == 0 &&
          memcmp(&previous, fixture.overlap, sizeof(previous)) == 0);
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool spectrum_words(FILE *file, uint32_t output[576]) {
    for (size_t index = 0; index < 576; index += 192) {
        CHECK(words(file, output + index, 192));
    }
    return true;
}

static bool matches(const float actual[576], const uint32_t expected[576],
                    uint32_t frame, size_t channel, const char *label) {
    for (size_t index = 0; index < 576; ++index) {
        uint32_t value;
        memcpy(&value, actual + index, sizeof(value));
        if (value != expected[index]) {
            fprintf(stderr,
                    "%s frame %" PRIu32 " channel %zu index %zu: "
                    "%08" PRIx32 " != %08" PRIx32 "\n",
                    label, frame, channel, index, value, expected[index]);
            return false;
        }
    }
    return true;
}

static bool compare_frame(FILE *file, HybridFixture *fixture,
                          const AbMp3FrameInfo *info, AbSpan window, uint32_t frame) {
    NativeChannel channels[2];
    bool joint = info->channels == 2 && ((info->header >> 6) & 3) == 1;
    bool mid_side = joint && (info->header & 32) != 0;
    CHECK(!joint || !(info->header & 16));
    size_t part_offset = 0;
    for (size_t channel = 0; channel < info->channels; ++channel) {
        NativeChannel *values = channels + channel;
        const AbMp3Granule *granule = info->side.channel + channel;
        CHECK(words(file, values->metadata, 8) &&
              values->metadata[0] == granule->block_type &&
              values->metadata[1] == (uint32_t)granule->mixed_block &&
              spectrum_words(file, values->spectrum) &&
              spectrum_words(file, values->previous) &&
              spectrum_words(file, values->matrix) &&
              spectrum_words(file, values->following));
        AbMp3Scalefactors scales;
        int16_t quantized[576];
        CHECK(ab_mp3_scalefactors(granule, false, window, part_offset, &scales, NULL) &&
              ab_mp3_quantized_spectrum(granule, info->sample_rate, window, part_offset,
                                        scales.bit_count, quantized, NULL) &&
              ab_mp3_requantize_spectrum(granule, &scales, info->sample_rate, mid_side,
                                         quantized, values->reconstructed, NULL) &&
              ab_mp3_spectral_limits(granule, info->sample_rate, quantized,
                                     &values->limits, NULL) &&
              values->limits.active_subbands == values->metadata[2] &&
              values->limits.long_band == values->metadata[4]);
        for (size_t part = 0; part < 3; ++part) {
            CHECK(values->limits.short_bands[part] == values->metadata[5 + part]);
        }
        part_offset += granule->part23_length;
    }
    if (mid_side) {
        uint8_t shared =
            channels[0].limits.active_subbands > channels[1].limits.active_subbands
                ? channels[0].limits.active_subbands
                : channels[1].limits.active_subbands;
        CHECK(ab_mp3_mid_side_spectrum(shared, channels[0].reconstructed,
                                       channels[1].reconstructed, NULL));
        channels[0].limits.active_subbands = shared;
        channels[1].limits.active_subbands = shared;
    }
    for (size_t channel = 0; channel < info->channels; ++channel) {
        NativeChannel *values = channels + channel;
        float matrix[576];
        CHECK(values->limits.active_subbands == values->metadata[3] &&
              ab_mp3_alias_spectrum(info->side.channel + channel,
                                    values->limits.active_subbands,
                                    values->reconstructed, NULL) &&
              matches(values->reconstructed, values->spectrum, frame, channel,
                      "Post-alias") &&
              matches(fixture->overlap[channel].values, values->previous, frame,
                      channel, "Preceding overlap") &&
              ab_mp3_hybrid_channel(
                  &fixture->coefficients, &fixture->windows,
                  info->side.channel + channel, values->limits.active_subbands,
                  values->reconstructed, fixture->overlap + channel, matrix, NULL) &&
              matches(matrix, values->matrix, frame, channel, "Hybrid matrix") &&
              matches(fixture->overlap[channel].values, values->following, frame,
                      channel, "Next overlap"));
    }
    return true;
}

static bool native_checks(FILE *file, AbSpan source) {
    AbMp3 stream;
    HybridFixture fixture;
    CHECK(initialize(&fixture) && ab_mp3_read(source, &stream, NULL));
    char line[160];
    CHECK(fgets(line, sizeof(line), file) && strcmp(line, "AB_MP3_HYBRID_V1\n") == 0);
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
              compare_frame(file, &fixture, &info, window, meta[1]));
        ++frames;
        granules += info.channels;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB MP3 hybrid reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf(
        "Compared %zu native frames, %zu hybrid granules, %zu matrix/overlap floats\n",
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
        fprintf(stderr, "Usage: ab_mp3_hybrid_tests [NATIVE_CORPUS MP3]\n");
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
