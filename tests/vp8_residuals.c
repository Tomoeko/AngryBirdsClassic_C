#include "../src/resource/vp8_residuals.h"
#include "../src/resource/vp8_transform.h"
#include "ab/webp.h"
#include "ab/zip.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "VP8 residual check failed at %d: %s\n", __LINE__,         \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    int16_t input[16] = {8}, output[16];
    ab_vp8_walsh_inverse(input, output);
    for (size_t index = 0; index < 16; ++index) {
        CHECK(output[index] == 1);
    }
    input[0] = -4;
    ab_vp8_walsh_inverse(input, input);
    for (size_t index = 0; index < 16; ++index) {
        CHECK(input[index] == -1);
    }
    AbVp8Header header = {0};
    AbVp8Boolean reader = {0};
    AbVp8Macroblock block = {.skip = true, .luma4 = true};
    AbVp8Nonzero above = {.y2 = 1, .y = {1, 1, 1, 1}}, left = above;
    int16_t coefficients[384] = {1234};
    CHECK(ab_vp8_residuals_read(&header, &reader, &block, &above, &left, coefficients,
                                NULL) &&
          above.y2 == 1 && left.y2 == 1 && !above.y[0] && !left.y[3]);
    for (size_t index = 0; index < 384; ++index) {
        CHECK(coefficients[index] == 0);
    }
    block.luma4 = false;
    CHECK(ab_vp8_residuals_read(&header, &reader, &block, &above, &left, coefficients,
                                NULL) &&
          !above.y2 && !left.y2);
    above.u[1] = 2;
    CHECK(!ab_vp8_residuals_read(&header, &reader, &block, &above, &left, coefficients,
                                 NULL));
    return true;
}

static uint32_t context_word(const AbVp8Nonzero *context) {
    uint32_t value = (uint32_t)context->y2 << 8;
    for (size_t index = 0; index < 4; ++index) {
        value |= (uint32_t)context->y[index] << index;
    }
    for (size_t index = 0; index < 2; ++index) {
        value |= (uint32_t)context->u[index] << (index + 4);
        value |= (uint32_t)context->v[index] << (index + 6);
    }
    return value;
}

static bool compare_record(const uint8_t *expected, const int16_t coefficients[384],
                           const AbVp8Boolean *reader, const AbVp8Nonzero *above,
                           const AbVp8Nonzero *left, size_t image, uint32_t row,
                           uint32_t column) {
    for (size_t index = 0; index < 196; ++index) {
        uint32_t actual;
        if (index < 192) {
            actual = (uint32_t)(uint16_t)coefficients[index * 2] |
                     ((uint32_t)(uint16_t)coefficients[index * 2 + 1] << 16);
        } else if (index == 192) {
            actual = reader->range;
        } else if (index == 193) {
            actual = (uint32_t)((reader->offset - 2) * 8 + reader->shifted);
        } else if (index == 194) {
            actual = 0;
        } else {
            actual = context_word(above) | (context_word(left) << 16);
        }
        uint32_t value = ab_u32le(expected + index * 4);
        if (actual != value) {
            fprintf(stderr,
                    "VP8 residual image %zu block %u,%u word %zu: %08" PRIx32
                    "/%08" PRIx32 "\n",
                    image, column, row, index, actual, value);
            return false;
        }
    }
    return true;
}

static bool compare_frame(AbSpan frame, AbSpan reference, size_t image) {
    AbVp8Header header;
    AbError error = {0};
    CHECK(ab_vp8_header_read(frame, &header, &error) && header.partition_count == 1 &&
          header.macroblock_width <= 128 &&
          reference.size ==
              (size_t)header.macroblock_width * header.macroblock_height * 784);
    uint8_t above_modes[4 * 128] = {0};
    AbVp8Macroblock blocks[128];
    AbVp8Nonzero above[128] = {0};
    AbVp8Boolean tokens;
    CHECK(ab_vp8_boolean_init(header.partitions[0], &tokens, &error));
    size_t offset = 0;
    for (uint32_t row = 0; row < header.macroblock_height; ++row) {
        CHECK(ab_vp8_modes_read_row(&header, above_modes, sizeof(above_modes), blocks,
                                    128, &error));
        AbVp8Nonzero left = {0};
        for (uint32_t column = 0; column < header.macroblock_width; ++column) {
            int16_t coefficients[384];
            if (!ab_vp8_residuals_read(&header, &tokens, blocks + column,
                                       above + column, &left, coefficients, &error)) {
                fprintf(stderr, "VP8 residual image %zu block %u,%u: %s\n", image,
                        column, row, error.message);
                return false;
            }
            CHECK(compare_record(reference.data + offset, coefficients, &tokens,
                                 above + column, &left, image, row, column));
            offset += 784;
        }
    }
    printf(
        "Matched original VP8 image %zu: %u macroblock coefficient/context states.\n",
        image, header.macroblock_width * header.macroblock_height);
    return true;
}

static bool apk_checks(const AbZip *archive, const char *directory) {
    size_t count = 0;
    for (size_t entry = 0; entry < archive->count; ++entry) {
        AbSpan name = archive->entries[entry].name;
        if (name.size < 5 || memcmp(name.data + name.size - 5, ".webp", 5)) {
            continue;
        }
        AbBuffer payload = {0}, reference = {0};
        AbWebpStill image;
        char path[1024];
        int length =
            snprintf(path, sizeof(path), "%s/webp-%zu.coefficients", directory, count);
        CHECK(length > 0 && (size_t)length < sizeof(path));
        CHECK(ab_zip_read(archive, entry, 1024 * 1024, &payload, NULL));
        bool okay =
            ab_file_read(path, 8 * 1024 * 1024, &reference, NULL) &&
            ab_webp_still_read((AbSpan){payload.data, payload.size}, &image, NULL) &&
            compare_frame(image.color, (AbSpan){reference.data, reference.size}, count);
        ab_buffer_free(&reference);
        ab_buffer_free(&payload);
        CHECK(okay);
        ++count;
    }
    return count == 7;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 4 || strcmp(argv[1], "--apk")) {
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbError error = {0};
    bool okay = ab_file_read(argv[2], 128 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                apk_checks(&archive, argv[3]);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
