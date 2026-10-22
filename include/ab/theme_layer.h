#ifndef AB_THEME_LAYER_H
#define AB_THEME_LAYER_H

#include "ab/coordinates.h"
#include "ab/image_rectangle.h"
#include "ab/theme_sprite.h"

#include <stdint.h>

typedef struct {
    AbCoordinateCamera camera;
    int32_t viewport_width;
    float parallax;
} AbThemeLayerTransform;

/* Native ThemeLayer horizontal parallax transforms. Compute transformed center
 * and full-width distance independently, retaining camera-translation rounding.
 * Camera y fields are unused. Positive viewport width, finite used scalars and
 * nonzero zoom.x are required. Inverse also requires a nonzero rounded world
 * width. No allocation; unsupported/nonfinite results preserve output. */
bool ab_theme_screen_to_layer_x(const AbThemeLayerTransform *transform, float screen_x,
                                float *output);
bool ab_theme_layer_to_screen_x(const AbThemeLayerTransform *transform, float layer_x,
                                float *output);

typedef struct {
    float width, pivot_x, scale, x_offset;
    float speed, phase, vertical_offset;
    float color[4];
    bool repeats;
} AbThemeLayerBase;

typedef struct {
    AbSpriteRenderState geometry;
    float alpha;
} AbThemeLayerRenderState;

typedef bool (*AbThemeSpriteDraw)(void *context, AbVec2 position, AbVec2 scale,
                                  const float tint[4], AbError *error);

/* Base ThemeLayer sprite placement in native call order. The callback receives
 * DrawUtils arguments before sprite pivot subtraction/matrix transformation.
 * Width must be positive; layer scale and camera zoom.x must be nonzero. All
 * used numbers/intermediates must be finite. Time is unused when speed and phase
 * are zero. Explicit draw_limit bounds repeated draws; no allocation. Retain
 * render-state writes and completed callbacks on failure. Caller owns sprite
 * selection/lifetime. Overlays, composition and GPU submission are separate. */
bool ab_theme_layer_draw_base(const AbThemeLayerBase *layer,
                              const AbThemeLayerTransform *transform, float time,
                              AbThemeLayerRenderState *state, size_t draw_limit,
                              AbThemeSpriteDraw draw, void *context, AbError *error);

#endif
