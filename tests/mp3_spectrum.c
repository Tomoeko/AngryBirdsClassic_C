#include "ab/mp3.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "MP3 spectrum failed at %d: %s\n", __LINE__, #condition);  \
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

static bool compare_spectrum(const AbMp3FrameInfo *info, const uint32_t expected[576],
                             size_t frame) {
    int16_t actual[576];
    CHECK(ab_mp3_quantized_spectrum(info->side.channel, info->sample_rate,
                                    info->main_data, 0, 0, actual, NULL));
    for (size_t index = 0; index < 576; ++index) {
        if ((uint32_t)(int32_t)actual[index] != expected[index]) {
            fprintf(stderr, "Native frame %zu value %zu: %d != %" PRIx32 "\n", frame,
                    index, (int)actual[index], expected[index]);
            return false;
        }
    }
    uint8_t shifted[1024] = {0};
    CHECK(info->main_data.size < sizeof(shifted));
    memcpy(shifted + 1, info->main_data.data, info->main_data.size);
    AbMp3Granule granule = info->side.channel[0];
    granule.part23_length += 5;
    CHECK(ab_mp3_quantized_spectrum(&granule, info->sample_rate,
                                    (AbSpan){shifted, info->main_data.size + 1}, 3, 5,
                                    actual, NULL));
    for (size_t index = 0; index < 576; ++index) {
        CHECK((uint32_t)(int32_t)actual[index] == expected[index]);
    }
    return true;
}

static bool native_checks(FILE *file, AbSpan source) {
    AbMp3 stream;
    CHECK(ab_mp3_read(source, &stream, NULL) && !stream.has_xing);
    char line[128];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_MP3_SPECTRUM_V1\n") == 0);
    size_t frames = 0, offset = 0;
    for (;;) {
        uint32_t header[5];
        CHECK(fgets(line, sizeof(line), file));
        int count =
            sscanf(line, "%" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32 " %" SCNx32,
                   header, header + 1, header + 2, header + 3, header + 4);
        if (count == 1) {
            CHECK(header[0] == frames && frames == stream.frame_count);
            break;
        }
        CHECK(count == 5 && header[2] == 1 && header[3] == 9 && header[4] == frames);
        uint32_t raw[9], common[2], observation[1155], fields[16];
        CHECK(words(file, raw, 9) && words(file, common, 2) &&
              words(file, observation, 3));
        for (size_t index = 3; index < 1155; index += 192) {
            CHECK(words(file, observation + index, 192));
        }
        CHECK(words(file, fields, 16));
        AbMp3FrameInfo info;
        CHECK(offset < stream.frames.size &&
              ab_mp3_frame_info(
                  (AbSpan){stream.frames.data + offset, stream.frames.size - offset},
                  &info, NULL));
        const AbMp3Granule *granule = info.side.channel;
        uint16_t regions[2];
        CHECK(info.header == header[0] && info.frame.size - 4 == header[1] &&
              info.channels == 1 && info.side_information.size == 9 &&
              !info.side.main_data_begin && !common[0] && !common[1] &&
              !granule->scalefactor_compress && granule->global_gain == 210 &&
              granule->part23_length == fields[0] && granule->big_values == fields[1] &&
              ab_mp3_huffman_regions(granule, info.sample_rate, regions, NULL) &&
              regions[0] == fields[8] && regions[1] == fields[9] &&
              observation[0] == 0 && observation[1] == granule->part23_length &&
              observation[2] == UINT32_C(0x3f800000));
        for (size_t index = 0; index < 9; ++index) {
            CHECK(raw[index] == info.side_information.data[index]);
        }
        CHECK(compare_spectrum(&info, observation + 579, frames));
        offset += info.frame.size;
        ++frames;
    }
    CHECK(offset == stream.frames.size && fgets(line, sizeof(line), file) &&
          strcmp(line, "AB MP3 spectrum reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared %zu native frames and %zu quantized spectral values\n", frames,
           frames * 576);
    return true;
}

static bool edge_checks(void) {
    AbMp3Granule granule = {0};
    uint16_t regions[2] = {13, 17};
    CHECK(ab_mp3_huffman_regions(&granule, 16000, regions, NULL) && regions[0] == 3 &&
          regions[1] == 6);
    granule.region_count[0] = 15;
    granule.region_count[1] = 7;
    CHECK(ab_mp3_huffman_regions(&granule, 24000, regions, NULL) && regions[0] == 116 &&
          regions[1] == 288);
    granule.window_switch = true;
    granule.block_type = 1;
    CHECK(ab_mp3_huffman_regions(&granule, 22050, regions, NULL) && regions[0] == 27 &&
          regions[1] == 288);
    granule.block_type = 2;
    CHECK(ab_mp3_huffman_regions(&granule, 24000, regions, NULL) && regions[0] == 18 &&
          regions[1] == 288);
    CHECK(!ab_mp3_huffman_regions(&granule, 48000, regions, NULL) &&
          !ab_mp3_huffman_regions(NULL, 16000, regions, NULL) &&
          !ab_mp3_huffman_regions(&granule, 16000, NULL, NULL) && regions[0] == 18);
    granule = (AbMp3Granule){0};
    int16_t values[576], prior[576];
    memset(values, 0x35, sizeof(values));
    CHECK(ab_mp3_quantized_spectrum(&granule, 16000, (AbSpan){0}, 0, 0, values, NULL));
    for (size_t index = 0; index < 576; ++index) {
        CHECK(!values[index]);
    }
    uint8_t bytes[] = {5};
    granule.part23_length = 5;
    granule.count1_table = true;
    CHECK(ab_mp3_quantized_spectrum(&granule, 16000, (AbSpan){bytes, 1}, 0, 0, values,
                                    NULL) &&
          values[0] == 1 && !values[1]);
    granule.part23_length = 8;
    granule.window_switch = true;
    granule.block_type = 2;
    CHECK(ab_mp3_quantized_spectrum(&granule, 16000, (AbSpan){bytes, 1}, 0, 0, values,
                                    NULL) &&
          values[0] == 1 && values[3] == -1 && values[6] == 1 && values[9] == -1);
    memcpy(prior, values, sizeof(values));
    granule.mixed_block = true;
    CHECK(!ab_mp3_quantized_spectrum(&granule, 16000, (AbSpan){bytes, 1}, 0, 0, values,
                                     NULL) &&
          memcmp(prior, values, sizeof(values)) == 0);
    granule.mixed_block = false;
    granule.big_values = 1;
    granule.table_select[0] = 1;
    granule.part23_length = 0;
    CHECK(!ab_mp3_quantized_spectrum(&granule, 16000, (AbSpan){0}, 0, 0, values, NULL));
    granule.big_values = 289;
    CHECK(!ab_mp3_quantized_spectrum(&granule, 16000, (AbSpan){bytes, 1}, 0, 0, values,
                                     NULL));
    granule.big_values = 0;
    granule.table_select[0] = 32;
    CHECK(!ab_mp3_quantized_spectrum(&granule, 16000, (AbSpan){bytes, 1}, 0, 0, values,
                                     NULL));
    granule.table_select[0] = 1;
    CHECK(!ab_mp3_quantized_spectrum(&granule, 16000, (AbSpan){bytes, 1025}, 0, 0,
                                     values, NULL) &&
          !ab_mp3_quantized_spectrum(&granule, 16000, (AbSpan){NULL, 1}, 0, 0, values,
                                     NULL) &&
          !ab_mp3_quantized_spectrum(&granule, 16000, (AbSpan){bytes, 1}, 9, 0, values,
                                     NULL) &&
          !ab_mp3_quantized_spectrum(&granule, 16000, (AbSpan){bytes, 1}, 0, 1, values,
                                     NULL) &&
          !ab_mp3_quantized_spectrum(&granule, 16000, (AbSpan){bytes, 1}, 0, 0, NULL,
                                     NULL) &&
          memcmp(prior, values, sizeof(values)) == 0);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 3) {
        fprintf(stderr, "Usage: ab_mp3_spectrum_tests [NATIVE_CORPUS MP3]\n");
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
