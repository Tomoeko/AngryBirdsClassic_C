#include "ab/composite_draw.h"

#include <math.h>
#include <string.h>

bool ab_composite_draw_points(const AbCompositeBoundsPart *part,
                              AbSpriteRenderState parent, AbVec2 position,
                              AbVec2 output[4], AbError *error) {
    AbTransform2 local;
    if (!output || !part || !part->enabled || parent.direct_vertices ||
        parent.angle_radians != 0 || !(parent.scale_x > 0) || !(parent.scale_y > 0) ||
        !isfinite(parent.scale_x) || !isfinite(parent.scale_y) ||
        !isfinite(parent.translation_x) || !isfinite(parent.translation_y) ||
        !isfinite(position.x) || !isfinite(position.y) ||
        !ab_composite_part_transform(part, &local))
        return ab_fail(error, "Unsupported composite draw transform domain");
    AbTransform2 transform = {
        .xx = parent.scale_x * local.xx,
        .xy = parent.scale_x * local.xy,
        .yx = parent.scale_y * local.yx,
        .yy = parent.scale_y * local.yy,
        .tx = parent.translation_x * parent.scale_x + position.x * parent.scale_x,
        .ty = parent.translation_y * parent.scale_y + position.y * parent.scale_y};
    transform.tx += part->offset.x * parent.scale_x;
    transform.ty += part->offset.y * parent.scale_y;
    float pivot_x = -(float)(int32_t)((float)part->region->pivot_x + 0.5f);
    float pivot_y = -(float)(int32_t)((float)part->region->pivot_y + 0.5f);
    float x = transform.xy * pivot_y;
    x += transform.xx * pivot_x;
    x += transform.tx;
    float y = transform.yy * pivot_y;
    y += transform.yx * pivot_x;
    y += transform.ty;
    transform.tx = x;
    transform.ty = y;
    const AbVec2 corners[] = {{0, 0},
                              {part->region->width, 0},
                              {0, part->region->height},
                              {part->region->width, part->region->height}};
    AbVec2 points[4];
    for (size_t index = 0; index < 4; ++index) {
        points[index].x = corners[index].y * transform.xy;
        points[index].x += corners[index].x * transform.xx;
        points[index].x += transform.tx;
        points[index].y = corners[index].y * transform.yy;
        points[index].y += corners[index].x * transform.yx;
        points[index].y += transform.ty;
        if (!isfinite(points[index].x) || !isfinite(points[index].y))
            return ab_fail(error, "Nonfinite composite draw point");
    }
    memcpy(output, points, sizeof(points));
    return true;
}
