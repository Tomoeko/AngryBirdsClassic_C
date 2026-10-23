#include "ab/theme_draw.h"

#include "ab/fill_rectangle.h"
#include "ab/image_points.h"

#include <limits.h>

typedef struct {
    AbThemeDraw *owner;
    AbThemeLayerDrawInput layer;
    uint32_t height;
    AbDrawList *list;
    bool force_alpha;
} DrawCall;

bool ab_theme_draw_initialize(AbThemeDrawConfig config, AbThemeDraw *output,
                              AbError *error) {
    if (!output || !config.layers || !config.provider || !config.provider->sprite ||
        !config.provider->texture || !config.render || !config.background_argb ||
        !config.white_texture || !config.placement_limit)
        return ab_fail(error, "Invalid owned theme draw bindings");
    *output = (AbThemeDraw){.config = config, .initialized = true};
    return true;
}

static bool image(void *context, const AbSpriteBinding *sprite,
                  const AbVec2 vertices[4], const float tint[4], AbError *error) {
    DrawCall *call = context;
    const AbThemeDrawConfig *config = &call->owner->config;
    AbSpriteAtlasBinding binding;
    uint32_t texture = 0;
    (void)tint;
    if (!sprite || !sprite->region)
        return ab_fail(error, "Missing retained theme sprite metadata");
    /* A composite part retains its original sprite selection. A later resource
     * group can mask that name without changing the already bound part. */
    bool found =
        config->provider->binding
            ? config->provider->binding(config->provider->context, *sprite, &binding)
            : config->provider->sprite(config->provider->context, sprite->region->name,
                                       &binding);
    if (!found || binding.sprite.region != sprite->region ||
        binding.sprite.sheet != sprite->sheet || !binding.image ||
        binding.alpha == AB_SPRITE_ALPHA_UNKNOWN ||
        !config->provider->texture(config->provider->context, binding.image, &texture))
        return ab_fail(error, "Missing or changed retained theme image binding");
    AbDrawQuad atlas;
    if (!ab_sprite_quad(sprite->region, binding.image->width, binding.image->height,
                        (AbSpritePlacement){.scale_x = 1, .scale_y = 1}, &atlas, error))
        return false;
    float uv[8];
    for (size_t vertex = 0; vertex < 4; ++vertex) {
        uv[2 * vertex] = atlas.vertices[vertex].u;
        uv[2 * vertex + 1] = atlas.vertices[vertex].v;
    }
    AbDrawCommand command = {0};
    bool visible = false;
    return ab_image_points_command(
               vertices, (uint32_t)call->layer.viewport_width, call->height, uv,
               (AbImagePointMaterial){texture,
                                      call->force_alpha ||
                                          binding.alpha == AB_SPRITE_ALPHA_PRESENT,
                                      config->render->alpha},
               &command, &visible, error) &&
           (!visible || ab_draw_list_push(call->list, &command, error));
}

static bool fill(void *context, const AbGameRectanglePlan *plan, AbError *error);

static bool layer(void *context, uintptr_t handle, uint32_t filter, AbError *error) {
    DrawCall *call = context;
    const AbThemeLayerBinding *binding;
    if (!ab_theme_layers_binding(call->owner->config.layers, handle, &binding))
        return ab_fail(error, "Missing cached theme layer handle");
    if (binding->kind == AB_THEME_SPRITE_RECT) {
        AbGameRectanglePlan plan;
        bool active = false;
        return ab_theme_rect_plan(&binding->rect, (uint32_t)call->layer.viewport_width,
                                  call->height, &plan, &active, error) &&
               (!active || fill(call, &plan, error));
    }
    call->force_alpha = binding->kind == AB_THEME_SPRITE_BORDER;
    if (call->force_alpha)
        return ab_theme_layers_draw_border(
            call->owner->config.layers, handle, (uint32_t)call->layer.viewport_width,
            call->height, filter, call->owner->config.render, image, call, error);
    return ab_theme_layers_draw_base(call->owner->config.layers, handle, &call->layer,
                                     call->owner->config.render, image, call, error);
}

static bool fill(void *context, const AbGameRectanglePlan *plan, AbError *error) {
    DrawCall *call = context;
    if (!plan->reset_state)
        return ab_fail(error, "Unsupported nonidentity theme ground fill");
    *call->owner->config.render =
        (AbThemeLayerRenderState){.geometry = {.scale_x = 1, .scale_y = 1}, .alpha = 1};
    AbDrawCommand command = {0};
    bool visible = false;
    return ab_fill_rectangle_reset_command(
               (AbFillRectangle){plan->argb, plan->left, plan->top, plan->width,
                                 plan->height, AB_FILL_BLEND_AUTO},
               (uint32_t)call->layer.viewport_width, call->height,
               call->owner->config.white_texture, &command, &visible, error) &&
           (!visible || ab_draw_list_push(call->list, &command, error));
}

static bool draw(AbThemeDraw *owner, AbThemeDrawInput input, bool foreground,
                 AbDrawList *list, AbError *error) {
    if (!owner || !owner->initialized || owner->calling || owner->failed || !list ||
        list->count > list->capacity || (list->capacity && !list->commands) ||
        !input.width || !input.height || input.width > INT32_MAX ||
        input.height > INT32_MAX)
        return ab_fail(error, "Invalid or reentrant theme draw");
    owner->calling = true;
    DrawCall call = {owner,
                     {input.camera, (int32_t)input.width, input.time,
                      owner->config.placement_limit, true},
                     input.height,
                     list,
                     false};
    bool okay;
    if (foreground) {
        AbThemeForeground view;
        okay = ab_theme_layers_foreground(owner->config.layers, &view) &&
               ab_theme_foreground_draw(
                   &view,
                   &(AbThemeForegroundInput){input.camera, (int32_t)input.width,
                                             (int32_t)input.height, input.filter},
                   owner->config.render,
                   &(AbThemeForegroundBoundaries){&call, fill, layer}, error);
    } else {
        AbThemeBackground view;
        okay = ab_theme_layers_background(owner->config.layers, &view) &&
               ab_theme_background_draw(
                   &view,
                   (AbThemeBackgroundState){owner->config.background_argb,
                                            owner->config.render},
                   input.filter, layer, &call, error);
    }
    owner->calling = false;
    owner->failed = !okay;
    return okay;
}

bool ab_theme_draw_background(AbThemeDraw *owner, AbThemeDrawInput input,
                              AbDrawList *list, AbError *error) {
    return draw(owner, input, false, list, error);
}

bool ab_theme_draw_foreground(AbThemeDraw *owner, AbThemeDrawInput input,
                              AbDrawList *list, AbError *error) {
    return draw(owner, input, true, list, error);
}
