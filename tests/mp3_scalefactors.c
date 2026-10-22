#include "ab/mp3.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "MP3 scalefactors failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
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

static bool native_checks(FILE *file, AbSpan source) {
    AbMp3 stream;
    CHECK(ab_mp3_read(source, &stream, NULL));
    char line[128];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_MP3_SCALEFACTORS_V1\n") == 0);
    AbMp3Reservoir reservoir = {0};
    size_t offset = 0, source_index = 0, frames = 0, values = 0;
    uint8_t output[1024];
    for (;;) {
        uint32_t header[5];
        CHECK(fgets(line, sizeof(line), file));
        int count =
            sscanf(line, "%" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32,
                   header, header + 1, header + 2, header + 3, header + 4);
        if (count == 1) {
            CHECK(header[0] == frames);
            break;
        }
        CHECK(count == 5 && (header[2] == 1 || header[2] == 2) &&
              header[3] == (header[2] == 1 ? 9u : 17u) &&
              header[4] < stream.frame_count);
        size_t target = header[4] + (stream.has_xing ? 1u : 0u);
        CHECK(target < stream.frame_count && source_index <= target);
        uint32_t raw[17], common[2], ignored[16], scale_info[2][2], expected[2][39];
        CHECK(words(file, raw, header[3]) && words(file, common, 2));
        for (size_t channel = 0; channel < header[2]; ++channel) {
            CHECK(words(file, ignored, 16) && words(file, scale_info[channel], 2) &&
                  scale_info[channel][0] <= 39 && scale_info[channel][1] <= 1 &&
                  words(file, expected[channel], scale_info[channel][0]));
        }
        AbSpan window = {0};
        AbMp3FrameInfo info = {0};
        while (source_index <= target) {
            CHECK(offset < stream.frames.size &&
                  ab_mp3_frame_info((AbSpan){stream.frames.data + offset,
                                             stream.frames.size - offset},
                                    &info, NULL) &&
                  ab_mp3_reservoir_push(&reservoir, &info, output, sizeof(output),
                                        &window, NULL));
            offset += info.frame.size;
            ++source_index;
        }
        CHECK(info.header == header[0] && info.frame.size - 4 == header[1] &&
              info.channels == header[2] && info.side_information.size == header[3] &&
              info.side.main_data_begin == common[0] &&
              info.side.private_bits == common[1]);
        size_t bit_offset = 0;
        for (size_t channel = 0; channel < info.channels; ++channel) {
            bool intensity = channel == 1 && ((info.header >> 6) & 3) == 1 &&
                             (info.header & 16) != 0;
            AbMp3Scalefactors scales;
            CHECK(ab_mp3_scalefactors(info.side.channel + channel, intensity, window,
                                      bit_offset, &scales, NULL) &&
                  scales.count == scale_info[channel][0] &&
                  scales.preflag == (scale_info[channel][1] != 0));
            for (size_t index = 0; index < scales.count; ++index) {
                if (scales.values[index] != expected[channel][index]) {
                    fprintf(stderr,
                            "Native MP3 frame %" PRIu32 " channel %zu scalefactor %zu: "
                            "%u != %" PRIu32 "\n",
                            header[4], channel, index, scales.values[index],
                            expected[channel][index]);
                    return false;
                }
                ++values;
            }
            bit_offset += info.side.channel[channel].part23_length;
        }
        ++frames;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB MP3 scalefactor reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared %zu native MPEG-2 frames and %zu scalefactors\n", frames, values);
    return true;
}

static bool edge_checks(void) {
    AbMp3Granule granule = {0};
    AbMp3Scalefactors scales;
    CHECK(ab_mp3_scalefactors(&granule, false, (AbSpan){0}, 0, &scales, NULL) &&
          scales.count == 22 && scales.data_count == 21 && !scales.bit_count &&
          !scales.preflag);
    for (size_t index = 0; index < sizeof(scales.values); ++index) {
        CHECK(!scales.values[index]);
    }
    uint8_t bytes[12];
    memset(bytes, 255, sizeof(bytes));
    granule.scalefactor_compress = 511;
    granule.part23_length = 53;
    CHECK(ab_mp3_scalefactors(&granule, false, (AbSpan){bytes, sizeof(bytes)}, 0,
                              &scales, NULL) &&
          scales.count == 22 && scales.data_count == 21 && scales.bit_count == 53 &&
          scales.preflag && scales.values[0] == 7 && scales.values[10] == 7 &&
          scales.values[11] == 3 && scales.values[20] == 3 && !scales.values[21]);
    CHECK(ab_mp3_scalefactors(&granule, true, (AbSpan){bytes, sizeof(bytes)}, 0,
                              &scales, NULL) &&
          scales.bit_count == 40 && !scales.preflag && scales.values[7] == 7 &&
          scales.values[8] == 3 && scales.values[15] == 3 && !scales.values[16]);
    granule.block_type = 2;
    granule.mixed_block = true;
    granule.part23_length = 81;
    CHECK(ab_mp3_scalefactors(&granule, false, (AbSpan){bytes, sizeof(bytes)}, 0,
                              &scales, NULL) &&
          scales.count == 38 && scales.data_count == 33 && scales.bit_count == 81 &&
          scales.values[14] == 7 && scales.values[15] == 3 && scales.values[32] == 3 &&
          !scales.values[33] && !scales.values[37]);
    granule.mixed_block = false;
    granule.part23_length = 90;
    CHECK(ab_mp3_scalefactors(&granule, false, (AbSpan){bytes, sizeof(bytes)}, 3,
                              &scales, NULL) &&
          scales.count == 39 && scales.data_count == 36 && scales.bit_count == 90 &&
          scales.values[17] == 7 && scales.values[18] == 3 && scales.values[35] == 3 &&
          !scales.values[36] && !scales.values[38]);
    AbMp3Scalefactors prior = scales;
    granule.part23_length = 89;
    CHECK(!ab_mp3_scalefactors(&granule, false, (AbSpan){bytes, sizeof(bytes)}, 0,
                               &scales, NULL) &&
          memcmp(&scales, &prior, sizeof(scales)) == 0);
    granule.part23_length = 90;
    CHECK(
        !ab_mp3_scalefactors(&granule, false, (AbSpan){bytes, 11}, 0, &scales, NULL) &&
        !ab_mp3_scalefactors(&granule, false, (AbSpan){bytes, sizeof(bytes)}, SIZE_MAX,
                             &scales, NULL) &&
        !ab_mp3_scalefactors(NULL, false, (AbSpan){0}, 0, &scales, NULL) &&
        !ab_mp3_scalefactors(&granule, false, (AbSpan){0}, 0, NULL, NULL) &&
        memcmp(&scales, &prior, sizeof(scales)) == 0);
    granule.scalefactor_compress = 512;
    CHECK(!ab_mp3_scalefactors(&granule, false, (AbSpan){bytes, sizeof(bytes)}, 0,
                               &scales, NULL));
    granule.scalefactor_compress = 0;
    granule.block_type = 4;
    CHECK(!ab_mp3_scalefactors(&granule, false, (AbSpan){bytes, sizeof(bytes)}, 0,
                               &scales, NULL));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 3) {
        fprintf(stderr, "Usage: ab_mp3_scalefactor_tests [NATIVE_CORPUS MP3]\n");
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
