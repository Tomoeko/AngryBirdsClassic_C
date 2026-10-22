#ifndef AB_THEME_LAYER_DATA_H
#define AB_THEME_LAYER_DATA_H

#include "ab/lua_data.h"
#include "ab/theme_layer.h"

typedef struct {
    AbSpan sprite;
    bool has_base_sprite;
    float height, pivot_y, parallax;
    AbThemeLayerBase base;
} AbThemeLayerDefinition;

typedef struct {
    bool present;
    int32_t width, height, pivot_x, pivot_y;
} AbThemeSpriteMetrics;

/* Static ordinary ThemeLayer constructor, with explicitly resolved metadata.
 * Sprite name is tuple[2], parallax/scale are [3]/[4], repeats uses BOOLEAN [5]
 * only, [6] sets both x_offset and animation phase, [7] is vertical offset and
 * named "v" is speed. Numeric defaults use NUMBER only; strings in numeric
 * fields are explicitly unsupported because native numeric coercion is outside
 * this contract. Other nonnumeric kinds use native defaults. A NULL region
 * means absent sprite, with zero dimensions/pivots. Output borrows name bytes;
 * no allocation. Failure preserves output. Composite sprites, native lookup
 * ownership and overlays are separate. */
bool ab_theme_layer_definition(const AbDataTable *table, const AbSpriteRegion *region,
                               AbThemeLayerDefinition *output, AbError *error);

/* Same static constructor with resolved native integer getter results, including
 * composite bounds. Metrics are used only when present; NULL means absent.
 * No extra pivot rounding occurs after integer resource getters. */
bool ab_theme_layer_definition_metrics(const AbDataTable *table,
                                       const AbThemeSpriteMetrics *metrics,
                                       AbThemeLayerDefinition *output, AbError *error);

#endif
