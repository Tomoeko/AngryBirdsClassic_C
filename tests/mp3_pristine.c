#include "ab/mp3.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Pristine MP3 failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare_channel(const AbMp3FrameInfo *info, AbSpan window,
                            const uint32_t meta[9], const uint32_t expected_scales[39],
                            const uint32_t state[5], const uint32_t expected[576]) {
    size_t channel = meta[2], part_offset = 0;
    for (size_t previous = 0; previous < channel; ++previous) {
        part_offset += info->side.channel[previous].part23_length;
    }
    const AbMp3Granule *granule = info->side.channel + channel;
    bool joint = info->channels == 2 && ((info->header >> 6) & 3) == 1;
    bool intensity = channel == 1 && joint && (info->header & 16) != 0;
    bool mid_side = joint && (info->header & 32) != 0;
    AbMp3Scalefactors scales;
    CHECK(ab_mp3_scalefactors(granule, intensity, window, part_offset, &scales, NULL) &&
          scales.bit_count == meta[4] && part_offset + scales.bit_count == meta[5] &&
          scales.count == meta[7] && scales.preflag == (meta[8] != 0));
    for (size_t index = 0; index < scales.count; ++index) {
        CHECK(scales.values[index] == expected_scales[index]);
    }
    uint16_t regions[2];
    CHECK(ab_mp3_huffman_regions(granule, info->sample_rate, regions, NULL) &&
          state[0] == 0 && state[1] == part_offset + granule->part23_length &&
          state[2] == regions[0] && state[3] == regions[1] &&
          state[4] ==
              UINT32_C(0x2ea8) - 4u * granule->global_gain + (mid_side ? 8u : 0u));
    int16_t quantized[576];
    float reconstructed[576];
    CHECK(ab_mp3_quantized_spectrum(granule, info->sample_rate, window, part_offset,
                                    scales.bit_count, quantized, NULL) &&
          ab_mp3_requantize_spectrum(granule, &scales, info->sample_rate, mid_side,
                                     quantized, reconstructed, NULL));
    for (size_t index = 0; index < 576; ++index) {
        uint32_t actual;
        memcpy(&actual, reconstructed + index, sizeof(actual));
        if (actual != expected[index]) {
            fprintf(stderr,
                    "Pristine frame %" PRIu32 " channel %zu value %zu: "
                    "%08" PRIx32 " != %08" PRIx32 "\n",
                    meta[1], channel, index, actual, expected[index]);
            return false;
        }
    }
    return true;
}

static bool native_checks(FILE *file, AbSpan source) {
    AbMp3 stream;
    CHECK(ab_mp3_read(source, &stream, NULL));
    char line[160];
    CHECK(fgets(line, sizeof(line), file) && strcmp(line, "AB_MP3_PRISTINE_V1\n") == 0);
    AbMp3Reservoir reservoir = {0};
    uint8_t joined[1024];
    AbSpan window = {0};
    AbMp3FrameInfo info = {0};
    size_t offset = 0, source_index = 0, calls = 0, current_frame = SIZE_MAX;
    uint32_t next_channel = 0;
    for (;;) {
        uint32_t meta[9];
        CHECK(fgets(line, sizeof(line), file));
        int count = sscanf(line,
                           "%" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32
                           " %" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32,
                           meta, meta + 1, meta + 2, meta + 3, meta + 4, meta + 5,
                           meta + 6, meta + 7, meta + 8);
        if (count == 1) {
            CHECK(meta[0] == calls && calls && next_channel == 0);
            break;
        }
        CHECK(count == 9 && meta[6] <= 1024 && meta[7] <= 39 && meta[8] <= 1 &&
              meta[2] == next_channel);
        size_t target = meta[1] + (stream.has_xing ? 1u : 0u);
        CHECK(target < stream.frame_count &&
              (next_channel == 0 ? current_frame == SIZE_MAX || target > current_frame
                                 : target == current_frame));
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
        CHECK(info.header == meta[0] && window.size == meta[6] &&
              meta[2] < info.channels &&
              meta[3] == (info.sample_rate == 16000   ? 5u
                          : info.sample_rate == 22050 ? 3u
                                                      : 4u));
        uint32_t body_side[17], packed[256], scales[39], state[5], expected[576];
        CHECK(words(file, body_side, info.side_information.size) &&
              words(file, packed, (window.size + 3) / 4) &&
              words(file, scales, meta[7]) && words(file, state, 5));
        for (size_t index = 0; index < 576; index += 192) {
            CHECK(words(file, expected + index, 192));
        }
        for (size_t index = 0; index < window.size; ++index) {
            CHECK((packed[index / 4] >> ((index % 4) * 8) & 255u) ==
                  window.data[index]);
        }
        /* Native set_pointer can overwrite the side-information slice while
         * joining its preceding reservoir tail. This is a post-join body read. */
        for (size_t index = 0; index < info.side_information.size; ++index) {
            int joined_index = (int)index + info.side.main_data_begin -
                               (int)info.side_information.size;
            uint8_t expected_byte = joined_index >= 0
                                        ? window.data[joined_index]
                                        : info.side_information.data[index];
            CHECK(body_side[index] == expected_byte);
        }
        CHECK(compare_channel(&info, window, meta, scales, state, expected));
        current_frame = target;
        next_channel = meta[2] + 1 == info.channels ? 0 : meta[2] + 1;
        ++calls;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB MP3 pristine reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared %zu pristine native granules and %zu spectral float values\n",
           calls, calls * 576);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: ab_mp3_pristine_tests NATIVE_CORPUS MP3\n");
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
