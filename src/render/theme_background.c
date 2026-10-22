#include "ab/theme_background.h"

#include <math.h>

static uint32_t channel(float value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return (uint32_t)value;
}

bool ab_background_color_rgb(const float rgb[3], uint32_t *output, AbError *error) {
    if (!rgb || !output || !isfinite(rgb[0]) || !isfinite(rgb[1]) ||
        !isfinite(rgb[2])) {
        return ab_fail(error, "Invalid background RGB channels");
    }
    *output = UINT32_C(0xff000000) | (channel(rgb[0]) << 16) | (channel(rgb[1]) << 8) |
              channel(rgb[2]);
    return true;
}

bool ab_theme_background_draw(const AbThemeBackground *background,
                              AbThemeBackgroundState state, uint32_t filter,
                              AbThemeLayerDraw draw, void *context, AbError *error) {
    if (!background || !state.background_argb || !state.render ||
        (background->count && (!background->layers || !draw))) {
        return ab_fail(error, "Invalid theme background state/providers");
    }
    if (!ab_background_color_rgb(background->color, state.background_argb, error)) {
        return false;
    }
    for (size_t index = 0; index < background->count; ++index) {
        uintptr_t layer = background->layers[index];
        if (!layer) {
            return ab_fail(error, "Invalid theme background layer handle");
        }
        if (!draw(context, layer, filter, error)) {
            return false;
        }
    }
    state.render->alpha = 1;
    return true;
}
