#ifndef AB_THEME_BACKGROUND_H
#define AB_THEME_BACKGROUND_H

#include "ab/theme_layer.h"

typedef struct {
    float color[3];
    const uintptr_t *layers;
    size_t count;
} AbThemeBackground;

typedef struct {
    uint32_t *background_argb;
    AbThemeLayerRenderState *render;
} AbThemeBackgroundState;

typedef bool (*AbThemeLayerDraw)(void *context, uintptr_t layer, uint32_t filter,
                                 AbError *error);

/* GameLua background color: clamp finite RGB channels to0..255, truncate, store
 * opaque ARGB. Failure preserves output; no allocation. */
bool ab_background_color_rgb(const float rgb[3], uint32_t *output, AbError *error);

/* ThemeComposition background path with no spotlight instance. Set background
 * before visiting the stable borrowed layer vector in order, forwarding filter
 * unchanged. Completed traversal resets render alpha to1, including empty lists.
 * Layer handles must be nonzero; draw may be NULL for an empty list. Caller owns
 * all data/state/provider lifetimes. No allocation. Failure preserves preceding
 * color/state/callback writes and does not perform the final alpha reset.
 * Spotlight/effects, layer construction/drawing and GPU output are separate. */
bool ab_theme_background_draw(const AbThemeBackground *background,
                              AbThemeBackgroundState state, uint32_t filter,
                              AbThemeLayerDraw draw, void *context, AbError *error);

#endif
