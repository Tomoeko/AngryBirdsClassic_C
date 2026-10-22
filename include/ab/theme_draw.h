#ifndef AB_THEME_DRAW_H
#define AB_THEME_DRAW_H

#include "ab/draw.h"
#include "ab/sprite_provider.h"
#include "ab/theme_layers.h"

typedef struct {
    const AbThemeLayers *layers;
    const AbSpriteProvider *provider;
    AbThemeLayerRenderState *render;
    uint32_t *background_argb;
    uint32_t white_texture;
    size_t placement_limit;
} AbThemeDrawConfig;

typedef struct {
    AbCoordinateCamera camera;
    uint32_t width, height;
    float time;
    uint32_t filter;
} AbThemeDrawInput;

typedef struct {
    AbThemeDrawConfig config;
    bool initialized, calling, failed;
} AbThemeDraw;

/* Actual cached base ThemeLayer background/foreground command consumers.
 * Borrow all metadata, immutable sprite/texture bindings, current render state,
 * background color and prepared white texture. No loading or frame allocation.
 * Ordinary/composite base layers use the target's default point-image shaders,
 * which ignore the supplied per-vertex tint. Unknown source texture alpha,
 * changed sprite identities and missing bindings reject when reached.
 *
 * This inherits ThemeLayers' base-layer/no-Spotlight domain. Specialized layers,
 * dynamic effects/shader overrides, complete drawGame ordering and GUI remain
 * separate. State and preceding commands survive a late failure; stop the owner.
 */
bool ab_theme_draw_initialize(AbThemeDrawConfig config, AbThemeDraw *output,
                              AbError *error);
bool ab_theme_draw_background(AbThemeDraw *draw, AbThemeDrawInput input,
                              AbDrawList *list, AbError *error);
bool ab_theme_draw_foreground(AbThemeDraw *draw, AbThemeDrawInput input,
                              AbDrawList *list, AbError *error);

#endif
