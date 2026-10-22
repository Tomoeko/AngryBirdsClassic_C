#ifndef AB_FONT_DRAW_H
#define AB_FONT_DRAW_H

#include "ab/font_layout.h"
#include "ab/image_rectangle.h"

typedef struct {
    uint32_t texture;
    uint32_t texture_width;
    uint32_t texture_height;
    float scale_x;
    float scale_y;
    AbTransform2 transform;
    AbBlend blend;
    float color[4];
    float opacity;
} AbFontDrawStyle;

/* Append known-glyph quads in layout order without allocating. Uses original
 * Sprite pivot anchors and raw atlas coordinates. The explicit caller affine
 * transform follows local glyph geometry; this is not a complete native
 * RenderState2D rotation/clip/effect implementation. Layout rotation origins
 * are unused by this unrotated path. On failure, restores the list's count. */
bool ab_font_draw_unrotated(const AbFontGlyphPlacement *glyphs, size_t count,
                            AbFontDrawStyle style, AbDrawList *list, AbError *error);

typedef struct {
    uint32_t texture, texture_width, texture_height;
    uint32_t viewport_width, viewport_height;
    float glyph_scale_x, glyph_scale_y;
    AbSpriteRenderState state;
    AbBlend blend;
    float color[4], opacity;
} AbFontNativeDrawStyle;

/* Original font Sprite/Image rectangle composition into backend-neutral clip
 * quads. Apply each glyph's temporary rotation origin, rounded FONT pivot,
 * explicit glyph scales and caller render state. Preserve glyph order and
 * native rectangle culling. Caller supplies font atlas/texture, viewport,
 * blend/color/opacity and stable finite glyph layouts. No allocation, shader
 * selection, clipping effects or GPU sampling. state rotation-origin fields
 * are superseded by each placement's origin. Failure restores list.count;
 * storage beyond that count may change. */
bool ab_font_draw_native(const AbFontGlyphPlacement *glyphs, size_t count,
                         AbFontNativeDrawStyle style, AbDrawList *list, AbError *error);

#endif
