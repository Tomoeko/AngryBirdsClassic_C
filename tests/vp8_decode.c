#include "../src/resource/vp8_decode.h"
#include "ab/webp.h"
#include "ab/zip.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "VP8 decode check failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    AbVp8Image image = {.width = 99};
    CHECK(!ab_vp8_decode((AbSpan){0}, 0, false, &image, NULL) && !image.width &&
          !image.height && !image.yuv.data && !image.yuv.size);
    CHECK(!ab_vp8_decode((AbSpan){0}, 0, false, NULL, NULL));
    return true;
}

static bool compare_planes(AbSpan frame, const char *directory, size_t index,
                           bool bypass) {
    char path[1024];
    int length = snprintf(path, sizeof(path), "%s/webp-%zu.%syuv", directory, index,
                          bypass ? "unfiltered." : "");
    CHECK(length > 0 && (size_t)length < sizeof(path));
    AbBuffer expected = {0};
    AbVp8Image image = {0};
    AbError error = {0};
    CHECK(ab_file_read(path, 8 * 1024 * 1024, &expected, &error));
    bool okay = ab_vp8_decode(frame, 32 * 1024 * 1024, bypass, &image, &error);
    if (!okay) {
        fprintf(stderr, "VP8 image %zu bypass %d: %s\n", index, bypass, error.message);
    } else if (image.yuv.size != expected.size) {
        fprintf(stderr, "VP8 image %zu bypass %d has wrong plane size\n", index,
                bypass);
        okay = false;
    } else {
        size_t mismatches = 0, first = 0;
        for (size_t offset = 0; offset < expected.size; ++offset) {
            if (image.yuv.data[offset] != expected.data[offset]) {
                if (!mismatches) {
                    first = offset;
                }
                ++mismatches;
            }
        }
        if (mismatches) {
            size_t y_size = (size_t)image.width * image.height;
            fprintf(stderr,
                    "VP8 image %zu bypass %d: %zu bytes differ; first %zu "
                    "(%u/%u), Y size %zu, width %u\n",
                    index, bypass, mismatches, first, image.yuv.data[first],
                    expected.data[first], y_size, image.width);
            okay = false;
        }
    }
    if (okay) {
        printf("Matched original VP8 image %zu bypass %d: %zu YUV bytes.\n", index,
               bypass, expected.size);
    }
    ab_buffer_free(&image.yuv);
    ab_buffer_free(&expected);
    return okay;
}

static bool frame_edges(AbSpan frame) {
    AbVp8Image image;
    CHECK(!ab_vp8_decode(frame, 0, false, &image, NULL) && !image.yuv.data &&
          !image.yuv.size);
    for (size_t size = 0; size < 16; ++size) {
        CHECK(!ab_vp8_decode((AbSpan){frame.data, size}, 32 * 1024 * 1024, false,
                             &image, NULL) &&
              !image.yuv.data && !image.yuv.size);
    }
    CHECK(!ab_vp8_decode((AbSpan){frame.data, frame.size / 2}, 32 * 1024 * 1024, false,
                         &image, NULL) &&
          !image.yuv.data && !image.yuv.size);
    return true;
}

static bool apk_checks(const AbZip *archive, const char *directory, bool bypass) {
    size_t count = 0;
    for (size_t entry = 0; entry < archive->count; ++entry) {
        AbSpan name = archive->entries[entry].name;
        if (name.size < 5 || memcmp(name.data + name.size - 5, ".webp", 5)) {
            continue;
        }
        AbBuffer payload = {0};
        AbWebpStill image;
        CHECK(ab_zip_read(archive, entry, 1024 * 1024, &payload, NULL));
        bool okay =
            ab_webp_still_read((AbSpan){payload.data, payload.size}, &image, NULL) &&
            (count != 0 || frame_edges(image.color)) &&
            compare_planes(image.color, directory, count, bypass);
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
    if (argc != 4 || (strcmp(argv[1], "--apk") && strcmp(argv[1], "--unfiltered"))) {
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbError error = {0};
    bool okay = ab_file_read(argv[2], 128 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                apk_checks(&archive, argv[3], strcmp(argv[1], "--unfiltered") == 0);
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
