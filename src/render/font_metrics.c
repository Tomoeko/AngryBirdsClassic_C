#include "ab/font_metrics.h"

#include <limits.h>

bool ab_font_measure_codes(const AbBitmapFont *font, const uint32_t *codes,
                           size_t length, size_t start, size_t count,
                           AbFontMeasure *output) {
    if (!font || !output || (length && !codes) || (font->count && !font->glyphs) ||
        length > INT32_MAX || start > INT32_MAX || count > INT32_MAX) {
        return false;
    }
    int64_t font_height = (int64_t)font->max_ascending + font->max_descending;
    if (font_height < INT32_MIN || font_height > INT32_MAX) {
        return false;
    }
    AbFontMeasure result = {.font_height = (int32_t)font_height};
    if (!length) {
        *output = result;
        return true;
    }
    if (start > length) {
        start = length;
    }
    if (count > length - start) {
        count = length - start;
    }
    int64_t width = 0;
    int32_t fallback = font->leading / 3;
    for (size_t item = 0; item < count; ++item) {
        const AbFontGlyph *glyph = ab_font_glyph(font, codes[start + item]);
        if (glyph) {
            width += glyph->width;
            if (glyph->height > result.height) {
                result.height = glyph->height;
            }
        } else {
            width += fallback;
        }
        if (width < INT32_MIN || width > INT32_MAX) {
            return false;
        }
    }
    width += (int64_t)font->tracking * ((int64_t)count - 1);
    if (width < INT32_MIN || width > INT32_MAX) {
        return false;
    }
    result.width = (int32_t)width;
    *output = result;
    return true;
}
