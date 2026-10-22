#ifndef AB_FONT_ASSET_H
#define AB_FONT_ASSET_H

#include "ab/font.h"
#include "ab/image.h"
#include "ab/zip.h"

typedef struct {
    AbBuffer metadata;
    AbBitmapFont font;
    AbImage image;
    bool has_alpha;
} AbFontAsset;

/* Decode one explicitly selected APK FONT DAT and its relative texture. Owns
 * metadata, glyphs and RGBA until close; the archive is borrowed only during
 * open. Texture references must be basenames. The retained allocation limit is
 * shared by metadata/glyphs/pixels, with a separate per-entry temporary limit.
 * No font/profile/locale selection or GPU ownership is implied. Fresh output
 * starts empty, including on failure. */
bool ab_font_asset_open(const AbZip *archive, const char *metadata_path,
                        size_t allocation_limit, size_t entry_limit,
                        AbFontAsset *output, AbError *error);
void ab_font_asset_close(AbFontAsset *asset);

#endif
