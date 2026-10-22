#include "ab/mp3.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "MP3 Huffman failed at %d: %s\n", __LINE__, #condition);   \
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

static bool decode(AbMp3HuffmanBits *bits, const AbMp3Granule *granule,
                   int16_t output[4]) {
    if (granule->big_values) {
        if (!ab_mp3_huffman_pair(bits, granule->table_select[0], output, NULL)) {
            return false;
        }
        output[2] = output[3] = 0;
        return true;
    }
    return ab_mp3_huffman_quad(bits, granule->count1_table, output, NULL);
}

static bool compare_code(AbSpan payload, const AbMp3Granule *granule,
                         const uint32_t expected[4]) {
    /* Move each native packet through all bit alignments without changing its
     * codeword, extensions or signs. Truncations must publish nothing. */
    for (size_t alignment = 0; alignment < 8; ++alignment) {
        uint8_t shifted[32] = {0};
        for (size_t bit = 0; bit < granule->part23_length; ++bit) {
            unsigned value = (unsigned)payload.data[bit / 8] >> (7 - bit % 8) & 1u;
            shifted[(alignment + bit) / 8] |=
                (uint8_t)(value << (7 - (alignment + bit) % 8));
        }
        AbMp3HuffmanBits bits = {
            {shifted, sizeof(shifted)}, alignment, alignment + granule->part23_length};
        int16_t actual[4] = {0};
        CHECK(decode(&bits, granule, actual) && bits.position == bits.limit);
        for (size_t index = 0; index < 4; ++index) {
            CHECK((uint32_t)(int32_t)actual[index] == expected[index]);
        }
        if (alignment) {
            continue;
        }
        for (size_t limit = 0; limit < granule->part23_length; ++limit) {
            bits.position = 0;
            bits.limit = limit;
            int16_t prior[4] = {1234, -2345, 3456, -4567};
            memcpy(actual, prior, sizeof(prior));
            CHECK(!decode(&bits, granule, actual) && !bits.position &&
                  bits.limit == limit && memcmp(actual, prior, sizeof(prior)) == 0);
        }
    }
    return true;
}

static bool native_checks(FILE *file, AbSpan source) {
    AbMp3 stream;
    CHECK(ab_mp3_read(source, &stream, NULL) && !stream.has_xing);
    char line[128];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_MP3_HUFFMAN_V1\n") == 0);
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
        uint32_t raw[9], common[2], observation[11], fields[16];
        CHECK(words(file, raw, 9) && words(file, common, 2) &&
              words(file, observation, 11) && words(file, fields, 16));
        AbMp3FrameInfo info;
        CHECK(offset < stream.frames.size &&
              ab_mp3_frame_info(
                  (AbSpan){stream.frames.data + offset, stream.frames.size - offset},
                  &info, NULL));
        const AbMp3Granule *granule = info.side.channel;
        CHECK(info.header == header[0] && info.frame.size - 4 == header[1] &&
              info.channels == 1 && info.side_information.size == 9 &&
              !info.side.main_data_begin && !common[0] && !common[1] &&
              !granule->block_type && !granule->scalefactor_compress &&
              granule->global_gain == 210 && granule->big_values <= 1 &&
              granule->part23_length <= info.main_data.size * 8 &&
              granule->part23_length == fields[0] && granule->big_values == fields[1] &&
              granule->table_select[0] == fields[5] &&
              granule->count1_table == (fields[11] != 0) && observation[0] == 0 &&
              observation[1] == granule->part23_length &&
              observation[2] == UINT32_C(0x3f800000));
        for (size_t index = 0; index < 9; ++index) {
            CHECK(raw[index] == info.side_information.data[index]);
        }
        CHECK(compare_code(info.main_data, granule, observation + 7));
        for (size_t index = 0; index < 4; ++index) {
            uint32_t signed_value = observation[7 + index];
            bool negative = (signed_value & UINT32_C(0x80000000)) != 0;
            uint32_t magnitude = negative ? 0u - signed_value : signed_value;
            float power = 0;
            CHECK(magnitude <= 8206 &&
                  ab_mp3_magnitude_power((uint16_t)magnitude, &power, NULL));
            uint32_t bits;
            memcpy(&bits, &power, sizeof(bits));
            bits |= negative ? UINT32_C(0x80000000) : 0;
            CHECK(bits == observation[3 + index]);
        }
        offset += info.frame.size;
        ++frames;
    }
    CHECK(offset == stream.frames.size && fgets(line, sizeof(line), file) &&
          strcmp(line, "AB MP3 Huffman reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared %zu native Huffman packets, alignments and truncations\n", frames);
    return true;
}

static bool edge_checks(void) {
    const uint16_t magnitudes[] = {0, 1, 8, 27, 64};
    const float powers[] = {0, 1, 16, 81, 256};
    float power = -123.0f;
    CHECK(!ab_mp3_magnitude_power(8207, &power, NULL) && power == -123.0f &&
          !ab_mp3_magnitude_power(1, NULL, NULL));
    for (size_t index = 0; index < 5; ++index) {
        CHECK(ab_mp3_magnitude_power(magnitudes[index], &power, NULL) &&
              power == powers[index]);
    }
    AbMp3HuffmanBits bits = {0};
    int16_t values[4] = {7, 8, 9, 10};
    const uint32_t empty_tables[] = {0, 4, 14};
    for (size_t index = 0; index < 3; ++index) {
        CHECK(ab_mp3_huffman_pair(&bits, empty_tables[index], values, NULL) &&
              !values[0] && !values[1] && !bits.position);
    }
    const uint8_t pair[] = {8};
    bits = (AbMp3HuffmanBits){{pair, sizeof(pair)}, 0, 5};
    CHECK(ab_mp3_huffman_pair(&bits, 1, values, NULL) && values[0] == 1 &&
          values[1] == -1 && bits.position == 5);
    const uint8_t quad[] = {5};
    bits = (AbMp3HuffmanBits){{quad, sizeof(quad)}, 0, 8};
    CHECK(ab_mp3_huffman_quad(&bits, true, values, NULL) && values[0] == 1 &&
          values[1] == -1 && values[2] == 1 && values[3] == -1 && bits.position == 8);
    int16_t prior[4];
    memcpy(prior, values, sizeof(values));
    bits.position = 0;
    bits.limit = 7;
    CHECK(!ab_mp3_huffman_quad(&bits, true, values, NULL) && !bits.position &&
          memcmp(prior, values, sizeof(values)) == 0);
    CHECK(!ab_mp3_huffman_pair(&bits, 32, values, NULL) &&
          !ab_mp3_huffman_pair(NULL, 0, values, NULL) &&
          !ab_mp3_huffman_pair(&bits, 0, NULL, NULL) &&
          !ab_mp3_huffman_quad(NULL, false, values, NULL) &&
          !ab_mp3_huffman_quad(&bits, false, NULL, NULL));
    bits.input = (AbSpan){NULL, 1};
    CHECK(!ab_mp3_huffman_pair(&bits, 0, values, NULL));
    bits.input = (AbSpan){pair, 1025};
    CHECK(!ab_mp3_huffman_pair(&bits, 0, values, NULL));
    bits.input = (AbSpan){pair, 1};
    bits.limit = 9;
    CHECK(!ab_mp3_huffman_quad(&bits, false, values, NULL));
    bits.limit = 0;
    bits.position = 1;
    CHECK(!ab_mp3_huffman_pair(&bits, 0, values, NULL) &&
          memcmp(prior, values, sizeof(values)) == 0);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 3) {
        fprintf(stderr, "Usage: ab_mp3_huffman_tests [NATIVE_CORPUS MP3]\n");
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
