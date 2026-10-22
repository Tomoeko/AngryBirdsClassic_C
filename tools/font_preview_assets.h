#ifndef AB_FONT_PREVIEW_ASSETS_H
#define AB_FONT_PREVIEW_ASSETS_H

#include "ab/font_draw.h"
#include "ab/hash.h"
#include "ab/utf8.h"
#include "ab/zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PREVIEW_WIDTH = 480, PREVIEW_HEIGHT = 320, TEXT_LIMIT = 512 };

static bool pinned_archive(const char *path, AbBuffer *bytes, AbZip *zip,
                           AbError *error) {
    uint8_t digest[32];
    char hash[65];
    if (!ab_file_read(path, 128 * 1024 * 1024, bytes, error) ||
        !ab_sha256((AbSpan){bytes->data, bytes->size}, digest, error)) {
        return false;
    }
    ab_digest_hex(digest, hash);
    if (strcmp(hash,
               "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad") !=
        0) {
        return ab_fail(error, "Preview requires the pinned Android 5.2.0 APK");
    }
    return ab_zip_open((AbSpan){bytes->data, bytes->size}, zip, error);
}

static bool read_entry(const AbZip *zip, const char *path, AbBuffer *output,
                       AbError *error) {
    const AbZipEntry *entry = ab_zip_find(zip, path);
    if (!entry) {
        return ab_fail(error, "Preview archive entry is missing");
    }
    return ab_zip_read(zip, (size_t)(entry - zip->entries), 32 * 1024 * 1024, output,
                       error);
}

static bool metadata_image(const AbZip *zip, const char *font_path, AbSpan texture,
                           AbBuffer *encoded, AbImage *image, AbError *error) {
    const char *slash = strrchr(font_path, '/');
    size_t prefix = slash ? (size_t)(slash - font_path) + 1 : 0;
    char path[1024];
    if (!texture.size || texture.size >= sizeof(path) ||
        prefix > sizeof(path) - 1 - texture.size ||
        memchr(texture.data, 0, texture.size)) {
        return ab_fail(error, "Invalid preview texture path");
    }
    memcpy(path, font_path, prefix);
    memcpy(path + prefix, texture.data, texture.size);
    path[prefix + texture.size] = 0;
    if (!read_entry(zip, path, encoded, error)) {
        return false;
    }
    AbSpan bytes = {encoded->data, encoded->size};
    if (texture.size >= 4 && memcmp(texture.data + texture.size - 4, ".pvr", 4) == 0) {
        return ab_pvr_decode(bytes, 64 * 1024 * 1024, image, error);
    }
    if (texture.size >= 4 && memcmp(texture.data + texture.size - 4, ".png", 4) == 0) {
        return ab_png_decode(bytes, 64 * 1024 * 1024, image, error);
    }
    return ab_fail(error, "Preview texture format is unsupported");
}

static bool build_text(const AbBitmapFont *font, const AbImage *image, const char *text,
                       bool native_rectangles, AbDrawList *list, AbError *error) {
    uint32_t codes[TEXT_LIMIT];
    AbFontGlyphPlacement layout[TEXT_LIMIT];
    size_t byte_count = strlen(text);
    size_t length;
    if (byte_count > TEXT_LIMIT * 4 ||
        !ab_utf8_to_codes((AbSpan){(const uint8_t *)text, byte_count}, codes,
                          TEXT_LIMIT, &length)) {
        return ab_fail(error, "Preview text exceeds bounded capacity");
    }
    AbFontPlacement placement = {.placement = {.x = PREVIEW_WIDTH / 2,
                                               .y = PREVIEW_HEIGHT / 2,
                                               .scale_x = 1,
                                               .scale_y = 1,
                                               .vertical = AB_ANCHOR_VCENTER,
                                               .horizontal = AB_ANCHOR_HCENTER}};
    size_t count;
    if (!ab_font_layout_codes(font, codes, length, 0, length, placement, layout,
                              TEXT_LIMIT, &count)) {
        return ab_fail(error, "Cannot lay out preview text");
    }
    if (native_rectangles) {
        AbFontNativeDrawStyle style = {.texture = 1,
                                       .texture_width = image->width,
                                       .texture_height = image->height,
                                       .viewport_width = PREVIEW_WIDTH,
                                       .viewport_height = PREVIEW_HEIGHT,
                                       .glyph_scale_x = 1,
                                       .glyph_scale_y = 1,
                                       .state = {.scale_x = 1, .scale_y = 1},
                                       .blend = AB_BLEND_PREMULTIPLIED,
                                       .color = {1, 1, 1, 1},
                                       .opacity = 1};
        return ab_font_draw_native(layout, count, style, list, error);
    }
    AbFontDrawStyle style = {.texture = 1,
                             .texture_width = image->width,
                             .texture_height = image->height,
                             .scale_x = 1,
                             .scale_y = 1,
                             .transform = {.xx = 1, .yy = 1},
                             .blend = AB_BLEND_PREMULTIPLIED,
                             .color = {1, 1, 1, 1},
                             .opacity = 1};
    return ab_font_draw_unrotated(layout, count, style, list, error);
}

static bool save_ppm(const char *path, const AbImage *image, AbError *error) {
    char header[80];
    int header_size = snprintf(header, sizeof(header), "P6\n%u %u\n255\n", image->width,
                               image->height);
    if (header_size <= 0 || (size_t)header_size >= sizeof(header) || !image->width ||
        !image->height || !image->rgba.data ||
        (size_t)image->width > (64 * 1024 * 1024) / 4 / image->height ||
        image->rgba.size != (size_t)image->width * image->height * 4) {
        return ab_fail(error, "Invalid preview capture dimensions");
    }
    size_t size = (size_t)header_size + (size_t)image->width * image->height * 3;
    uint8_t *bytes = malloc(size);
    if (!bytes) {
        return ab_fail(error, "Cannot allocate preview image output");
    }
    memcpy(bytes, header, (size_t)header_size);
    uint8_t *destination = bytes + header_size;
    for (size_t pixel = 0; pixel < image->rgba.size / 4; ++pixel) {
        memcpy(destination + pixel * 3, image->rgba.data + pixel * 4, 3);
    }
    bool okay = ab_file_write(path, (AbSpan){bytes, size}, error);
    free(bytes);
    return okay;
}

#endif
