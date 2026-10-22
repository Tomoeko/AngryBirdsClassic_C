#include "ab/theme_sprite.h"

#include <math.h>
#include <string.h>

bool ab_theme_sprite_vertices(const AbSpriteRegion *region, AbVec2 position,
                              AbVec2 scale, AbVec2 output[4], AbError *error) {
    if (!region || !output || !isfinite(position.x) || !isfinite(position.y) ||
        !isfinite(scale.x) || !isfinite(scale.y)) {
        return ab_fail(error, "Invalid theme sprite geometry");
    }
    float pivot_x = (float)(int32_t)((float)region->pivot_x + 0.5f);
    float pivot_y = (float)(int32_t)((float)region->pivot_y + 0.5f);
    float translation_x = position.x - pivot_x;
    float translation_y = position.y - pivot_y;
    translation_x *= scale.x;
    translation_y *= scale.y;
    float xy = scale.y * -0.0f, yx = scale.x * 0.0f;
    const AbVec2 corners[] = {{0, 0},
                              {region->width, 0},
                              {0, region->height},
                              {region->width, region->height}};
    AbVec2 vertices[4];
    for (size_t index = 0; index < 4; ++index) {
        float x = corners[index].y * xy;
        x += corners[index].x * scale.x;
        x += translation_x;
        float y = corners[index].y * scale.y;
        y += corners[index].x * yx;
        y += translation_y;
        if (!isfinite(x) || !isfinite(y)) {
            return ab_fail(error, "Nonfinite theme sprite vertex");
        }
        vertices[index] = (AbVec2){x, y};
    }
    memcpy(output, vertices, sizeof(vertices));
    return true;
}

bool ab_theme_composite_draw(const AbComposite *composite, AbVec2 position,
                             AbVec2 scale, const float tint[4],
                             const AbThemeCompositeBoundaries *boundaries,
                             AbError *error) {
    if (!composite || (composite->part_count && !composite->parts) || !tint ||
        !boundaries || !boundaries->lookup || !boundaries->image ||
        !isfinite(position.x) || !isfinite(position.y) || !isfinite(scale.x) ||
        !isfinite(scale.y)) {
        return ab_fail(error, "Invalid theme composite draw arguments");
    }
    for (size_t index = 0; index < composite->part_count; ++index) {
        const AbCompositePart *part = composite->parts + index;
        const AbSpriteRegion *region = NULL;
        if (part->sprite.size && !part->sprite.data) {
            return ab_fail(error, "Invalid theme composite sprite name");
        }
        if (!boundaries->lookup(boundaries->context, part->sprite, &region, error)) {
            return false;
        }
        AbVec2 point = {position.x + (float)part->x, position.y + (float)part->y};
        AbVec2 vertices[4];
        if (!ab_theme_sprite_vertices(region, point, scale, vertices, error) ||
            !boundaries->image(boundaries->context, region, vertices, tint, error)) {
            return false;
        }
    }
    return true;
}
