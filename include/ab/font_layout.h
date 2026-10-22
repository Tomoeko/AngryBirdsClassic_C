#ifndef AB_FONT_LAYOUT_H
#define AB_FONT_LAYOUT_H

#include "ab/draw.h"
#include "ab/font_metrics.h"

typedef struct {
    AbSpritePlacement placement;
    float rotation_origin_x;
    float rotation_origin_y;
} AbFontPlacement;

typedef struct {
    const AbFontGlyph *glyph;
    size_t character;
    /* Original drawString glyph-callback position and temporary context rotation
     * origin. */
    float x;
    float y;
    float rotation_origin_x;
    float rotation_origin_y;
} AbFontGlyphPlacement;

/* Bounded native UTF-32 drawString placement. The selected range must be within
 * the supplied text. Missing glyphs neither draw nor advance, although they
 * contribute to the width used for alignment. Known glyphs advance by their
 * signed width plus tracking, scaled horizontally. No allocation or wrapping.
 * The caller retains the font/text and owns output storage. On rejection,
 * output_count is unchanged; output storage may contain a partial layout. */
bool ab_font_layout_codes(const AbBitmapFont *font, const uint32_t *codes,
                          size_t length, size_t start, size_t count,
                          AbFontPlacement placement, AbFontGlyphPlacement *output,
                          size_t capacity, size_t *output_count);

#endif
