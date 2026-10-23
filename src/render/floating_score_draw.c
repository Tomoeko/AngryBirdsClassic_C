#include "ab/floating_score_draw.h"

#include "ab/utf8.h"

#include <string.h>

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool draw_text(const AbBirdCollisionFloatingScore *score,
                      const AbFloatingScoreDrawResources *resources,
                      AbFloatingScoreDrawPlan plan, uint32_t width, uint32_t height,
                      AbDrawList *list, AbError *error) {
    if (!score->text_present)
        return true;
    AbSpan localized;
    size_t length, count;
    if (!ab_text_resources_get(resources->strings, span("TEXTS_BASIC"), score->text,
                               &localized, error) ||
        !ab_utf8_to_codes(localized, resources->codes, resources->capacity, &length))
        return ab_fail(error, "Floating-score text exceeds prepared UTF storage");
    AbFontPlacement placement = {.placement = {.x = plan.text_position.x,
                                               .y = plan.text_position.y,
                                               .scale_x = 1,
                                               .scale_y = 1,
                                               .vertical = AB_ANCHOR_BOTTOM,
                                               .horizontal = AB_ANCHOR_HCENTER}};
    if (!ab_font_layout_codes(resources->font, resources->codes, length, 0, length,
                              placement, resources->glyphs, resources->capacity,
                              &count))
        return ab_fail(error, "Cannot lay out actual floating-score font");
    return ab_font_draw_native(
        resources->glyphs, count,
        (AbFontNativeDrawStyle){.texture = resources->texture,
                                .texture_width = resources->texture_width,
                                .texture_height = resources->texture_height,
                                .viewport_width = width,
                                .viewport_height = height,
                                .glyph_scale_x = 1,
                                .glyph_scale_y = 1,
                                .state = {.scale_x = plan.scale, .scale_y = plan.scale},
                                .blend = AB_BLEND_PREMULTIPLIED,
                                .color = {1, 1, 1, 1},
                                .opacity = 1},
        list, error);
}

static bool draw_sprite(AbDataValue sprite,
                        const AbFloatingScoreDrawResources *resources,
                        AbFloatingScoreDrawPlan plan, uint32_t width, uint32_t height,
                        AbDrawList *list, AbError *error) {
    if (sprite.kind == AB_DATA_NIL)
        return true;
    if (sprite.kind != AB_DATA_STRING)
        return ab_fail(error, "Unsupported floating-score sprite identifier");
    AbSpriteAtlasBinding binding;
    const AbSpriteProvider *provider = resources->sprites;
    if (!provider->sprite(provider->context, sprite.string, &binding))
        return true;
    uint32_t texture;
    if (!binding.sprite.region || !binding.image ||
        binding.alpha == AB_SPRITE_ALPHA_UNKNOWN ||
        !provider->texture(provider->context, binding.image, &texture))
        return ab_fail(error, "Missing prepared floating-score sprite image");
    AbDrawQuad quad;
    if (!ab_sprite_quad(
            binding.sprite.region, binding.image->width, binding.image->height,
            (AbSpritePlacement){plan.sprite_position.x, plan.sprite_position.y, 1, 1,
                                AB_ANCHOR_BOTTOM, AB_ANCHOR_HCENTER},
            &quad, error))
        return false;
    AbDrawCommand command;
    bool visible;
    AbImageRectangle rectangle = {quad.vertices[0].x, quad.vertices[0].y,
                                  (float)binding.sprite.region->width,
                                  (float)binding.sprite.region->height};
    float uv[8];
    for (size_t vertex = 0; vertex < 4; ++vertex) {
        uv[vertex * 2] = quad.vertices[vertex].u;
        uv[vertex * 2 + 1] = quad.vertices[vertex].v;
    }
    return ab_image_rectangle_command(
               rectangle,
               (AbSpriteRenderState){.scale_x = plan.scale, .scale_y = plan.scale},
               width, height, uv,
               (AbImageRectangleMaterial){texture,
                                          binding.alpha == AB_SPRITE_ALPHA_PRESENT, 1},
               &command, &visible, error) &&
           (!visible || ab_draw_list_push(list, &command, error));
}

bool ab_floating_scores_draw(AbFloatingScores *scores,
                             const AbFloatingScoreDrawResources *resources,
                             AbCoordinateCamera camera, float ui_scale, uint32_t width,
                             uint32_t height, AbDrawList *list, AbError *error) {
    if (!scores || !resources || !resources->font || !resources->texture ||
        !resources->strings || !resources->sprites || !resources->sprites->sprite ||
        !resources->sprites->texture || !resources->codes || !resources->glyphs ||
        !resources->capacity || !width || !height || !list)
        return ab_fail(error, "Missing prepared floating-score draw resources");
    size_t count = ab_floating_scores_count(scores);
    for (size_t index = 0; index < count; ++index) {
        const AbBirdCollisionFloatingScore *score =
            ab_floating_scores_at(scores, index);
        AbFloatingScoreDrawPlan plan;
        if (!score || !ab_floating_score_draw_plan(score->position, score->xs, camera,
                                                   ui_scale, &plan, error))
            return false;
        if (plan.visible &&
            (!draw_text(score, resources, plan, width, height, list, error) ||
             !draw_sprite(score->sprite, resources, plan, width, height, list, error)))
            return false;
    }
    return true;
}
