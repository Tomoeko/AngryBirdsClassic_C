#include "../src/resource/vp8_header.h"
#include "ab/webp.h"
#include "ab/zip.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "VP8 header check failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    uint8_t bytes[10] = {16, 0, 0, 0x9d, 1, 0x2a, 1, 0, 1, 0};
    AbVp8Header output = {.width = 99};
    uint8_t snapshot[sizeof(output)];
    memcpy(snapshot, &output, sizeof(output));
    for (size_t size = 0; size <= sizeof(bytes); ++size) {
        CHECK(!ab_vp8_header_read((AbSpan){bytes, size}, &output, NULL) &&
              memcmp(snapshot, &output, sizeof(output)) == 0);
    }
    CHECK(!ab_vp8_header_read((AbSpan){NULL, 10}, &output, NULL) &&
          !ab_vp8_header_read((AbSpan){bytes, 10}, NULL, NULL));
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

static void header_words(const AbVp8Header *header, uint32_t output[88]) {
    memset(output, 0, 88 * sizeof(*output));
    output[1] = output[2] = output[4] = 1;
    output[3] = header->profile;
    output[5] = header->first_partition_size;
    output[6] = header->width;
    output[7] = header->height;
    output[8] = header->x_scale;
    output[9] = header->y_scale;
    output[10] = header->colorspace;
    output[11] = header->clamp;
    output[12] = header->filter.simple;
    output[13] = header->filter.level;
    output[14] = header->filter.sharpness;
    output[15] = header->filter.use_deltas;
    for (size_t index = 0; index < 4; ++index) {
        output[16 + index] = (uint32_t)header->filter.reference_deltas[index];
        output[20 + index] = (uint32_t)header->filter.mode_deltas[index];
        output[27 + index] = (uint32_t)(int32_t)header->segments.quantizers[index];
        output[31 + index] = (uint32_t)(int32_t)header->segments.filters[index];
    }
    output[24] = header->segments.enabled;
    output[25] = header->segments.update_map;
    output[26] = header->segments.absolute;
    for (size_t index = 0; index < 3; ++index) {
        output[35 + index] = header->segments.probabilities[index];
    }
    output[38] = header->partition_count;
    output[39] = header->macroblock_width;
    output[40] = header->macroblock_height;
    output[41] = header->use_skip_probability;
    output[42] = header->skip_probability;
    for (size_t segment = 0; segment < 4; ++segment) {
        for (size_t value = 0; value < AB_VP8_QUANTIZER_VALUES; ++value) {
            output[44 + segment * 7 + value] =
                (uint32_t)header->quantizers[segment][value];
        }
    }
    for (size_t index = 0; index < header->partition_count; ++index) {
        AbSpan partition = header->partitions[index];
        size_t offset = (size_t)(partition.data - header->frame.data);
        output[72 + index * 2] = (uint32_t)offset;
        output[73 + index * 2] = (uint32_t)(offset + partition.size);
    }
}

static bool compare_header(FILE *file, AbSpan frame, size_t image) {
    uint32_t expected[88], actual[88], probabilities[264], continuation[34];
    CHECK(read_words(file, expected, 88) && read_words(file, probabilities, 264) &&
          read_words(file, continuation, 34));
    AbVp8Header header;
    AbError error = {0};
    if (!ab_vp8_header_read(frame, &header, &error)) {
        fprintf(stderr, "%s\n", error.message);
        return false;
    }
    header_words(&header, actual);
    for (size_t index = 0; index < 88; ++index) {
        if (expected[index] != actual[index]) {
            fprintf(stderr,
                    "Original VP8 header %zu field %zu differs: %08" PRIx32
                    "/%08" PRIx32 "\n",
                    image, index, actual[index], expected[index]);
            return false;
        }
    }
    for (size_t index = 0; index < 264; ++index) {
        uint32_t value = ab_u32le(header.coefficient_probabilities + index * 4);
        CHECK(value == probabilities[index]);
    }
    size_t consumed = (header.control.offset - 2) * 8 + header.control.shifted;
    CHECK(header.control.range == continuation[0] && consumed == continuation[1]);
    for (size_t index = 0; index < 32; ++index) {
        unsigned width = (unsigned)(index * 7 + image * 3) % 11;
        uint32_t value;
        CHECK(ab_vp8_boolean_value(&header.control, width, &value, NULL) &&
              value == continuation[index + 2]);
    }
    printf("Matched original VP8 header %zu, all 1,056 coefficient probabilities, "
           "quantizer matrices and continuing arithmetic state.\n",
           image);
    return true;
}

static bool frame_checks(const AbZip *archive, FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_VP8_HEADER_V2 7\n") == 0);
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
            input[3] == payload.size && compare_header(file, image.color, count);
        ab_buffer_free(&payload);
        CHECK(okay);
        ++count;
    }
    CHECK(count == 7 && fgets(line, sizeof(line), file) &&
          strcmp(line, "AB VP8 header reference complete\n") == 0 &&
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
    if (fclose(reference) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
