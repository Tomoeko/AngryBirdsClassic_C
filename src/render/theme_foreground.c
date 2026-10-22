#include "ab/theme_foreground.h"

#include <math.h>

static bool ground_plan(const AbThemeForeground *foreground,
                        const AbThemeForegroundInput *input,
                        AbGameRectanglePlan *output, AbError *error) {
    float units = 0, scale = 1;
    if (foreground->count && foreground->first.is_base_layer) {
        float difference = foreground->first.height - foreground->first.pivot_y;
        if (!isfinite(foreground->first.height) ||
            !isfinite(foreground->first.pivot_y) || !isfinite(difference) ||
            difference < -0x1p31f || difference >= 0x1p31f ||
            !isfinite(foreground->first.scale)) {
            return ab_fail(error, "Unsupported first foreground layer metrics");
        }
        units = (float)(int32_t)difference;
        scale = foreground->first.scale;
    }
    float top = input->camera.top_left.y - units * scale;
    float ground = -(top * input->camera.zoom.y);
    ground = floorf(ground);
    AbGameRectangle rectangle = {.color = {foreground->ground_color[0],
                                           foreground->ground_color[1],
                                           foreground->ground_color[2], 1},
                                 .left = 0,
                                 .top = ground,
                                 .right = (float)input->viewport_width,
                                 .bottom = (float)input->viewport_height + 1,
                                 .respect_state = false};
    return ab_game_rectangle_plan(&rectangle, output, error);
}

bool ab_theme_foreground_draw(const AbThemeForeground *foreground,
                              const AbThemeForegroundInput *input,
                              AbThemeLayerRenderState *render,
                              const AbThemeForegroundBoundaries *boundaries,
                              AbError *error) {
    if (!foreground || !input || !render || !boundaries || !boundaries->fill ||
        input->viewport_width <= 0 || input->viewport_height <= 0 ||
        !isfinite(input->camera.top_left.y) || !isfinite(input->camera.zoom.y) ||
        (foreground->count && (!foreground->layers || !boundaries->layer))) {
        return ab_fail(error, "Invalid foreground composition state/providers");
    }
    AbGameRectanglePlan plan;
    if (!ground_plan(foreground, input, &plan, error) ||
        !boundaries->fill(boundaries->context, &plan, error)) {
        return false;
    }
    for (size_t index = 0; index < foreground->count; ++index) {
        uintptr_t layer = foreground->layers[index];
        if (!layer) {
            return ab_fail(error, "Invalid foreground layer handle");
        }
        if (!boundaries->layer(boundaries->context, layer, input->filter, error)) {
            return false;
        }
    }
    render->alpha = 1;
    return true;
}
