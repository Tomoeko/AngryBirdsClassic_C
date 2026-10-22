#include "ab/mp3.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "MP3 reservoir failed at %d: %s\n", __LINE__, #condition); \
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
          strcmp(line, "AB_ANDROID_MP3_RESERVOIR_V1\n") == 0);
    AbMp3Reservoir reservoir = {0};
    size_t offset = 0, source_index = 0, frames = 0, compared_bytes = 0;
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
        uint32_t raw[17], common[2], ignored[16];
        CHECK(words(file, raw, header[3]) && words(file, common, 2));
        for (size_t channel = 0; channel < header[2]; ++channel) {
            CHECK(words(file, ignored, 16));
        }
        uint32_t window_size, packed[256];
        CHECK(words(file, &window_size, 1) && window_size > 0 && window_size <= 1024 &&
              words(file, packed, (window_size + 3u) / 4u));
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
              info.side.private_bits == common[1] && window.size == window_size);
        for (size_t index = 0; index < info.side_information.size; ++index) {
            CHECK(raw[index] == info.side_information.data[index]);
        }
        for (size_t index = 0; index < window.size; ++index) {
            uint8_t expected = (uint8_t)(packed[index / 4] >> ((index % 4) * 8));
            if (window.data[index] != expected) {
                fprintf(stderr,
                        "Native MP3 frame %" PRIu32 " reservoir byte %zu differs\n",
                        header[4], index);
                return false;
            }
        }
        compared_bytes += window.size;
        ++frames;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB MP3 reservoir reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Compared %zu native MPEG-2 reservoir windows and %zu bytes\n", frames,
           compared_bytes);
    return true;
}

static bool edge_checks(void) {
    uint8_t payload[300], output[600];
    for (size_t index = 0; index < sizeof(payload); ++index) {
        payload[index] = (uint8_t)(index * 7u + 3u);
    }
    AbMp3Reservoir state = {0};
    AbMp3FrameInfo frame = {0};
    AbSpan window = {payload, 9};
    CHECK(ab_mp3_reservoir_push(&state, &frame, NULL, 0, &window, NULL) &&
          !window.data && !window.size && !state.size);
    frame.main_data = (AbSpan){payload, sizeof(payload)};
    CHECK(
        ab_mp3_reservoir_push(&state, &frame, output, sizeof(output), &window, NULL) &&
        window.size == 300 && memcmp(output, payload, 300) == 0 && state.size == 255 &&
        memcmp(state.bytes, payload + 45, 255) == 0);
    frame.main_data.size = 3;
    frame.side.main_data_begin = 255;
    AbMp3Reservoir saved = state;
    AbSpan prior = window;
    output[0] = 99;
    CHECK(
        !ab_mp3_reservoir_push(&state, &frame, output, 257, &window, NULL) &&
        memcmp(&state, &saved, sizeof(state)) == 0 && output[0] == 99 &&
        window.data == prior.data && window.size == prior.size &&
        ab_mp3_reservoir_push(&state, &frame, output, sizeof(output), &window, NULL) &&
        window.size == 258 && memcmp(output, saved.bytes, 255) == 0 &&
        memcmp(output + 255, payload, 3) == 0 && state.size == 255 &&
        memcmp(state.bytes, saved.bytes + 3, 252) == 0 &&
        memcmp(state.bytes + 252, payload, 3) == 0);
    frame.main_data = (AbSpan){0};
    frame.side.main_data_begin = 1;
    CHECK(
        ab_mp3_reservoir_push(&state, &frame, output, sizeof(output), &window, NULL) &&
        window.size == 1 && output[0] == payload[2] && state.size == 255);
    saved = state;
    frame.side.main_data_begin = 256;
    CHECK(
        !ab_mp3_reservoir_push(&state, &frame, output, sizeof(output), &window, NULL) &&
        memcmp(&state, &saved, sizeof(state)) == 0 &&
        !ab_mp3_reservoir_push(NULL, &frame, output, sizeof(output), &window, NULL) &&
        !ab_mp3_reservoir_push(&state, NULL, output, sizeof(output), &window, NULL) &&
        !ab_mp3_reservoir_push(&state, &frame, output, sizeof(output), NULL, NULL));
    frame.side.main_data_begin = 0;
    frame.main_data = (AbSpan){NULL, 1};
    CHECK(
        !ab_mp3_reservoir_push(&state, &frame, output, sizeof(output), &window, NULL));
    frame.main_data = (AbSpan){0};
    state.size = 256;
    CHECK(
        !ab_mp3_reservoir_push(&state, &frame, output, sizeof(output), &window, NULL));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 3) {
        fprintf(stderr, "Usage: ab_mp3_reservoir_tests [NATIVE_CORPUS MP3]\n");
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    AbBuffer source = {0};
    bool okay = file && ab_file_read(argv[2], 512 * 1024, &source, NULL) &&
                native_checks(file, (AbSpan){source.data, source.size});
    if (file && fclose(file) != 0) {
        okay = false;
    }
    ab_buffer_free(&source);
    return okay ? 0 : 1;
}
