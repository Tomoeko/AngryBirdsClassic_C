#ifndef AB_FONT_H
#define AB_FONT_H

#include "ab/base.h"

typedef struct {
    uint32_t code;
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    int16_t pivot_y;
} AbFontGlyph;

typedef struct {
    uint16_t version;
    AbSpan texture;
    int16_t leading;
    int16_t tracking;
    int32_t max_ascending;
    int32_t max_descending;
    AbFontGlyph *glyphs;
    size_t count;
} AbBitmapFont;

/* Owns the glyph array; texture borrows input. Output starts empty. */
bool ab_font_read(AbSpan input, size_t allocation_limit, AbBitmapFont *output,
                  AbError *error);
const AbFontGlyph *ab_font_glyph(const AbBitmapFont *font, uint32_t code);
void ab_font_free(AbBitmapFont *font);

#endif
