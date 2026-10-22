#ifndef AB_THEME_SPRITE_H
#define AB_THEME_SPRITE_H

#include "ab/composite.h"
#include "ab/physics_geometry.h"
#include "ab/sprite.h"

/* DrawUtils tinted ordinary-Sprite image vertices, before context/GPU transform.
 * Sprite pivot getters add 0.5 then truncate, including negative pivots. Preserve
 * the native matrix's zero products and operation order. Region is borrowed;
 * output is unchanged on invalid/nonfinite geometry. No allocation. */
bool ab_theme_sprite_vertices(const AbSpriteRegion *region, AbVec2 position,
                              AbVec2 scale, AbVec2 output[4], AbError *error);

typedef struct {
    void *context;
    bool (*lookup)(void *context, AbSpan sprite, const AbSpriteRegion **output,
                   AbError *error);
    bool (*image)(void *context, const AbSpriteRegion *region, const AbVec2 vertices[4],
                  const float tint[4], AbError *error);
} AbThemeCompositeBoundaries;

/* DrawUtils' theme-specific composite route visits stored part order and adds
 * each part's x/y before ordinary sprite pivot/matrix handling. This route does
 * not apply part scale, rotation or flip fields. Providers resolve each part's
 * ordinary sprite and submit its image; lookup policy/ownership remain explicit.
 * All objects/spans borrowed, no allocation. Empty composites make no calls.
 * Failure retains earlier callbacks. Composite loading/bounds and GPU context
 * transformation are separate. */
bool ab_theme_composite_draw(const AbComposite *composite, AbVec2 position,
                             AbVec2 scale, const float tint[4],
                             const AbThemeCompositeBoundaries *boundaries,
                             AbError *error);

#endif
