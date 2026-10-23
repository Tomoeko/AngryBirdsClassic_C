#ifndef AB_UI_IMAGE_RENDER_H
#define AB_UI_IMAGE_RENDER_H

#include "ab/image_rectangle.h"
#include "ab/composite_resources.h"
#include "ab/ui_image_draw.h"
#include "ab/screen_clip.h"
#include "ab/sprite_atlas.h"

typedef struct {
    AbSpriteRenderState geometry;
    float alpha;
    AbDrawClip clip;
} AbUiImageRenderState;

typedef struct {
    const AbSpriteRegion *region;
    /* A successful empty region/composite/texture binding means absent sprite:
     * native drawing appends no command. Provider errors still fail. */
    uint32_t texture, texture_width, texture_height;
    AbSpriteAlpha alpha;
    AbResolvedComposite composite;
} AbUiImageRenderBinding;

typedef struct {
    void *context;
    bool (*lookup)(void *context, AbSpan image, AbUiImageRenderBinding *output,
                   AbError *error);
    uint32_t viewport_width, viewport_height;
    AbBlend blend;
    float color[4];
    bool (*part)(void *context, const AbSpriteBinding *sprite,
                 AbUiImageRenderBinding *output, AbError *error);
} AbUiImageRenderBoundaries;

typedef struct {
    AbSpan image;
    AbVec2 position, size;
    AbVerticalAnchor vertical;
    AbHorizontalAnchor horizontal;
    bool sized;
} AbUiSpriteRender;

/* Draw one resource sprite with natural or explicitly supplied size,
 * preserving its native anchors, current geometry and alpha. No state reset,
 * resource allocation or empty-identifier policy is applied here. KA3D/RVIO
 * composites use cached part bindings and ignore explicit width/height,
 * as Resources.drawSprite does. Composite contexts support finite signed scale
 * and native rotation, including transformed RVIO parts. Direct composite
 * context states reject before drawing. Ordinary sprites retain their existing
 * transform support. */
bool ab_ui_sprite_render(const AbUiSpriteRender *sprite,
                         const AbUiImageRenderState *state,
                         const AbUiImageRenderBoundaries *boundaries, AbDrawList *list,
                         AbError *error);

/* Apply an Image draw plan to explicit render context and shared clip quads.
 * Seven-argument state calls preserve alpha; eight-argument calls replace it.
 * Preserve direct-vertex mode and caller blend/color policy. Sprite lookup runs
 * after its preceding state writes, then explicitly-sized native Sprite anchors
 * and Image rectangle geometry compose in order. Only visible quads append;
 * culled/empty plans leave the context alone. All used numbers must be finite
 * and within the rotation module's domain. A collapsed scale may retain the
 * source's nonfinite divided translation; preserve lookup/reset order without
 * submitting singular zero-area geometry. Stable borrowed bindings, no allocation
 * or GPU calls.
 * On failure retain prior state writes, callbacks and appended commands. Shader
 * selection, effects, live resource lookup and GPU sampling are separate. */
bool ab_ui_image_render_plan(const AbUiImageDrawPlan *plan, AbUiImageRenderState *state,
                             const AbUiImageRenderBoundaries *boundaries,
                             AbDrawList *list, AbError *error);

#endif
