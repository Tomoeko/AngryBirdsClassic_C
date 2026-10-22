#include "ab/font.h"

#include "ab/dat.h"

#include <stdlib.h>
#include <string.h>

static bool read_glyph(AbDatReader *reader, uint16_t version, AbFontGlyph *glyph,
                       AbError *error) {
    if (version == 1) {
        uint16_t code;
        if (!ab_dat_u16(reader, &code, error)) {
            return false;
        }
        glyph->code = code;
    } else if (!ab_dat_u32(reader, &glyph->code, error)) {
        return false;
    }
    return ab_dat_i16(reader, &glyph->x, error) &&
           ab_dat_i16(reader, &glyph->y, error) &&
           ab_dat_i16(reader, &glyph->width, error) &&
           ab_dat_i16(reader, &glyph->height, error) &&
           ab_dat_i16(reader, &glyph->pivot_y, error);
}

static bool read_glyphs(AbSpan payload, size_t allocation_limit, AbBitmapFont *font,
                        AbError *error) {
    AbDatReader reader = {payload, 0};
    if (!ab_dat_u16(&reader, &font->version, error) ||
        (font->version != 1 && font->version != 2)) {
        return ab_fail(error, "Unsupported FONT version");
    }
    int16_t count;
    if (!ab_dat_string(&reader, &font->texture, error) ||
        !ab_dat_i16(&reader, &font->leading, error) ||
        !ab_dat_i16(&reader, &font->tracking, error) ||
        !ab_dat_i16(&reader, &count, error) || count < 0) {
        return ab_fail(error, "Invalid FONT header");
    }
    size_t glyph_count = (size_t)count;
    size_t record_size = font->version == 1 ? 12 : 14;
    if (glyph_count > allocation_limit / sizeof(*font->glyphs) ||
        glyph_count > (payload.size - reader.offset) / record_size) {
        return ab_fail(error, "FONT glyphs exceed allocation or input bounds");
    }
    if (glyph_count) {
        font->glyphs = calloc(glyph_count, sizeof(*font->glyphs));
        if (!font->glyphs) {
            return ab_fail(error, "Cannot allocate FONT glyphs");
        }
    }
    font->count = glyph_count;
    for (size_t index = 0; index < glyph_count; ++index) {
        AbFontGlyph *glyph = font->glyphs + index;
        if (!read_glyph(&reader, font->version, glyph, error)) {
            return false;
        }
        if (glyph->pivot_y > font->max_ascending) {
            font->max_ascending = glyph->pivot_y;
        }
        int32_t descending = (int32_t)glyph->height - glyph->pivot_y;
        if (descending > font->max_descending) {
            font->max_descending = descending;
        }
    }
    return reader.offset == payload.size || ab_fail(error, "Trailing FONT data");
}

bool ab_font_read(AbSpan input, size_t allocation_limit, AbBitmapFont *output,
                  AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing font output");
    }
    *output = (AbBitmapFont){0};
    AbDatReader reader;
    if (!ab_dat_open(input, "KA3D", &reader, error)) {
        return false;
    }
    AbBitmapFont font = {0};
    bool found = false;
    while (reader.offset < reader.bytes.size) {
        AbDatChunk chunk;
        if (!ab_dat_next(&reader, &chunk, error)) {
            goto release_font;
        }
        if (memcmp(chunk.tag.data, "FONT", 4) != 0) {
            continue;
        }
        if (found) {
            ab_fail(error, "Multiple FONT chunks are unsupported");
            goto release_font;
        }
        if (!read_glyphs(chunk.payload, allocation_limit, &font, error)) {
            goto release_font;
        }
        found = true;
    }
    if (!found) {
        ab_fail(error, "DAT file has no FONT chunk");
        goto release_font;
    }
    *output = font;
    return true;

release_font:
    ab_font_free(&font);
    return false;
}

const AbFontGlyph *ab_font_glyph(const AbBitmapFont *font, uint32_t code) {
    if (!font) {
        return NULL;
    }
    for (size_t remaining = font->count; remaining; --remaining) {
        const AbFontGlyph *glyph = font->glyphs + remaining - 1;
        if (glyph->code == code) {
            return glyph;
        }
    }
    return NULL;
}

void ab_font_free(AbBitmapFont *font) {
    if (font) {
        free(font->glyphs);
        *font = (AbBitmapFont){0};
    }
}
