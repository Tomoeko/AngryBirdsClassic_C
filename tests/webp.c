#include "ab/webp.h"
#include "ab/zip.h"
#include "webp_alpha_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "WebP check failed at %d: %s\n", __LINE__, #condition);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

static void put_word(uint8_t *output, uint32_t value) {
    for (unsigned byte = 0; byte < 4; ++byte) {
        output[byte] = (uint8_t)(value >> (byte * 8));
    }
}

static size_t chunk(uint8_t bytes[256], size_t offset, const char *tag,
                    const uint8_t *data, size_t size) {
    memcpy(bytes + offset, tag, 4);
    put_word(bytes + offset + 4, (uint32_t)size);
    memcpy(bytes + offset + 8, data, size);
    size_t end = offset + 8 + size;
    if (size & 1) {
        bytes[end++] = 0;
    }
    put_word(bytes + 4, (uint32_t)end - 8);
    return end;
}

static bool format_edges(void) {
    const uint8_t frame[10] = {16, 0, 0, 0x9d, 1, 0x2a, 7, 0, 5, 0};
    uint8_t bytes[256] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'E', 'B', 'P'};
    size_t size = chunk(bytes, 12, "VP8 ", frame, sizeof(frame));
    AbWebpStill view;
    CHECK(ab_webp_still_read((AbSpan){bytes, size}, &view, NULL) && view.width == 7 &&
          view.height == 5 && !view.lossless && !view.has_alpha &&
          view.color.data == bytes + 20 && view.color.size == 10);
    const uint8_t junk[3] = {1, 2, 3};
    size = chunk(bytes, size, "JUNK", junk, 3);
    CHECK(ab_webp_still_read((AbSpan){bytes, size}, &view, NULL));
    bytes[size - 1] = 1;
    CHECK(!ab_webp_still_read((AbSpan){bytes, size}, &view, NULL));
    bytes[size - 1] = 0;
    const uint8_t extended[10] = {16, 0, 0, 0, 6, 0, 0, 4, 0, 0};
    size = chunk(bytes, 12, "VP8X", extended, 10);
    uint8_t alpha[36] = {12};
    size_t alpha_offset = size + 8;
    size = chunk(bytes, size, "ALPH", alpha, sizeof(alpha));
    size = chunk(bytes, size, "VP8 ", frame, sizeof(frame));
    CHECK(ab_webp_still_read((AbSpan){bytes, size}, &view, NULL) && view.width == 7 &&
          view.height == 5 && view.has_alpha && view.alpha_filter == 3 &&
          !view.compressed_alpha && !view.alpha_preprocessing &&
          view.alpha.size == 35 && view.alpha.data == bytes + alpha_offset + 1);
    for (size_t length = 0; length < size; ++length) {
        AbWebpStill sentinel = {.width = 99};
        CHECK(!ab_webp_still_read((AbSpan){bytes, length}, &sentinel, NULL) &&
              sentinel.width == 99);
    }
    const uint8_t bad_alpha_flags[] = {2, 0x40, 0x20};
    for (size_t index = 0; index < sizeof(bad_alpha_flags); ++index) {
        bytes[alpha_offset] = bad_alpha_flags[index];
        CHECK(!ab_webp_still_read((AbSpan){bytes, size}, &view, NULL));
    }
    bytes[alpha_offset] = 12;
    bytes[20] = 18;
    CHECK(!ab_webp_still_read((AbSpan){bytes, size}, &view, NULL));
    bytes[20] = 16;
    bytes[24] = 7;
    CHECK(!ab_webp_still_read((AbSpan){bytes, size}, &view, NULL));
    bytes[24] = 6;
    size = chunk(bytes, size, "VP8 ", frame, sizeof(frame));
    CHECK(!ab_webp_still_read((AbSpan){bytes, size}, &view, NULL));
    uint8_t lossless[5] = {0x2f};
    put_word(lossless + 1, 8 | (10u << 14) | (1u << 28));
    size = chunk(bytes, 12, "VP8L", lossless, 5);
    CHECK(ab_webp_still_read((AbSpan){bytes, size}, &view, NULL) && view.width == 9 &&
          view.height == 11 && view.lossless && view.has_alpha);
    bytes[24] |= 0x20;
    CHECK(!ab_webp_still_read((AbSpan){bytes, size}, &view, NULL));
    return true;
}

static bool alpha_edges(void) {
    uint8_t pixels[12] = {10, 250, 8, 0x99, 250, 20, 5, 0x99, 9, 2, 200, 0x99};
    const uint8_t expected[12] = {10, 4, 12, 0x99, 4, 20, 33, 0x99, 13, 31, 244, 0x99};
    CHECK(ab_webp_alpha_unfilter(pixels, sizeof(pixels), 3, 3, 4, 3, 0, 1, NULL) &&
          ab_webp_alpha_unfilter(pixels, sizeof(pixels), 3, 3, 4, 3, 1, 2, NULL) &&
          memcmp(pixels, expected, sizeof(pixels)) == 0);
    uint8_t snapshot[sizeof(pixels)];
    memcpy(snapshot, pixels, sizeof(pixels));
    CHECK(!ab_webp_alpha_unfilter(pixels, 9, 3, 3, 4, 3, 0, 1, NULL) &&
          !ab_webp_alpha_unfilter(pixels, 12, 3, 3, 2, 3, 0, 1, NULL) &&
          !ab_webp_alpha_unfilter(pixels, 12, 3, 3, 4, 4, 0, 1, NULL) &&
          !ab_webp_alpha_unfilter(pixels, 12, 3, 3, 4, 3, 2, 2, NULL) &&
          ab_webp_alpha_unfilter(pixels, 12, 3, 3, 4, 3, 3, 0, NULL) &&
          memcmp(snapshot, pixels, sizeof(pixels)) == 0);
    return true;
}

static bool alpha_decode_edges(void) {
    const uint8_t source[9] = {10, 250, 8, 250, 20, 5, 9, 2, 200};
    const uint8_t expected[9] = {10, 4, 12, 4, 20, 33, 13, 31, 244};
    AbWebpStill image = {.width = 3,
                         .height = 3,
                         .alpha = {source, sizeof(source)},
                         .has_alpha = true,
                         .alpha_filter = 3};
    AbBuffer output = {0};
    CHECK(ab_webp_alpha_decode(&image, 9, &output, NULL) && output.size == 9 &&
          memcmp(output.data, expected, sizeof(expected)) == 0);
    ab_buffer_free(&output);
    CHECK(!ab_webp_alpha_decode(&image, 8, &output, NULL) && !output.data &&
          !output.size);
    image.has_alpha = false;
    CHECK(ab_webp_alpha_decode(&image, 9, &output, NULL) && output.size == 9);
    for (size_t index = 0; index < output.size; ++index) {
        CHECK(output.data[index] == 255);
    }
    ab_buffer_free(&output);
    const uint8_t unsupported[1] = {1};
    image.has_alpha = true;
    image.compressed_alpha = true;
    image.alpha = (AbSpan){unsupported, sizeof(unsupported)};
    CHECK(!ab_webp_alpha_decode(&image, 4096, &output, NULL) && !output.data);
    CHECK(!ab_webp_alpha_decode(NULL, 4096, &output, NULL) && !output.data);
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
    }
    return fgetc(file) == '\n';
}

static bool native_alpha(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_WEBP_ALPHA_V1 4096\n") == 0);
    size_t compared = 0;
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint8_t pixels[640];
        uint32_t input[6], generated[6], expected[160];
        size_t size = webp_alpha_case(sample, generated, pixels);
        CHECK(read_words(file, input, 6) &&
              memcmp(input, generated, sizeof(input)) == 0 &&
              read_words(file, expected, (size + 3) / 4));
        CHECK(ab_webp_alpha_unfilter(pixels, size, input[0], input[1], input[2],
                                     input[3], 0, input[4], NULL) &&
              ab_webp_alpha_unfilter(pixels, size, input[0], input[1], input[2],
                                     input[3], input[4], input[1] - input[4], NULL));
        for (size_t index = 0; index < size; ++index) {
            uint8_t value = (uint8_t)(expected[index / 4] >> ((index % 4) * 8));
            if (pixels[index] != value) {
                fprintf(stderr, "Original alpha sample %zu byte %zu differs\n", sample,
                        index);
                return false;
            }
        }
        compared += size;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB WebP alpha reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Matched 4,096 original alpha unfilters and %zu image/padding bytes, "
           "including split-row reconstruction.\n",
           compared);
    return true;
}

static uint32_t hash_bytes(AbSpan input) {
    uint32_t hash = UINT32_C(2166136261);
    for (size_t index = 0; index < input.size; ++index) {
        hash = (hash ^ input.data[index]) * UINT32_C(16777619);
    }
    return hash;
}

static bool compare_alpha(const AbWebpStill *image, const char *directory, size_t index,
                          AbError *error) {
    char path[1024];
    int length = snprintf(path, sizeof(path), "%s/webp-%zu.rgba", directory, index);
    CHECK(length > 0 && (size_t)length < sizeof(path));
    AbBuffer alpha = {0}, native = {0};
    bool okay = ab_webp_alpha_decode(image, 32 * 1024 * 1024, &alpha, error) &&
                ab_file_read(path, 16 * 1024 * 1024, &native, error) &&
                native.size == alpha.size * 4;
    if (okay) {
        for (size_t pixel = 0; pixel < alpha.size; ++pixel) {
            if (alpha.data[pixel] != native.data[pixel * 4 + 3]) {
                ab_fail(error, "Original WebP alpha image %zu pixel %zu differs: %u/%u",
                        index, pixel, alpha.data[pixel], native.data[pixel * 4 + 3]);
                okay = false;
                break;
            }
        }
    }
    if (okay) {
        printf("Matched %zu original alpha bytes for image %zu.\n", alpha.size, index);
    }
    ab_buffer_free(&alpha);
    ab_buffer_free(&native);
    if (okay && !index) {
        CHECK(!ab_webp_alpha_decode(image, 0, &alpha, NULL) && !alpha.data);
        CHECK(!ab_webp_alpha_decode(image, (size_t)image->width * image->height, &alpha,
                                    NULL) &&
              !alpha.data);
        for (size_t size = 0; size < 16; ++size) {
            AbWebpStill truncated = *image;
            truncated.alpha.size = size;
            CHECK(!ab_webp_alpha_decode(&truncated, 32 * 1024 * 1024, &alpha, NULL) &&
                  !alpha.data);
        }
    }
    return okay;
}

static bool apk_metadata(const AbZip *archive, FILE *native, const char *pixels) {
    size_t count = 0;
    AbError error = {0};
    char line[100];
    if (native) {
        CHECK(fgets(line, sizeof(line), native) &&
              strcmp(line, "AB_ANDROID_WEBP_IMAGES_V1 7\n") == 0);
    }
    for (size_t index = 0; index < archive->count; ++index) {
        const AbZipEntry *entry = archive->entries + index;
        if (entry->name.size < 5 ||
            memcmp(entry->name.data + entry->name.size - 5, ".webp", 5)) {
            continue;
        }
        AbBuffer payload = {0};
        CHECK(ab_zip_read(archive, index, 1024 * 1024, &payload, &error));
        AbWebpStill view;
        bool okay =
            ab_webp_still_read((AbSpan){payload.data, payload.size}, &view, &error);
        if (okay) {
            okay = !view.lossless && view.has_alpha && view.compressed_alpha;
            if (native) {
                uint32_t expected[7];
                okay =
                    okay && read_words(native, expected, 7) && expected[0] == count &&
                    expected[1] == payload.size &&
                    expected[2] == hash_bytes((AbSpan){payload.data, payload.size}) &&
                    expected[3] == view.width && expected[4] == view.height &&
                    expected[5] == (size_t)view.width * view.height * 4;
            }
            if (okay && pixels) {
                okay = compare_alpha(&view, pixels, count, &error);
            }
            printf("%.*s: %ux%u, %zu color/%zu compressed alpha bytes, filter %u.\n",
                   (int)entry->name.size, (const char *)entry->name.data, view.width,
                   view.height, view.color.size, view.alpha.size, view.alpha_filter);
        }
        ab_buffer_free(&payload);
        if (!okay) {
            fprintf(stderr, "%s\n", error.message);
            return false;
        }
        ++count;
    }
    CHECK(count == 7);
    if (native) {
        CHECK(fgets(line, sizeof(line), native) &&
              strcmp(line, "AB WebP image reference complete\n") == 0 &&
              fgetc(native) == EOF && !ferror(native));
    }
    puts("Parsed all seven APK WebP containers; complete color decoding remains open.");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return format_edges() && alpha_edges() && alpha_decode_edges() ? 0 : 1;
    }
    if (argc == 2) {
        FILE *file = fopen(argv[1], "r");
        if (!file) {
            return 1;
        }
        bool okay = native_alpha(file);
        if (fclose(file) != 0) {
            okay = false;
        }
        return okay ? 0 : 1;
    }
    bool metadata = argc == 4 && strcmp(argv[1], "--metadata") == 0;
    bool alpha = argc == 4 && strcmp(argv[1], "--alpha") == 0;
    if (!metadata && !alpha && (argc != 3 || strcmp(argv[1], "--apk"))) {
        return 2;
    }
    FILE *native = metadata ? fopen(argv[3], "r") : NULL;
    if (metadata && !native) {
        return 1;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbError error = {0};
    bool okay = ab_file_read(argv[2], 128 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                apk_metadata(&archive, native, alpha ? argv[3] : NULL);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    if (native && fclose(native) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
