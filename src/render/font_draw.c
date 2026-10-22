#include "ab/font_draw.h"

#include <math.h>

bool ab_font_draw_unrotated(const AbFontGlyphPlacement *glyphs, size_t count,
                            AbFontDrawStyle style, AbDrawList *list, AbError *error) {
    if ((count && !glyphs) || !list || list->count > list->capacity ||
        count > list->capacity - list->count) {
        return ab_fail(error, "Invalid font draw input or insufficient draw capacity");
    }
    size_t original_count = list->count;
    for (size_t index = 0; index < count; ++index) {
        const AbFontGlyphPlacement *entry = glyphs + index;
        if (!entry->glyph) {
            ab_fail(error, "Missing font draw glyph");
            goto restore_count;
        }
        const AbFontGlyph *glyph = entry->glyph;
        AbSpriteRegion region = {.x = glyph->x,
                                 .y = glyph->y,
                                 .width = glyph->width,
                                 .height = glyph->height,
                                 .pivot_y = glyph->pivot_y};
        AbSpritePlacement placement = {.x = entry->x,
                                       .y = entry->y,
                                       .scale_x = style.scale_x,
                                       .scale_y = style.scale_y,
                                       .vertical = AB_ANCHOR_VPIVOT,
                                       .horizontal = AB_ANCHOR_HPIVOT};
        AbDrawCommand command = {
            .texture = style.texture, .blend = style.blend, .opacity = style.opacity};
        for (size_t channel = 0; channel < 4; ++channel) {
            command.color[channel] = style.color[channel];
        }
        AbDrawQuad local;
        if (!ab_sprite_quad(&region, style.texture_width, style.texture_height,
                            placement, &local, error) ||
            !ab_quad_transform(&local, style.transform, &command.quad, error) ||
            !ab_draw_list_push(list, &command, error)) {
            goto restore_count;
        }
    }
    return true;

restore_count:
    list->count = original_count;
    return false;
}

static bool append_native_glyph(const AbFontGlyphPlacement *entry,
                                const AbFontNativeDrawStyle *style, AbDrawList *list,
                                AbError *error) {
    if (!entry->glyph) {
        return ab_fail(error, "Missing native font draw glyph");
    }
    const AbFontGlyph *glyph = entry->glyph;
    AbSpriteRegion region = {.x = glyph->x,
                             .y = glyph->y,
                             .width = glyph->width,
                             .height = glyph->height,
                             .pivot_y = glyph->pivot_y};
    AbSpritePlacement placement = {
        entry->x,         entry->y,        style->glyph_scale_x, style->glyph_scale_y,
        AB_ANCHOR_VPIVOT, AB_ANCHOR_HPIVOT};
    AbDrawQuad local;
    if (!ab_sprite_quad(&region, style->texture_width, style->texture_height, placement,
                        &local, error)) {
        return false;
    }
    float uv[8];
    for (size_t vertex = 0; vertex < 4; ++vertex) {
        uv[vertex * 2] = local.vertices[vertex].u;
        uv[vertex * 2 + 1] = local.vertices[vertex].v;
    }
    AbImageRectangle rectangle = {local.vertices[0].x, local.vertices[0].y,
                                  (float)glyph->width * style->glyph_scale_x,
                                  (float)glyph->height * style->glyph_scale_y};
    AbSpriteRenderState state = style->state;
    state.rotation_origin_x = entry->rotation_origin_x;
    state.rotation_origin_y = entry->rotation_origin_y;
    AbDrawCommand command = {.texture = style->texture,
                             .blend = style->blend,
                             .opacity = style->opacity,
                             .coordinates = AB_DRAW_CLIP};
    for (size_t channel = 0; channel < 4; ++channel) {
        command.color[channel] = style->color[channel];
    }
    bool visible;
    if (!ab_image_rectangle_quad(rectangle, state, style->viewport_width,
                                 style->viewport_height, uv, &command.quad, &visible,
                                 error)) {
        return false;
    }
    return !visible || ab_draw_list_push(list, &command, error);
}

bool ab_font_draw_native(const AbFontGlyphPlacement *glyphs, size_t count,
                         AbFontNativeDrawStyle style, AbDrawList *list,
                         AbError *error) {
    if (!list || list->count > list->capacity || (list->capacity && !list->commands) ||
        (count && !glyphs) || !style.texture || !style.texture_width ||
        !style.texture_height || !style.viewport_width || !style.viewport_height ||
        !isfinite(style.opacity) || (unsigned)style.blend > AB_BLEND_MULTIPLY) {
        return ab_fail(error, "Invalid native font draw input");
    }
    for (size_t channel = 0; channel < 4; ++channel) {
        if (!isfinite(style.color[channel])) {
            return ab_fail(error, "Invalid native font draw color");
        }
    }
    size_t original_count = list->count;
    for (size_t index = 0; index < count; ++index) {
        if (!append_native_glyph(glyphs + index, &style, list, error)) {
            list->count = original_count;
            return false;
        }
    }
    return true;
}
