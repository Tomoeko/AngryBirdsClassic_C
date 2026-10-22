#include "ab/webp_color.h"
#include "ab/webp.h"
#include "ab/zip.h"
#include "webp_upsample_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "WebP color check failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    const uint8_t y[4] = {16, 235, 81, 145}, uv[1] = {128};
    const uint8_t alpha[4] = {0, 10, 128, 255};
    const uint8_t expected[16] = {0,  0,  0,  0,   255, 255, 255, 10,
                                  76, 76, 76, 128, 150, 150, 150, 255};
    AbWebpYuv420 image = {2, 2, {y, 4}, {uv, 1}, {uv, 1}};
    uint8_t rgba[16], snapshot[16];
    CHECK(ab_webp_yuv420_rgba(&image, (AbSpan){alpha, 4}, (AbBuffer){rgba, 16}, NULL) &&
          memcmp(rgba, expected, sizeof(expected)) == 0);
    CHECK(ab_webp_yuv420_rgba(&image, (AbSpan){0}, (AbBuffer){rgba, 16}, NULL));
    for (size_t index = 0; index < 4; ++index) {
        CHECK(rgba[index * 4 + 3] == 255);
    }
    memcpy(snapshot, rgba, sizeof(snapshot));
    CHECK(
        !ab_webp_yuv420_rgba(&image, (AbSpan){alpha, 3}, (AbBuffer){rgba, 16}, NULL) &&
        !ab_webp_yuv420_rgba(&image, (AbSpan){0}, (AbBuffer){rgba, 15}, NULL));
    image.y.size = 3;
    CHECK(!ab_webp_yuv420_rgba(&image, (AbSpan){0}, (AbBuffer){rgba, 16}, NULL) &&
          memcmp(snapshot, rgba, sizeof(snapshot)) == 0);
    AbWebpYuvPair pair = {.top_y = {y, 2},
                          .top_u = {uv, 1},
                          .top_v = {uv, 1},
                          .bottom_u = {uv, 1},
                          .bottom_v = {uv, 1},
                          .width = 2};
    CHECK(ab_webp_upsample_rgba(&pair, (AbBuffer){rgba, 16}, (AbBuffer){0}, NULL));
    memcpy(snapshot, rgba, sizeof(snapshot));
    pair.bottom_y = (AbSpan){y, 2};
    CHECK(!ab_webp_upsample_rgba(&pair, (AbBuffer){rgba, 16}, (AbBuffer){0}, NULL) &&
          memcmp(snapshot, rgba, sizeof(snapshot)) == 0);
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
    }
    return fgetc(file) == '\n';
}

static bool native_checks(FILE *file) {
    char line[100];
    uint32_t routine;
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_WEBP_UPSAMPLE_V1 4096\n") == 0 &&
          read_words(file, &routine, 1) && routine == 0x875eb8);
    size_t pixels = 0;
    for (size_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[4], generated[4], expected[160];
        WebpUpsampleCase source;
        webp_upsample_case(sample, generated, &source);
        CHECK(read_words(file, input, 4) &&
              memcmp(input, generated, sizeof(input)) == 0 &&
              read_words(file, expected, 160));
        uint8_t top[320], bottom[320];
        memset(top, 0xa5, sizeof(top));
        memset(bottom, 0xa5, sizeof(bottom));
        AbWebpYuvPair pair = {.top_y = {source.top_y, input[0]},
                              .bottom_y = input[1] & 2u
                                              ? (AbSpan){source.bottom_y, input[0]}
                                              : (AbSpan){0},
                              .top_u = {source.top_u, 48},
                              .top_v = {source.top_v, 48},
                              .bottom_u = {source.bottom_u, 48},
                              .bottom_v = {source.bottom_v, 48},
                              .width = input[0]};
        CHECK(ab_webp_upsample_rgba(&pair, (AbBuffer){top, sizeof(top)},
                                    (AbBuffer){bottom, sizeof(bottom)}, NULL));
        for (size_t word = 0; word < 160; ++word) {
            const uint8_t *row = word < 80 ? top : bottom;
            uint32_t actual = ab_u32le(row + (word % 80) * 4);
            if (actual != expected[word]) {
                fprintf(stderr, "Original upsample sample %zu word %zu differs\n",
                        sample, word);
                return false;
            }
        }
        pixels += (size_t)input[0] * (input[1] & 2u ? 2u : 1u);
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB WebP upsample reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("Matched 4,096 original RGBA line pairs, %zu pixels and every output "
           "guard byte.\n",
           pixels);
    return true;
}

static bool compare_frame(const AbZip *archive, size_t entry, const char *directory,
                          size_t index, AbError *error) {
    AbBuffer encoded = {0}, native = {0}, planes = {0}, alpha = {0}, rgba = {0};
    AbWebpStill info;
    bool okay = false;
    if (!ab_zip_read(archive, entry, 1024 * 1024, &encoded, error) ||
        !ab_webp_still_read((AbSpan){encoded.data, encoded.size}, &info, error)) {
        goto release_frame;
    }
    size_t count = (size_t)info.width * info.height;
    size_t chroma = (size_t)((info.width + 1) / 2) * ((info.height + 1) / 2);
    char yuv_path[1024], rgba_path[1024];
    int first =
        snprintf(yuv_path, sizeof(yuv_path), "%s/webp-%zu.yuv", directory, index);
    int second =
        snprintf(rgba_path, sizeof(rgba_path), "%s/webp-%zu.rgba", directory, index);
    if (count > 4 * 1024 * 1024 || first <= 0 || (size_t)first >= sizeof(yuv_path) ||
        second <= 0 || (size_t)second >= sizeof(rgba_path) ||
        !ab_file_read(yuv_path, 16 * 1024 * 1024, &planes, error) ||
        !ab_file_read(rgba_path, 16 * 1024 * 1024, &native, error) ||
        planes.size != count + chroma * 2 || native.size != count * 4 ||
        !ab_webp_alpha_decode(&info, 32 * 1024 * 1024, &alpha, error)) {
        goto release_frame;
    }
    rgba.data = malloc(count * 4);
    rgba.size = count * 4;
    if (!rgba.data) {
        ab_fail(error, "Cannot allocate WebP color comparison output");
        goto release_frame;
    }
    AbWebpYuv420 image = {info.width,
                          info.height,
                          {planes.data, count},
                          {planes.data + count, chroma},
                          {planes.data + count + chroma, chroma}};
    if (!ab_webp_yuv420_rgba(&image, (AbSpan){alpha.data, alpha.size}, rgba, error)) {
        goto release_frame;
    }
    for (size_t byte = 0; byte < rgba.size; ++byte) {
        if (rgba.data[byte] != native.data[byte]) {
            ab_fail(error, "Original WebP RGBA image %zu byte %zu differs: %u/%u",
                    index, byte, rgba.data[byte], native.data[byte]);
            goto release_frame;
        }
    }
    printf("Matched %zu original RGBA bytes from native YUV planes and decoded "
           "C alpha for image %zu.\n",
           rgba.size, index);
    okay = true;

release_frame:
    ab_buffer_free(&rgba);
    ab_buffer_free(&alpha);
    ab_buffer_free(&planes);
    ab_buffer_free(&native);
    ab_buffer_free(&encoded);
    return okay;
}

static bool plane_checks(const AbZip *archive, const char *directory, AbError *error) {
    size_t count = 0;
    for (size_t entry = 0; entry < archive->count; ++entry) {
        AbSpan name = archive->entries[entry].name;
        if (name.size >= 5 && memcmp(name.data + name.size - 5, ".webp", 5) == 0) {
            CHECK(compare_frame(archive, entry, directory, count, error));
            ++count;
        }
    }
    CHECK(count == 7);
    puts("All seven native YUV-to-RGBA compositions match; compressed VP8 plane "
         "decoding remains open.");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc == 2) {
        FILE *file = fopen(argv[1], "r");
        if (!file) {
            return 1;
        }
        bool okay = native_checks(file);
        if (fclose(file) != 0) {
            okay = false;
        }
        return okay ? 0 : 1;
    }
    if (argc != 4 || strcmp(argv[1], "--planes")) {
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbError error = {0};
    bool okay = ab_file_read(argv[2], 128 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                plane_checks(&archive, argv[3], &error);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
