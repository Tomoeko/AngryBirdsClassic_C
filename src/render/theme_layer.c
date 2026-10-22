#include "ab/theme_layer.h"

#include <math.h>

typedef struct {
    float screen_width, world_width, center;
} HorizontalSpace;

static bool horizontal_space(const AbThemeLayerTransform *transform,
                             HorizontalSpace *output) {
    if (!transform || transform->viewport_width <= 0 ||
        !isfinite(transform->camera.top_left.x) ||
        !isfinite(transform->camera.zoom.x) || transform->camera.zoom.x == 0 ||
        !isfinite(transform->parallax)) {
        return false;
    }
    float width = (float)transform->viewport_width;
    float half = width * 0.5f;
    float center = half / transform->camera.zoom.x;
    center += transform->camera.top_left.x;
    float origin = 0.0f / transform->camera.zoom.x;
    origin += transform->camera.top_left.x;
    float endpoint = width / transform->camera.zoom.x;
    endpoint += transform->camera.top_left.x;
    float world_width = endpoint - origin;
    if (!isfinite(center) || !isfinite(origin) || !isfinite(endpoint) ||
        !isfinite(world_width)) {
        return false;
    }
    *output = (HorizontalSpace){width, world_width, center};
    return true;
}

bool ab_theme_screen_to_layer_x(const AbThemeLayerTransform *transform, float screen_x,
                                float *output) {
    HorizontalSpace space;
    if (!output || !isfinite(screen_x) || !horizontal_space(transform, &space)) {
        return false;
    }
    float fraction = screen_x / space.screen_width;
    fraction -= 0.5f;
    float center = space.center * transform->parallax;
    float offset = fraction * space.world_width;
    float result = center + offset;
    if (!isfinite(fraction) || !isfinite(center) || !isfinite(offset) ||
        !isfinite(result)) {
        return false;
    }
    *output = result;
    return true;
}

bool ab_theme_layer_to_screen_x(const AbThemeLayerTransform *transform, float layer_x,
                                float *output) {
    HorizontalSpace space;
    if (!output || !isfinite(layer_x) || !horizontal_space(transform, &space) ||
        space.world_width == 0) {
        return false;
    }
    float center = space.center * transform->parallax;
    float relative = layer_x - center;
    float fraction = relative / space.world_width;
    fraction += 0.5f;
    float result = fraction * space.screen_width;
    if (!isfinite(center) || !isfinite(relative) || !isfinite(fraction) ||
        !isfinite(result)) {
        return false;
    }
    *output = result;
    return true;
}

static bool base_valid(const AbThemeLayerBase *layer,
                       const AbThemeLayerTransform *transform, float time) {
    if (!layer || !transform || !isfinite(layer->width) || layer->width <= 0 ||
        !isfinite(layer->pivot_x) || !isfinite(layer->scale) || layer->scale == 0 ||
        !isfinite(layer->x_offset) || !isfinite(layer->speed) ||
        !isfinite(layer->phase) || !isfinite(layer->vertical_offset) ||
        !isfinite(transform->camera.top_left.y) ||
        !isfinite(transform->camera.zoom.y) ||
        ((layer->speed != 0 || layer->phase != 0) && !isfinite(time))) {
        return false;
    }
    for (size_t index = 0; index < 4; ++index) {
        if (!isfinite(layer->color[index])) {
            return false;
        }
    }
    return true;
}

static bool draw_repeated(const AbThemeLayerBase *layer,
                          const AbThemeLayerTransform *transform, double remainder,
                          float inverse_scale, float y, AbVec2 scale,
                          const float tint[4], size_t draw_limit,
                          AbThemeSpriteDraw draw, void *context, AbError *error) {
    float first = (float)remainder;
    first *= inverse_scale;
    first += layer->width;
    first = -first;
    float integral;
    float fraction = modff(first, &integral);
    float end = (float)transform->viewport_width / scale.x;
    end += layer->pivot_x;
    if (!isfinite(integral) || !isfinite(fraction) || !isfinite(end)) {
        return ab_fail(error, "Nonfinite repeated theme placement");
    }
    size_t count = 0;
    while (end > integral) {
        if (count++ == draw_limit) {
            return ab_fail(error, "Theme repeated draw limit exceeded");
        }
        AbVec2 position = {integral + fraction, y};
        if (!draw(context, position, scale, tint, error)) {
            return false;
        }
        float next = integral + layer->width;
        if (!isfinite(next) || next <= integral) {
            return ab_fail(error, "Theme repeated placement does not advance");
        }
        integral = next;
    }
    return true;
}

bool ab_theme_layer_draw_base(const AbThemeLayerBase *layer,
                              const AbThemeLayerTransform *transform, float time,
                              AbThemeLayerRenderState *state, size_t draw_limit,
                              AbThemeSpriteDraw draw, void *context, AbError *error) {
    float screen_zero;
    if (!state || !draw || !draw_limit || !base_valid(layer, transform, time) ||
        !ab_theme_screen_to_layer_x(transform, 0, &screen_zero)) {
        return ab_fail(error, "Invalid theme base draw arguments");
    }
    state->geometry.translation_x = -transform->camera.top_left.x;
    state->geometry.translation_y = -transform->camera.top_left.y;
    state->geometry.angle_radians = 0;
    state->alpha = layer->color[3];
    float phase = 0;
    float period = layer->width * layer->scale;
    if (layer->phase != 0 || layer->speed != 0) {
        float animation = layer->speed * time;
        animation += layer->phase;
        animation *= transform->parallax;
        phase = fmodf(animation, period);
    }
    float offset = phase + screen_zero;
    offset -= layer->scale * layer->x_offset;
    double remainder = fmod((double)offset, (double)period);
    AbVec2 scale = {layer->scale * transform->camera.zoom.x,
                    layer->scale * transform->camera.zoom.y};
    state->geometry.scale_x = scale.x;
    state->geometry.scale_y = scale.y;
    float inverse_scale = 1 / layer->scale;
    float y = layer->vertical_offset - transform->camera.top_left.y;
    y *= inverse_scale;
    float tint[4];
    for (size_t index = 0; index < 4; ++index) {
        tint[index] = layer->color[index] * 255;
        if (!isfinite(tint[index])) {
            return ab_fail(error, "Nonfinite theme tint");
        }
    }
    if (!isfinite(period) || period == 0 || !isfinite(remainder) ||
        !isfinite(scale.x) || scale.x == 0 || !isfinite(scale.y) ||
        !isfinite(inverse_scale) || !isfinite(y)) {
        return ab_fail(error, "Nonfinite theme base placement");
    }
    if (layer->repeats) {
        return draw_repeated(layer, transform, remainder, inverse_scale, y, scale, tint,
                             draw_limit, draw, context, error);
    }
    float x = -layer->x_offset;
    x += inverse_scale * screen_zero;
    x = -x;
    if (!isfinite(x)) {
        return ab_fail(error, "Nonfinite single theme placement");
    }
    return draw(context, (AbVec2){x, y}, scale, tint, error);
}
