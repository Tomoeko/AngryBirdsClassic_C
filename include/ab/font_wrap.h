#ifndef AB_FONT_WRAP_H
#define AB_FONT_WRAP_H

#include "ab/font_metrics.h"

typedef struct {
    size_t start, count;
    int32_t width;
} AbFontWrappedLine;

typedef struct {
    AbFontWrappedLine *lines;
    size_t count, capacity;
    int32_t widest_line;
} AbFontWrap;

/* Native clipText's decoded UTF-32 line selection with explicit current font.
 * Break at LF, ASCII space, hyphen and U+200B. Hyphens stay in their candidate;
 * skip spaces/U+200B between candidates and after the last word. Preserve
 * leading/embedded whitespace, hard breaks and strict candidate width < limit.
 * Overlong first words split before the first overflowing character. Widest
 * line starts at zero even when measured empty-line widths are negative.
 * Width <=0 returns one empty line without inspecting font or text; a positive
 * width and empty text returns no lines. Positive infinity is supported.
 * NaN, insufficient capacity, invalid metrics/storage and nonprogressing
 * first-character splits fail. The latter is an explicit bounded boundary;
 * unsafe original tiny-width/invalid-font loops are not reproduced.
 * Font/codes remain borrowed; output slices index codes. No allocation, UTF
 * conversion, localization or class construction. On failure count/widest are
 * preserved; line storage may contain partial output. Same int32 measurement
 * domain as ab_font_measure_codes. */
bool ab_font_wrap_codes(const AbBitmapFont *font, const uint32_t *codes, size_t length,
                        float width_limit, AbFontWrap *output, AbError *error);

#endif
