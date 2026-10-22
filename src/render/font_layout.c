#include "ab/font_layout.h"

#include <limits.h>
#include <math.h>

static int32_t half_down(int32_t value) {
    int64_t wide = value;
    return (int32_t)(wide >= 0 ? wide / 2 : -((-wide + 1) / 2));
}

static bool finite_placement(AbFontPlacement value) {
    return isfinite(value.placement.x) && isfinite(value.placement.y) &&
           isfinite(value.placement.scale_x) && isfinite(value.placement.scale_y) &&
           isfinite(value.rotation_origin_x) && isfinite(value.rotation_origin_y);
}

bool ab_font_layout_codes(const AbBitmapFont *font, const uint32_t *codes,
                          size_t length, size_t start, size_t count,
                          AbFontPlacement placement, AbFontGlyphPlacement *output,
                          size_t capacity, size_t *output_count) {
    if (!font || !output_count || (length && !codes) ||
        (font->count && !font->glyphs) || (capacity && !output) || length > INT32_MAX ||
        start > length || count > length - start || !finite_placement(placement) ||
        (unsigned)placement.placement.vertical > AB_ANCHOR_VPIVOT ||
        (unsigned)placement.placement.horizontal > AB_ANCHOR_HPIVOT) {
        return false;
    }
    AbFontMeasure measure;
    if (!ab_font_measure_codes(font, codes, length, start, count, &measure)) {
        return false;
    }
    float x = placement.placement.x;
    float y = placement.placement.y;
    float scale_x = placement.placement.scale_x;
    float scale_y = placement.placement.scale_y;
    float origin_x = x + placement.rotation_origin_x;
    float origin_y = y + placement.rotation_origin_y;
    switch (placement.placement.vertical) {
        case AB_ANCHOR_TOP:
            y += (float)font->max_ascending * scale_y;
            break;
        case AB_ANCHOR_VCENTER: {
            int64_t offset =
                (int64_t)font->max_ascending - half_down(measure.font_height);
            if (offset < INT32_MIN || offset > INT32_MAX) {
                return false;
            }
            y += (float)(int32_t)offset * scale_y;
            break;
        }
        case AB_ANCHOR_BOTTOM:
            y -= (float)font->max_descending * scale_y;
            break;
        default:
            break;
    }
    if (placement.placement.horizontal == AB_ANCHOR_HCENTER) {
        x -= (float)half_down(measure.width) * scale_x;
    } else if (placement.placement.horizontal == AB_ANCHOR_RIGHT) {
        x -= (float)measure.width * scale_x;
    }
    if (!isfinite(x) || !isfinite(y) || !isfinite(origin_x) || !isfinite(origin_y)) {
        return false;
    }
    size_t written = 0;
    for (size_t index = start; index < start + count; ++index) {
        const AbFontGlyph *glyph = ab_font_glyph(font, codes[index]);
        if (!glyph) {
            continue;
        }
        if (written == capacity) {
            return false;
        }
        /* FONT creates each Sprite with zero X pivot and a signed integer Y
         * pivot. Sprite getters add 0.5 and truncate before these subtractions. */
        int32_t pivot_y = (int32_t)((float)glyph->pivot_y + 0.5f);
        float offset_x = x - 0.0f;
        float offset_y = y - (float)pivot_y;
        AbFontGlyphPlacement item = {.glyph = glyph,
                                     .character = index,
                                     .x = x,
                                     .y = y,
                                     .rotation_origin_x = origin_x - offset_x,
                                     .rotation_origin_y = origin_y - offset_y};
        if (!isfinite(item.rotation_origin_x) || !isfinite(item.rotation_origin_y)) {
            return false;
        }
        output[written++] = item;
        int32_t advance = (int32_t)glyph->width + font->tracking;
        x += (float)advance * scale_x;
        if (!isfinite(x)) {
            return false;
        }
    }
    *output_count = written;
    return true;
}
