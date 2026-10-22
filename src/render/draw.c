#include "ab/draw.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>

_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   FLT_MAX_EXP == 128,
               "Sprite geometry requires binary32 float");

static bool finite_quad(const AbDrawQuad *quad) {
    for (size_t index = 0; index < 4; ++index) {
        const AbDrawVertex *vertex = quad->vertices + index;
        if (!isfinite(vertex->x) || !isfinite(vertex->y) || !isfinite(vertex->u) ||
            !isfinite(vertex->v)) {
            return false;
        }
    }
    return true;
}

static int32_t half_dimension(int16_t value) {
    /* ARM ASR rounds negative odd dimensions down, unlike C integer division. */
    int32_t wide = value;
    return wide >= 0 ? wide / 2 : -((-wide + 1) / 2);
}

static int32_t rounded_pivot(int16_t value) {
    /* The target uses add 0.5 then truncation, including for negative pivots. */
    return (int32_t)((float)value + 0.5f);
}

static int32_t horizontal_offset(const AbSpriteRegion *region,
                                 AbHorizontalAnchor anchor) {
    switch (anchor) {
        case AB_ANCHOR_HCENTER:
            return half_dimension(region->width);
        case AB_ANCHOR_RIGHT:
            return region->width;
        case AB_ANCHOR_HPIVOT:
            return rounded_pivot(region->pivot_x);
        default:
            return 0;
    }
}

static int32_t vertical_offset(const AbSpriteRegion *region, AbVerticalAnchor anchor) {
    switch (anchor) {
        case AB_ANCHOR_VCENTER:
            return half_dimension(region->height);
        case AB_ANCHOR_BOTTOM:
            return region->height;
        case AB_ANCHOR_BASELINE:
        case AB_ANCHOR_VPIVOT:
            return rounded_pivot(region->pivot_y);
        default:
            return 0;
    }
}

bool ab_sprite_quad(const AbSpriteRegion *region, uint32_t texture_width,
                    uint32_t texture_height, AbSpritePlacement placement,
                    AbDrawQuad *output, AbError *error) {
    if (!region || !output || !texture_width || !texture_height ||
        texture_width > INT32_MAX || texture_height > INT32_MAX ||
        !isfinite(placement.x) || !isfinite(placement.y) ||
        !isfinite(placement.scale_x) || !isfinite(placement.scale_y) ||
        (unsigned)placement.vertical > AB_ANCHOR_VPIVOT ||
        (unsigned)placement.horizontal > AB_ANCHOR_HPIVOT) {
        return ab_fail(error, "Invalid sprite geometry input");
    }
    float left = placement.x - (float)horizontal_offset(region, placement.horizontal) *
                                   placement.scale_x;
    float top = placement.y -
                (float)vertical_offset(region, placement.vertical) * placement.scale_y;
    float right = left + (float)region->width * placement.scale_x;
    float bottom = top + (float)region->height * placement.scale_y;
    float u0 = (float)region->x / (float)texture_width;
    float v0 = (float)region->y / (float)texture_height;
    float u1 = (float)((int32_t)region->x + region->width) / (float)texture_width;
    float v1 = (float)((int32_t)region->y + region->height) / (float)texture_height;
    AbDrawQuad quad = {{{left, top, u0, v0},
                        {right, top, u1, v0},
                        {left, bottom, u0, v1},
                        {right, bottom, u1, v1}}};
    if (!finite_quad(&quad)) {
        return ab_fail(error, "Sprite geometry exceeds finite range");
    }
    *output = quad;
    return true;
}

bool ab_sprite_sized_quad(const AbSpriteRegion *region, uint32_t texture_width,
                          uint32_t texture_height, AbSpriteSizedPlacement placement,
                          AbDrawQuad *output, AbError *error) {
    if (!output || !isfinite(placement.width) || !isfinite(placement.height)) {
        return ab_fail(error, "Invalid explicitly sized sprite input");
    }
    AbSpritePlacement anchored = {placement.x,        placement.y,         1, 1,
                                  placement.vertical, placement.horizontal};
    AbDrawQuad quad;
    if (!ab_sprite_quad(region, texture_width, texture_height, anchored, &quad,
                        error)) {
        return false;
    }
    float right = quad.vertices[0].x + placement.width;
    float bottom = quad.vertices[0].y + placement.height;
    if (!isfinite(right) || !isfinite(bottom)) {
        return ab_fail(error, "Explicit sprite size exceeds finite range");
    }
    quad.vertices[1].x = quad.vertices[3].x = right;
    quad.vertices[2].y = quad.vertices[3].y = bottom;
    *output = quad;
    return true;
}

bool ab_quad_transform(const AbDrawQuad *input, AbTransform2 transform,
                       AbDrawQuad *output, AbError *error) {
    if (!input || !output || !finite_quad(input) || !isfinite(transform.xx) ||
        !isfinite(transform.xy) || !isfinite(transform.tx) || !isfinite(transform.yx) ||
        !isfinite(transform.yy) || !isfinite(transform.ty)) {
        return ab_fail(error, "Invalid quad transform");
    }
    AbDrawQuad quad = *input;
    for (size_t index = 0; index < 4; ++index) {
        AbDrawVertex *vertex = quad.vertices + index;
        float x = vertex->y * transform.xy;
        x += vertex->x * transform.xx;
        x += transform.tx;
        float y = vertex->y * transform.yy;
        y += vertex->x * transform.yx;
        y += transform.ty;
        vertex->x = x;
        vertex->y = y;
    }
    if (!finite_quad(&quad)) {
        return ab_fail(error, "Quad transform exceeds finite range");
    }
    *output = quad;
    return true;
}

bool ab_draw_list_init(size_t capacity, size_t allocation_limit, AbDrawList *output,
                       AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing draw list output");
    }
    *output = (AbDrawList){0};
    if (capacity > allocation_limit / sizeof(*output->commands)) {
        return ab_fail(error, "Draw list exceeds allocation limit");
    }
    if (capacity) {
        output->commands = calloc(capacity, sizeof(*output->commands));
        if (!output->commands) {
            return ab_fail(error, "Cannot allocate draw list");
        }
    }
    output->capacity = capacity;
    return true;
}

bool ab_draw_command_check(const AbDrawCommand *command, AbError *error) {
    if (!command || !finite_quad(&command->quad) || !isfinite(command->opacity) ||
        (unsigned)command->blend > AB_BLEND_MULTIPLY ||
        (unsigned)command->coordinates > AB_DRAW_CLIP ||
        (command->scissor.enabled &&
         (command->scissor.right < command->scissor.left ||
          command->scissor.bottom < command->scissor.top))) {
        return ab_fail(error, "Invalid draw command");
    }
    return true;
}

bool ab_draw_list_push(AbDrawList *list, const AbDrawCommand *command, AbError *error) {
    if (!list || !list->commands || list->count >= list->capacity) {
        return ab_fail(error, "Invalid draw list or draw list is full");
    }
    if (!ab_draw_command_check(command, error)) {
        return false;
    }
    for (size_t channel = 0; channel < 4; ++channel) {
        if (!isfinite(command->color[channel])) {
            return ab_fail(error, "Invalid draw command color");
        }
    }
    list->commands[list->count++] = *command;
    return true;
}

void ab_draw_list_clear(AbDrawList *list) {
    if (list) {
        list->count = 0;
    }
}

void ab_draw_list_free(AbDrawList *list) {
    if (list) {
        free(list->commands);
        *list = (AbDrawList){0};
    }
}
