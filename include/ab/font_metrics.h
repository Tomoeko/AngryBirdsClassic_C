#ifndef AB_FONT_METRICS_H
#define AB_FONT_METRICS_H

#include "ab/font.h"

typedef struct {
    int32_t width;
    int32_t height;
    int32_t font_height;
} AbFontMeasure;

/* Native BitmapFont UTF-32 range measurement. Clamp start to length, then count
 * to the remaining range. Known glyph widths/heights use signed 16-bit
 * Sprite dimensions, including negative space heights. Missing glyphs add
 * leading / 3 to width and nothing to measured height.
 * Height is the greatest of zero and known glyph heights. Add tracking * (count - 1)
 * after width accumulation, even for an empty range in a nonempty string; a wholly
 * empty string returns zero width/height. Newlines receive no special treatment.
 *
 * The caller owns font and codes. A zero length permits NULL codes. Length,
 * start and count must fit the native nonnegative int32 domain; invalid font
 * storage or arithmetic outside int32 range rejects without changing output.
 * UTF-8 decoding, glyph placement and renderer submission are separate. */
bool ab_font_measure_codes(const AbBitmapFont *font, const uint32_t *codes,
                           size_t length, size_t start, size_t count,
                           AbFontMeasure *output);

#endif
