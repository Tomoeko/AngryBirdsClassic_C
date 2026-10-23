#include "ab/ui_image_render.h"

#include "ab/rotation.h"
#include "ab/composite_draw.h"
#include "ab/composite_part.h"
#include "ab/image_points.h"

#include <math.h>
#include <string.h>

static bool set_render_state(const AbUiImageDrawCall *call, AbUiImageRenderState *state,
                             AbError *error) {
    if (call->state.count != 7 && call->state.count != 8) {
        return ab_fail(error, "Invalid UI image render-state arity");
    }
    const float *values = call->state.arguments;
    for (size_t index = 0; index < call->state.count; ++index) {
        bool collapsed_translation =
            (index == 0 && values[2] == 0) || (index == 1 && values[3] == 0);
        if (!isfinite(values[index]) && !collapsed_translation) {
            return ab_fail(error, "Nonfinite UI image render state");
        }
    }
    AbRotation rotation;
    if (!ab_rotation_from_angle(values[4], &rotation)) {
        return ab_fail(error, "Unsupported UI image render rotation");
    }
    state->geometry.translation_x = values[0];
    state->geometry.translation_y = values[1];
    state->geometry.scale_x = values[2];
    state->geometry.scale_y = values[3];
    state->geometry.angle_radians = values[4];
    state->geometry.rotation_origin_x = values[5];
    state->geometry.rotation_origin_y = values[6];
    if (call->state.count == 8) {
        state->alpha = values[7];
    }
    return true;
}

static bool ordinary(const AbUiSpriteRender *sprite, const AbUiImageRenderState *state,
                     const AbUiImageRenderBoundaries *boundaries,
                     const AbUiImageRenderBinding *resolved, AbDrawList *list,
                     AbError *error) {
    AbUiImageRenderBinding binding = *resolved;
    if (!binding.texture || !binding.region) {
        return ab_fail(error, "Missing UI image texture binding");
    }
    AbDrawQuad local;
    AbVec2 size = sprite->sized
                      ? sprite->size
                      : (AbVec2){binding.region->width, binding.region->height};
    bool placed =
        sprite->sized
            ? ab_sprite_sized_quad(
                  binding.region, binding.texture_width, binding.texture_height,
                  (AbSpriteSizedPlacement){sprite->position.x, sprite->position.y,
                                           size.x, size.y, sprite->vertical,
                                           sprite->horizontal},
                  &local, error)
            : ab_sprite_quad(
                  binding.region, binding.texture_width, binding.texture_height,
                  (AbSpritePlacement){sprite->position.x, sprite->position.y, 1, 1,
                                      sprite->vertical, sprite->horizontal},
                  &local, error);
    if (!placed) {
        return false;
    }
    float uv[8];
    for (size_t vertex = 0; vertex < 4; ++vertex) {
        uv[vertex * 2] = local.vertices[vertex].u;
        uv[vertex * 2 + 1] = local.vertices[vertex].v;
    }
    AbImageRectangle rectangle = {local.vertices[0].x, local.vertices[0].y, size.x,
                                  size.y};
    AbDrawCommand command = {.texture = binding.texture,
                             .blend = boundaries->blend,
                             .opacity = state->alpha,
                             .scissor = state->clip,
                             .coordinates = AB_DRAW_CLIP};
    memcpy(command.color, boundaries->color, sizeof(command.color));
    bool visible;
    if (!ab_image_rectangle_quad(rectangle, state->geometry, boundaries->viewport_width,
                                 boundaries->viewport_height, uv, &command.quad,
                                 &visible, error)) {
        return false;
    }
    return !visible || ab_draw_list_push(list, &command, error);
}

static int32_t half(int32_t value) {
    return value >= 0 ? value / 2 : (int32_t)(-((-(int64_t)value + 1) / 2));
}

static bool transformed_part(const AbCompositePart *part, bool extended,
                             AbVec2 position, const AbUiImageRenderState *state,
                             const AbUiImageRenderBoundaries *boundaries,
                             const AbUiImageRenderBinding *binding, AbDrawList *list,
                             AbError *error) {
    if (!binding->region || !binding->texture ||
        (binding->alpha != AB_SPRITE_ALPHA_OPAQUE &&
         binding->alpha != AB_SPRITE_ALPHA_PRESENT))
        return ab_fail(error, "Transformed composite requires known texture alpha");
    AbCompositeBoundsPart transform;
    AbVec2 points[4];
    AbDrawQuad local;
    if (!ab_composite_part_state(part, extended, binding->region, &transform, error) ||
        !ab_composite_draw_points(&transform, state->geometry, position, points,
                                  error) ||
        !ab_sprite_quad(binding->region, binding->texture_width,
                        binding->texture_height,
                        (AbSpritePlacement){.scale_x = 1, .scale_y = 1}, &local, error))
        return false;
    float uv[8];
    for (size_t index = 0; index < 4; ++index) {
        uv[index * 2] = local.vertices[index].u;
        uv[index * 2 + 1] = local.vertices[index].v;
    }
    AbDrawCommand command;
    bool visible = false;
    if (!ab_image_points_command(
            points, boundaries->viewport_width, boundaries->viewport_height, uv,
            (AbImagePointMaterial){binding->texture,
                                   binding->alpha == AB_SPRITE_ALPHA_PRESENT,
                                   state->alpha},
            &command, &visible, error))
        return false;
    command.scissor = state->clip;
    return !visible || ab_draw_list_push(list, &command, error);
}

static bool composite(const AbUiSpriteRender *sprite, const AbUiImageRenderState *state,
                      const AbUiImageRenderBoundaries *boundaries,
                      const AbResolvedComposite *resolved, AbDrawList *list,
                      AbError *error) {
    const AbComposite *source = resolved->source;
    if (!source || (source->part_count && (!source->parts || !resolved->parts)) ||
        !boundaries->part || state->geometry.direct_vertices ||
        state->geometry.scale_x == 0 || state->geometry.scale_y == 0 ||
        (unsigned)sprite->vertical > AB_ANCHOR_VPIVOT ||
        (unsigned)sprite->horizontal > AB_ANCHOR_HPIVOT)
        return ab_fail(error, "Unsupported UI composite transform or part provider");
    AbVec2 position = sprite->position;
    const AbCompositeBounds *bounds = &resolved->bounds;
    int64_t horizontal = bounds->pivot_x, vertical = bounds->pivot_y;
    if (sprite->horizontal == AB_ANCHOR_HCENTER)
        horizontal -= half(bounds->width);
    else if (sprite->horizontal == AB_ANCHOR_RIGHT)
        horizontal -= bounds->width;
    else if (sprite->horizontal == AB_ANCHOR_HPIVOT)
        horizontal = 0;
    if (sprite->vertical == AB_ANCHOR_VCENTER)
        vertical -= half(bounds->height);
    else if (sprite->vertical == AB_ANCHOR_BOTTOM)
        vertical -= bounds->height;
    else if (sprite->vertical != AB_ANCHOR_TOP)
        vertical = 0;
    position.x += (float)horizontal;
    position.y += (float)vertical;
    for (size_t index = 0; index < source->part_count; ++index) {
        const AbCompositePart *part = source->parts + index;
        AbUiImageRenderBinding image = {0};
        if (!boundaries->part(boundaries->context, resolved->parts + index, &image,
                              error))
            return false;
        if (!transformed_part(part, resolved->extended, position, state, boundaries,
                              &image, list, error))
            return false;
    }
    return true;
}

bool ab_ui_sprite_render(const AbUiSpriteRender *sprite,
                         const AbUiImageRenderState *state,
                         const AbUiImageRenderBoundaries *boundaries, AbDrawList *list,
                         AbError *error) {
    if (!sprite || !state || !boundaries || !list || !boundaries->lookup ||
        (sprite->image.size && !sprite->image.data) || !isfinite(state->alpha) ||
        list->count > list->capacity || (list->capacity && !list->commands) ||
        !boundaries->viewport_width || !boundaries->viewport_height ||
        (unsigned)boundaries->blend > AB_BLEND_MULTIPLY) {
        return ab_fail(error, "Missing UI image sprite lookup boundary");
    }
    for (size_t channel = 0; channel < 4; ++channel)
        if (!isfinite(boundaries->color[channel]))
            return ab_fail(error, "Nonfinite UI sprite color");
    AbUiImageRenderBinding binding = {0};
    if (!boundaries->lookup(boundaries->context, sprite->image, &binding, error)) {
        return false;
    }
    /* Image.draw divides translation by scale, including zero scale during
     * recovered hide animations. Keep that state/lookup order, but submit no
     * singular, zero-area geometry to the native graphics backend. The plan's
     * following reset still restores the original state. */
    if (!state->geometry.direct_vertices &&
        (state->geometry.scale_x == 0 || state->geometry.scale_y == 0))
        return true;
    if (!binding.region && !binding.composite.source && !binding.texture)
        return true;
    return binding.composite.source
               ? composite(sprite, state, boundaries, &binding.composite, list, error)
               : ordinary(sprite, state, boundaries, &binding, list, error);
}

bool ab_ui_image_render_plan(const AbUiImageDrawPlan *plan, AbUiImageRenderState *state,
                             const AbUiImageRenderBoundaries *boundaries,
                             AbDrawList *list, AbError *error) {
    if (!plan || !state || !boundaries || !list || plan->count > 3 ||
        list->count > list->capacity || (list->capacity && !list->commands) ||
        !boundaries->viewport_width || !boundaries->viewport_height ||
        (unsigned)boundaries->blend > AB_BLEND_MULTIPLY || !isfinite(state->alpha)) {
        return ab_fail(error, "Invalid UI image render input");
    }
    for (size_t channel = 0; channel < 4; ++channel) {
        if (!isfinite(boundaries->color[channel])) {
            return ab_fail(error, "Nonfinite UI image render color");
        }
    }
    for (size_t index = 0; index < plan->count; ++index) {
        const AbUiImageDrawCall *call = plan->calls + index;
        if (call->kind == AB_UI_IMAGE_RENDER_STATE) {
            if (!set_render_state(call, state, error)) {
                return false;
            }
        } else if (call->kind == AB_UI_IMAGE_SPRITE) {
            AbUiSpriteRender sprite = {.image = call->sprite.image,
                                       .size = call->sprite.size,
                                       .vertical = AB_ANCHOR_VPIVOT,
                                       .horizontal = AB_ANCHOR_HPIVOT,
                                       .sized = true};
            if (!ab_ui_sprite_render(&sprite, state, boundaries, list, error)) {
                return false;
            }
        } else {
            return ab_fail(error, "Unknown UI image render command");
        }
    }
    return true;
}
