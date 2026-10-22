#include "ab/mp3.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "MP3 requantization failed at %d: %s\n", __LINE__,         \
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

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool compare_frame(const AbMp3FrameInfo *info, const uint32_t control[41],
                          const uint32_t expected[579], size_t frame) {
    CHECK(control[0] <= 1 && control[1] <= 1);
    bool short_block = info->side.channel[0].block_type == 2;
    AbMp3Scalefactors scales = {.count = short_block ? 39 : 22,
                                .data_count = short_block ? 36 : 21,
                                .preflag = control[0] != 0};
    for (size_t index = 0; index < 39; ++index) {
        CHECK(control[2 + index] <= 15);
        scales.values[index] = (uint8_t)control[2 + index];
    }
    int16_t quantized[576];
    float actual[576], global_gain;
    CHECK(ab_mp3_quantized_spectrum(info->side.channel, info->sample_rate,
                                    info->main_data, 0, 0, quantized, NULL) &&
          ab_mp3_requantize_spectrum(info->side.channel, &scales, info->sample_rate,
                                     control[1] != 0, quantized, actual, NULL) &&
          ab_mp3_gain_power(210 - info->side.channel[0].global_gain +
                                (control[1] ? 2 : 0),
                            &global_gain, NULL) &&
          float_bits(global_gain) == expected[2]);
    for (size_t index = 0; index < 576; ++index) {
        uint32_t bits = float_bits(actual[index]);
        if (bits != expected[3 + index]) {
            fprintf(stderr,
                    "Native requantized frame %zu value %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    frame, index, bits, expected[3 + index]);
            return false;
        }
    }
    return true;
}

static bool native_checks(FILE *file, AbSpan source) {
    AbMp3 stream;
    CHECK(ab_mp3_read(source, &stream, NULL) && !stream.has_xing);
    char line[128];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_MP3_REQUANTIZE_V1\n") == 0);
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
        uint32_t raw[9], common[2], control[41], observation[579], fields[16];
        CHECK(words(file, raw, 9) && words(file, common, 2) &&
              words(file, control, 41) && words(file, observation, 3));
        for (size_t index = 3; index < 579; index += 192) {
            CHECK(words(file, observation + index, 192));
        }
        CHECK(words(file, fields, 16));
        AbMp3FrameInfo info;
        CHECK(offset < stream.frames.size &&
              ab_mp3_frame_info(
                  (AbSpan){stream.frames.data + offset, stream.frames.size - offset},
                  &info, NULL) &&
              info.header == header[0] && info.frame.size - 4 == header[1] &&
              info.channels == 1 && !info.side.main_data_begin && !common[0] &&
              !common[1] && !info.side.channel[0].scalefactor_compress &&
              observation[0] == 0 &&
              observation[1] == info.side.channel[0].part23_length);
        for (size_t index = 0; index < 9; ++index) {
            CHECK(raw[index] == info.side_information.data[index]);
        }
        CHECK(compare_frame(&info, control, observation, frames));
        offset += info.frame.size;
        ++frames;
    }
    CHECK(offset == stream.frames.size && fgets(line, sizeof(line), file) &&
          strcmp(line, "AB MP3 requantization reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared %zu native frames and %zu requantized float values\n", frames,
           frames * 576);
    return true;
}

static bool edge_checks(void) {
    AbMp3Granule granule = {.global_gain = 210};
    AbMp3Scalefactors scales = {.count = 22, .data_count = 21};
    int16_t quantized[576] = {1, -8};
    quantized[575] = 27;
    float output[576], prior[576];
    CHECK(ab_mp3_requantize_spectrum(&granule, &scales, 16000, false, quantized, output,
                                     NULL) &&
          output[0] == 1 && output[1] == -16 && output[575] == 81);
    scales.values[0] = 1;
    granule.scalefactor_scale = true;
    CHECK(ab_mp3_requantize_spectrum(&granule, &scales, 24000, false, quantized, output,
                                     NULL) &&
          output[0] == 0.5f && output[1] == -8 && output[575] == 81);
    granule.window_switch = true;
    granule.block_type = 2;
    granule.scalefactor_scale = false;
    granule.subblock_gain[1] = 1;
    granule.subblock_gain[2] = 2;
    scales = (AbMp3Scalefactors){.count = 39, .data_count = 36};
    scales.values[0] = 1;
    scales.values[1] = 2;
    scales.values[2] = 3;
    quantized[0] = quantized[1] = quantized[2] = 1;
    CHECK(ab_mp3_requantize_spectrum(&granule, &scales, 22050, true, quantized, output,
                                     NULL));
    for (size_t window = 0; window < 3; ++window) {
        float expected;
        CHECK(ab_mp3_gain_power(4 + (int)window * 10, &expected, NULL) &&
              float_bits(output[window]) == float_bits(expected));
    }
    memcpy(prior, output, sizeof(output));
    granule.mixed_block = true;
    CHECK(!ab_mp3_requantize_spectrum(&granule, &scales, 22050, false, quantized,
                                      output, NULL));
    granule.mixed_block = false;
    scales.values[38] = 1;
    CHECK(!ab_mp3_requantize_spectrum(&granule, &scales, 22050, false, quantized,
                                      output, NULL));
    scales.values[38] = 0;
    scales.values[0] = 16;
    CHECK(!ab_mp3_requantize_spectrum(&granule, &scales, 22050, false, quantized,
                                      output, NULL));
    scales.values[0] = 1;
    quantized[0] = -8207;
    CHECK(!ab_mp3_requantize_spectrum(&granule, &scales, 22050, false, quantized,
                                      output, NULL));
    quantized[0] = 1;
    CHECK(!ab_mp3_requantize_spectrum(NULL, &scales, 22050, false, quantized, output,
                                      NULL) &&
          !ab_mp3_requantize_spectrum(&granule, NULL, 22050, false, quantized, output,
                                      NULL) &&
          !ab_mp3_requantize_spectrum(&granule, &scales, 22050, false, NULL, output,
                                      NULL) &&
          !ab_mp3_requantize_spectrum(&granule, &scales, 22050, false, quantized, NULL,
                                      NULL) &&
          memcmp(prior, output, sizeof(output)) == 0);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 3) {
        fprintf(stderr, "Usage: ab_mp3_requantize_tests [NATIVE_CORPUS MP3]\n");
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
