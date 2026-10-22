#include "ab/image.h"

#include <stdio.h>
#include <string.h>

enum { WIDTH = 480, HEIGHT = 320, HEADER_WORDS = 32 };

typedef struct {
    AbBuffer bytes;
    const uint8_t *pixels;
} Capture;

static bool read_capture(const char *path, uint32_t alpha_bits, Capture *output,
                         AbError *error) {
    static const char magic[] = "AB_GL_CAPTURE_V1\n";
    const size_t header = sizeof(magic) - 1;
    const size_t offset = header + HEADER_WORDS * 4;
    if (!ab_file_read(path, 1024 * 1024, &output->bytes, error)) {
        return false;
    }
    const uint8_t *data = output->bytes.data;
    if (output->bytes.size != offset + WIDTH * HEIGHT * 4 ||
        memcmp(data, magic, sizeof(magic) - 1) != 0 ||
        ab_u32le(data + header) != WIDTH || ab_u32le(data + header + 4) != HEIGHT ||
        ab_u32le(data + header + 8) != 0 || ab_u32le(data + header + 12) != 8 ||
        ab_u32le(data + header + 16) != 8 || ab_u32le(data + header + 20) != 8 ||
        ab_u32le(data + header + 24) != alpha_bits ||
        ab_u32le(data + header + 36) != 0) {
        return ab_fail(error, "Unexpected complete RGB8 framebuffer header");
    }
    output->pixels = data + offset;
    return true;
}

static const uint8_t *pixel(const Capture *capture, size_t x, size_t y) {
    return capture->pixels + ((HEIGHT - 1 - y) * WIDTH + x) * 4;
}

static bool native_png(const Capture *native, const char *path, AbError *error) {
    AbBuffer encoded = {0};
    AbImage image = {0};
    bool okay = ab_file_read(path, 1024 * 1024, &encoded, error) &&
                ab_png_decode((AbSpan){encoded.data, encoded.size}, 1024 * 1024, &image,
                              error) &&
                image.width == WIDTH && image.height == HEIGHT &&
                image.rgba.size == WIDTH * HEIGHT * 4;
    for (size_t y = 0; okay && y < HEIGHT; ++y) {
        okay = memcmp(image.rgba.data + y * WIDTH * 4, pixel(native, 0, y),
                      WIDTH * 4) == 0;
    }
    ab_image_free(&image);
    ab_buffer_free(&encoded);
    return okay || ab_fail(error, "Original PNG differs from native framebuffer");
}

static bool selected_regions(const Capture *native, const Capture *gles, bool sling,
                             bool hud, bool idle, AbError *error) {
    size_t background = 0, ground = 0;
    for (size_t y = 100; y < HEIGHT; ++y) {
        if (y >= 240 && y < 280) {
            continue;
        }
        for (size_t x = 0; x < WIDTH; ++x) {
            /* Exclude the original sling/bird from the background region.
             * HUD and foreground characters are outside the selected rows. */
            if (y < 240 && x >= 145 && x < 180 && y >= 190) {
                continue;
            }
            if (memcmp(pixel(native, x, y), pixel(gles, x, y), 4) != 0) {
                return ab_fail(error, "GLES2/native RGBA mismatch at (%zu,%zu)", x, y);
            }
            if (y < 240) {
                ++background;
            } else {
                ++ground;
            }
        }
    }
    if (background != 65450 || ground != 19200) {
        return ab_fail(error, "Unexpected selected framebuffer region size");
    }
    if (sling) {
        /* Full retained game area below the HUD, including sling, all visible
         * characters, ground and foreground. This is one fixed idle frame. */
        for (size_t y = 100; y < HEIGHT; ++y) {
            for (size_t x = 0; x < WIDTH; ++x) {
                if (memcmp(pixel(native, x, y), pixel(gles, x, y), 4) != 0) {
                    return ab_fail(error, "GLES2/native sling mismatch at (%zu,%zu)", x,
                                   y);
                }
            }
        }
    }
    if (hud) {
        for (size_t y = 0; y < 100; ++y) {
            for (size_t x = idle ? 0 : 160; x < WIDTH; ++x) {
                if (memcmp(pixel(native, x, y), pixel(gles, x, y), 4) != 0) {
                    return ab_fail(error, "GLES2/native HUD mismatch at (%zu,%zu)", x,
                                   y);
                }
            }
        }
    }
    return true;
}

int main(int argc, char **argv) {
    bool idle = argc == 5 && strcmp(argv[1], "--idle") == 0;
    bool hud = idle || (argc == 5 && strcmp(argv[1], "--hud") == 0);
    bool sling = hud || (argc == 5 && strcmp(argv[1], "--sling") == 0);
    if (argc != 4 && !sling) {
        return 2;
    }
    Capture native = {0}, gles = {0};
    AbError error = {0};
    int first = sling ? 2 : 1;
    bool okay = read_capture(argv[first], 0, &native, &error) &&
                read_capture(argv[first + 2], 8, &gles, &error) &&
                native_png(&native, argv[first + 1], &error) &&
                selected_regions(&native, &gles, sling, hud, idle, &error);
    if (okay) {
        puts("Original PNG matches all 614400 framebuffer bytes; first-party "
             "Android GLES2 matches 65450 background and 19200 ground RGBA pixels. "
             "Metal, presentation and complete gameplay remain separate.");
        if (sling) {
            puts("All 105600 pixels below the HUD also match every RGBA byte, "
                 "including the 4675-pixel sling/bird rectangle.");
        }
        if (hud) {
            puts("All 137600 pixels outside the upper-left button area match "
                 "every RGBA byte, including the 9000-pixel score HUD region.");
        }
        if (idle) {
            puts("All 153600 RGBA pixels in the fixed idle frame match the "
                 "original, including all three buttons; no regions excluded.");
        }
    } else {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_buffer_free(&gles.bytes);
    ab_buffer_free(&native.bytes);
    return okay ? 0 : 1;
}
