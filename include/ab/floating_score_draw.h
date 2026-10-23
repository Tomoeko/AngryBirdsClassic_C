#ifndef AB_FLOATING_SCORE_DRAW_H
#define AB_FLOATING_SCORE_DRAW_H

#include "ab/coordinates.h"
#include "ab/floating_scores.h"
#include "ab/font_draw.h"
#include "ab/lua_asset_sprite.h"
#include "ab/text_resources.h"

typedef struct {
    AbVec2 screen, text_position, sprite_position;
    float scale;
    bool visible;
} AbFloatingScoreDrawPlan;

/* GameHud.draw44365's floating-score position/scale boundary. Physics positions
 * use the recovered multiply-by-twenty conversion; camera zoom.x supplies both
 * axes. Scaling.getScaleNum multiplies xs after the screen position conversion.
 * Sprite coordinates floor after division by that scale; text does not floor.
 * Zero-scale endpoints produce no pixels and skip division. Finite used values
 * are required. No timer advancement, resource lookup or allocation. */
bool ab_floating_score_draw_plan(AbVec2 physics_position, float xs,
                                 AbCoordinateCamera camera, float ui_scale,
                                 AbFloatingScoreDrawPlan *output, AbError *error);

typedef struct {
    const AbBitmapFont *font;
    uint32_t texture, texture_width, texture_height;
    const AbTextResources *strings;
    const AbSpriteProvider *sprites;
    uint32_t *codes;
    AbFontGlyphPlacement *glyphs;
    size_t capacity;
} AbFloatingScoreDrawResources;

/* Draw the current dense score list in order, text then optional sprite, using
 * the actual FONT_SCORE atlas and TEXTS_BASIC lookup. Both use BOTTOM/HCENTER
 * anchors and the source scale animation. Borrow resources and reusable glyph
 * storage, with no frame allocation, decoding or upload. Commands are shared
 * by Metal/GLES2. Failure can retain earlier records' commands; stop the frame. */
bool ab_floating_scores_draw(AbFloatingScores *scores,
                             const AbFloatingScoreDrawResources *resources,
                             AbCoordinateCamera camera, float ui_scale, uint32_t width,
                             uint32_t height, AbDrawList *list, AbError *error);

#endif
