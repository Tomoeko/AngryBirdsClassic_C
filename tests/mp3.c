#include "ab/hash.h"
#include "ab/mp3.h"
#include "ab/zip.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "MP3 reader failed at %d: %s\n", __LINE__, #condition);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

static void big_word(uint8_t *bytes, uint32_t word) {
    for (size_t index = 0; index < 4; ++index) {
        bytes[index] = (uint8_t)(word >> ((3 - index) * 8));
    }
}

static bool edge_checks(void) {
    uint8_t bytes[1024] = {0};
    big_word(bytes, UINT32_C(0xfff318c0));
    big_word(bytes + 36, UINT32_C(0xfff318c0));
    AbMp3 output = {.sample_rate = 7};
    CHECK(!ab_mp3_read((AbSpan){0}, &output, NULL) && output.sample_rate == 7);
    CHECK(!ab_mp3_read((AbSpan){bytes, 72}, NULL, NULL));
    for (size_t size = 1; size < 72; ++size) {
        output.sample_rate = 7;
        bool accepted = ab_mp3_read((AbSpan){bytes, size}, &output, NULL);
        CHECK(accepted == (size == 36));
        CHECK(accepted || output.sample_rate == 7);
    }
    CHECK(ab_mp3_read((AbSpan){bytes, 72}, &output, NULL) && output.frame_count == 2 &&
          output.sample_frames == 1152 && output.sample_rate == 16000 &&
          output.channels == 1 && !output.has_xing);
    AbMp3FrameInfo frame = {.header = 7};
    CHECK(!ab_mp3_frame_info((AbSpan){NULL, 4}, &frame, NULL) && frame.header == 7 &&
          !ab_mp3_frame_info((AbSpan){bytes, 72}, NULL, NULL));
    for (size_t size = 0; size < 36; ++size) {
        CHECK(!ab_mp3_frame_info((AbSpan){bytes, size}, &frame, NULL) &&
              frame.header == 7);
    }
    CHECK(ab_mp3_frame_info((AbSpan){bytes, 72}, &frame, NULL) &&
          frame.frame.data == bytes && frame.frame.size == 36 &&
          frame.side_information.data == bytes + 4 &&
          frame.side_information.size == 9 && frame.main_data.data == bytes + 13 &&
          frame.main_data.size == 23 && frame.header == UINT32_C(0xfff318c0) &&
          frame.sample_rate == 16000 && frame.channels == 1 && !frame.has_crc &&
          !frame.crc);
    bytes[1] &= (uint8_t)~1;
    bytes[4] = 0xab;
    bytes[5] = 0xcd;
    CHECK(ab_mp3_frame_info((AbSpan){bytes, 72}, &frame, NULL) && frame.has_crc &&
          frame.crc == 0xabcd && frame.side_information.data == bytes + 6 &&
          frame.main_data.data == bytes + 15 && frame.main_data.size == 21);
    bytes[1] |= 1;
    bytes[4] = bytes[5] = 0;
    bytes[37] ^= 8;
    CHECK(!ab_mp3_read((AbSpan){bytes, 72}, &output, NULL));
    bytes[37] ^= 8;
    bytes[38] |= 4;
    CHECK(!ab_mp3_read((AbSpan){bytes, 72}, &output, NULL));
    bytes[38] &= (uint8_t)~4;
    bytes[39] = 0;
    CHECK(!ab_mp3_read((AbSpan){bytes, 72}, &output, NULL));
    bytes[39] = 0xc0;
    bytes[72] = 1;
    CHECK(!ab_mp3_read((AbSpan){bytes, 73}, &output, NULL));
    memcpy(bytes + 72, "TAG", 3);
    CHECK(ab_mp3_read((AbSpan){bytes, 200}, &output, NULL) &&
          output.trailing_tag.size == 128 && output.frames.size == 72);
    memmove(bytes + 10, bytes, 72);
    memset(bytes, 0, 10);
    memcpy(bytes, "ID3", 3);
    bytes[3] = 3;
    CHECK(ab_mp3_read((AbSpan){bytes, 82}, &output, NULL) &&
          output.leading_tag.size == 10 && output.frame_count == 2);
    bytes[6] = 128;
    CHECK(!ab_mp3_read((AbSpan){bytes, 82}, &output, NULL));
    bytes[6] = 0;
    bytes[9] = 127;
    CHECK(!ab_mp3_read((AbSpan){bytes, 82}, &output, NULL));
    bytes[9] = 0;
    bytes[5] = 1;
    CHECK(!ab_mp3_read((AbSpan){bytes, 82}, &output, NULL));
    memset(bytes, 0, sizeof(bytes));
    big_word(bytes, UINT32_C(0xfff388c0));
    big_word(bytes + 288, UINT32_C(0xfff388c0));
    memcpy(bytes + 13, "Xing", 4);
    big_word(bytes + 17, 1);
    big_word(bytes + 21, 1);
    memcpy(bytes + 25, "LAME3.98r", 9);
    bytes[46] = 6;
    bytes[47] = 0x40;
    bytes[48] = 20;
    CHECK(ab_mp3_read((AbSpan){bytes, 576}, &output, NULL) && output.has_xing &&
          output.has_lame && output.encoder_delay == 100 &&
          output.encoder_padding == 20 && output.sample_frames == 456);
    big_word(bytes + 21, 2);
    CHECK(!ab_mp3_read((AbSpan){bytes, 576}, &output, NULL));
    big_word(bytes + 21, 1);
    bytes[46] = 255;
    CHECK(!ab_mp3_read((AbSpan){bytes, 576}, &output, NULL));
    return true;
}

static bool decode_edges(void) {
    uint8_t bytes[72] = {0};
    big_word(bytes, UINT32_C(0xfff318c0));
    big_word(bytes + 36, UINT32_C(0xfff318c0));
    AbSpan input = {bytes, sizeof(bytes)};
    AbPcm16 pcm = {.frames = 123};
    CHECK(!ab_mp3_decode(input, 1151, &pcm, NULL) && pcm.frames == 123 &&
          !ab_mp3_decode(input, 1152, NULL, NULL) &&
          !ab_mp3_decode((AbSpan){NULL, 0}, 1152, &pcm, NULL));
    CHECK(ab_mp3_decode(input, 1152, &pcm, NULL) && pcm.frames == 1152 &&
          pcm.sample_rate == 16000 && pcm.channels == 1 && pcm.samples);
    for (size_t index = 0; index < pcm.frames; ++index) {
        CHECK(pcm.samples[index] == 0);
    }
    ab_pcm16_free(&pcm);
    CHECK(!pcm.samples && !pcm.frames && !pcm.channels && !pcm.sample_rate);
    ab_pcm16_free(&pcm);
    ab_pcm16_free(NULL);
    /* The second frame requests one more reservoir byte than the first
     * supplies. Reject after allocating/decoding without publishing ownership. */
    bytes[40] = 24;
    pcm.frames = 123;
    CHECK(!ab_mp3_decode(input, 1152, &pcm, NULL) && !pcm.samples && pcm.frames == 123);
    bytes[40] = 0;
    pcm.frames = 0;
    big_word(bytes, UINT32_C(0xfff31850));
    big_word(bytes + 36, UINT32_C(0xfff31850));
    CHECK(!ab_mp3_decode(input, 1152, &pcm, NULL) && !pcm.samples && !pcm.frames);
    return true;
}

static bool read_line(FILE *file, char *output, size_t capacity) {
    CHECK(capacity <= INT32_MAX && fgets(output, (int)capacity, file));
    size_t size = strlen(output);
    CHECK(size && output[size - 1] == '\n');
    output[size - 1] = 0;
    return true;
}

static bool native_checks(const AbZip *archive, FILE *file) {
    char line[256];
    CHECK(read_line(file, line, sizeof(line)) &&
          strcmp(line, "AB_MP3_FORMAT_REFERENCE_V1 347") == 0);
    size_t frames = 0, lame = 0, id3v2 = 0, id3v1 = 0;
    uint64_t samples = 0;
    for (size_t item = 0; item < 347; ++item) {
        char source_hash[66], pcm_hash[66];
        CHECK(read_line(file, line, sizeof(line)));
        const AbZipEntry *entry = ab_zip_find(archive, line);
        CHECK(entry && read_line(file, source_hash, sizeof(source_hash)));
        uint32_t expected[8];
        for (size_t word = 0; word < 8; ++word) {
            CHECK(fscanf(file, "%" SCNx32, expected + word) == 1 &&
                  fgetc(file) == (word == 7 ? '\n' : ' '));
        }
        CHECK(read_line(file, pcm_hash, sizeof(pcm_hash)) && strlen(pcm_hash) == 64);
        AbBuffer bytes = {0};
        AbError error = {0};
        CHECK(ab_zip_read(archive, (size_t)(entry - archive->entries), 512 * 1024,
                          &bytes, &error));
        AbMp3 stream = {0};
        uint8_t digest[32] = {0};
        char actual_hash[65];
        bool okay = ab_sha256((AbSpan){bytes.data, bytes.size}, digest, &error) &&
                    ab_mp3_read((AbSpan){bytes.data, bytes.size}, &stream, &error);
        ab_digest_hex(digest, actual_hash);
        if (!okay || strcmp(source_hash, actual_hash) != 0 ||
            bytes.size != expected[0] || stream.sample_rate != expected[1] ||
            stream.channels != expected[2] || expected[3] != 0xd0 ||
            stream.sample_frames != expected[4] || stream.frame_count != expected[5] ||
            stream.leading_tag.size != expected[6] ||
            stream.trailing_tag.size != expected[7]) {
            fprintf(stderr, "MP3 metadata differs at %s: %s\n", line, error.message);
            ab_buffer_free(&bytes);
            return false;
        }
        frames += stream.frame_count;
        AbPcm16 pcm = {0};
        if (!ab_mp3_decode((AbSpan){bytes.data, bytes.size}, expected[4], &pcm,
                           &error)) {
            fprintf(stderr, "MP3 PCM decode failed at %s: %s\n", line, error.message);
            ab_buffer_free(&bytes);
            return false;
        }
        CHECK(pcm.frames == expected[4] && pcm.sample_rate == expected[1] &&
              pcm.channels == expected[2]);
        size_t sample_count = pcm.frames * pcm.channels;
        uint8_t *encoded = (uint8_t *)pcm.samples;
        for (size_t index = 0; index < sample_count; ++index) {
            uint16_t value = (uint16_t)pcm.samples[index];
            encoded[index * 2] = (uint8_t)value;
            encoded[index * 2 + 1] = (uint8_t)(value >> 8);
        }
        okay = ab_sha256((AbSpan){encoded, sample_count * 2}, digest, &error);
        ab_digest_hex(digest, actual_hash);
        ab_pcm16_free(&pcm);
        if (!okay || strcmp(actual_hash, pcm_hash) != 0) {
            fprintf(stderr, "MP3 PCM hash differs at %s: %s != %s\n", line, actual_hash,
                    pcm_hash);
            ab_buffer_free(&bytes);
            return false;
        }
        samples += stream.sample_frames;
        lame += stream.has_lame;
        id3v2 += stream.leading_tag.size != 0;
        id3v1 += stream.trailing_tag.size != 0;
        ab_buffer_free(&bytes);
    }
    CHECK(read_line(file, line, sizeof(line)) &&
          strcmp(line, "AB MP3 format reference complete") == 0 && fgetc(file) == EOF &&
          !ferror(file));
    printf("All 347 MP3 formats, sample counts and complete PCM hashes match native "
           "decoding; %zu MPEG frames, %" PRIu64 " sample frames, %zu LAME, "
           "%zu ID3v2, %zu ID3v1.\n",
           frames, samples, lame, id3v2, id3v1);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() && decode_edges() ? 0 : 1;
    }
    if (argc != 3) {
        fprintf(stderr, "Usage: ab_mp3_tests [APK NATIVE_FORMAT_MANIFEST]\n");
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbError error = {0};
    FILE *file = fopen(argv[2], "rb");
    bool okay = file && ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                native_checks(&archive, file);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    if (file && fclose(file) != 0) {
        okay = false;
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
