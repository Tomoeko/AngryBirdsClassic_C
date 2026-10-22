#include "ab/image.h"
#include "ab/zip.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "WebP decode check failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    AbImage image = {.width = 99};
    CHECK(!ab_webp_decode((AbSpan){0}, 0, &image, NULL) && !image.width &&
          !image.height && !image.rgba.data && !image.rgba.size);
    CHECK(!ab_webp_decode((AbSpan){0}, 0, NULL, NULL));
    return true;
}

static bool compare_image(AbSpan input, const char *directory, size_t index) {
    char path[1024];
    int length = snprintf(path, sizeof(path), "%s/webp-%zu.rgba", directory, index);
    CHECK(length > 0 && (size_t)length < sizeof(path));
    AbImage image = {0};
    CHECK(!ab_webp_decode(input, 0, &image, NULL) && !image.rgba.data &&
          !image.rgba.size);
    AbBuffer expected = {0};
    AbError error = {0};
    CHECK(ab_file_read(path, 8 * 1024 * 1024, &expected, &error));
    bool okay = ab_webp_decode(input, 32 * 1024 * 1024, &image, &error);
    if (!okay) {
        fprintf(stderr, "WebP image %zu: %s\n", index, error.message);
    } else if (image.rgba.size != expected.size ||
               memcmp(image.rgba.data, expected.data, expected.size)) {
        fprintf(stderr, "WebP image %zu differs from original RGBA output\n", index);
        okay = false;
    } else {
        printf(
            "Matched original WebP image %zu: %zu RGBA bytes from compressed input.\n",
            index, expected.size);
    }
    ab_image_free(&image);
    ab_buffer_free(&expected);
    return okay;
}

static bool apk_checks(const AbZip *archive, const char *directory) {
    size_t count = 0;
    for (size_t entry = 0; entry < archive->count; ++entry) {
        AbSpan name = archive->entries[entry].name;
        if (name.size < 5 || memcmp(name.data + name.size - 5, ".webp", 5)) {
            continue;
        }
        AbBuffer payload = {0};
        CHECK(ab_zip_read(archive, entry, 1024 * 1024, &payload, NULL));
        bool okay =
            compare_image((AbSpan){payload.data, payload.size}, directory, count);
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
