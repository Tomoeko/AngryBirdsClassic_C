#include "../src/resource/vp8_modes.h"
#include "ab/webp.h"
#include "ab/zip.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "VP8 mode check failed at %d: %s\n", __LINE__,             \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    AbVp8Header header = {.macroblock_width = 1};
    uint8_t above[4] = {0};
    AbVp8Macroblock block = {.segment = 99};
    CHECK(!ab_vp8_modes_read_row(NULL, above, 4, &block, 1, NULL) &&
          !ab_vp8_modes_read_row(&header, above, 3, &block, 1, NULL) &&
          !ab_vp8_modes_read_row(&header, above, 4, &block, 0, NULL));
    above[2] = 10;
    CHECK(!ab_vp8_modes_read_row(&header, above, 4, &block, 1, NULL) &&
          block.segment == 99);
    above[2] = 0;
    CHECK(!ab_vp8_modes_read_row(&header, above, 4, &block, 1, NULL) &&
          block.segment == 99);
    return true;
}

static uint32_t hash(AbSpan input) {
    uint32_t result = UINT32_C(2166136261);
    for (size_t index = 0; index < input.size; ++index) {
        result = (result ^ input.data[index]) * UINT32_C(16777619);
    }
    return result;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
    }
    return fgetc(file) == '\n';
}

static uint8_t native_mode(uint8_t mode) {
    static const uint8_t mapping[10] = {0, 1, 2, 3, 6, 4, 5, 7, 8, 9};
    return mapping[mode];
}

static bool compare_frame(FILE *file, AbSpan frame, size_t image) {
    AbVp8Header header;
    AbError error = {0};
    if (!ab_vp8_header_read(frame, &header, &error)) {
        fprintf(stderr, "%s\n", error.message);
        return false;
    }
    uint32_t dimensions[4];
    CHECK(read_words(file, dimensions, 4) && dimensions[0] == header.width &&
          dimensions[1] == header.height && dimensions[2] == header.macroblock_width &&
          dimensions[3] == header.macroblock_height && header.macroblock_width <= 128);
    uint8_t above[4 * 128] = {0};
    AbVp8Macroblock blocks[128];
    for (uint32_t row = 0; row < header.macroblock_height; ++row) {
        if (!ab_vp8_modes_read_row(&header, above, sizeof(above), blocks, 128,
                                   &error)) {
            fprintf(stderr, "Image %zu row %u: %s\n", image, row, error.message);
            return false;
        }
        uint32_t state[4];
        size_t consumed = (header.control.offset - 2) * 8 + header.control.shifted;
        CHECK(read_words(file, state, 4) && state[0] == row &&
              state[1] == header.control.range && state[2] == consumed &&
              state[3] == 0);
        for (uint32_t column = 0; column < header.macroblock_width; ++column) {
            const AbVp8Macroblock *block = blocks + column;
            uint32_t expected[8],
                actual[8] = {block->segment, block->skip, block->luma4, 0, 0, 0, 0,
                             block->chroma};
            CHECK(read_words(file, expected, 8));
            for (size_t index = 0; index < 16; ++index) {
                actual[3 + index / 4] |= (uint32_t)native_mode(block->luma[index])
                                         << (index % 4 * 8);
            }
            for (size_t index = 0; index < 8; ++index) {
                if (actual[index] != expected[index]) {
                    fprintf(stderr,
                            "VP8 image %zu macroblock %u,%u field %zu: %08" PRIx32
                            "/%08" PRIx32 "\n",
                            image, column, row, index, actual[index], expected[index]);
                    return false;
                }
            }
        }
    }
    printf("Matched original VP8 image %zu: %u macroblocks and %u row states.\n", image,
           header.macroblock_width * header.macroblock_height,
           header.macroblock_height);
    return true;
}

static bool frame_checks(const AbZip *archive, FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_VP8_MODES_V1 7\n") == 0);
    size_t count = 0;
    for (size_t entry = 0; entry < archive->count; ++entry) {
        AbSpan name = archive->entries[entry].name;
        if (name.size < 5 || memcmp(name.data + name.size - 5, ".webp", 5)) {
            continue;
        }
        AbBuffer payload = {0};
        AbWebpStill image;
        CHECK(ab_zip_read(archive, entry, 1024 * 1024, &payload, NULL));
        uint32_t input[4];
        bool okay =
            ab_webp_still_read((AbSpan){payload.data, payload.size}, &image, NULL) &&
            read_words(file, input, 4) && input[0] == count &&
            input[1] == image.color.size && input[2] == hash(image.color) &&
            input[3] == payload.size && compare_frame(file, image.color, count);
        ab_buffer_free(&payload);
        CHECK(okay);
        ++count;
    }
    CHECK(count == 7 && fgets(line, sizeof(line), file) &&
          strcmp(line, "AB VP8 modes reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 4 || strcmp(argv[1], "--apk")) {
        return 2;
    }
    FILE *reference = fopen(argv[3], "r");
    if (!reference) {
        return 1;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbError error = {0};
    bool okay = ab_file_read(argv[2], 128 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                frame_checks(&archive, reference);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    if (fclose(reference)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
