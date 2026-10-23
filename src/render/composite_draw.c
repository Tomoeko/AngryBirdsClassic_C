#include "ab/composite_draw.h"
#include "ab/rotation.h"

#include <math.h>
#include <string.h>

static bool context_transform(AbSpriteRenderState parent, AbTransform2 local,
                              AbVec2 position, AbVec2 offset, AbTransform2 *output) {
    AbRotation rotation;
    if (!ab_rotation_from_angle(parent.angle_radians, &rotation))
        return false;
    float squared = rotation.sine * rotation.sine;
    squared += rotation.cosine * rotation.cosine;
    float inverse = 1 / sqrtf(squared);
    float xx = parent.scale_x * (inverse * rotation.cosine);
    float yx = parent.scale_x * (inverse * rotation.sine);
    float xy = parent.scale_y * (inverse * -rotation.sine);
    float yy = parent.scale_y * (inverse * rotation.cosine);
    squared = yx * yx;
    squared += xx * xx;
    float horizontal = sqrtf(squared);
    squared = yy * yy;
    squared += xy * xy;
    float vertical = sqrtf(squared);
    /* The native composite measures the context columns, scales translation
     * and anchor by those positive lengths, then adds each transformed offset.
     * Context rotation origins are unused by this point-image overload. */
    AbTransform2 transform;
    transform.xx = xy * local.yx;
    transform.xx += xx * local.xx;
    transform.xy = xy * local.yy;
    transform.xy += xx * local.xy;
    transform.yx = yy * local.yx;
    transform.yx += yx * local.xx;
    transform.yy = yy * local.yy;
    transform.yy += yx * local.xy;
    transform.tx = xy * offset.y;
    transform.tx += xx * offset.x;
    transform.tx += parent.translation_x * horizontal + position.x * horizontal;
    transform.ty = yy * offset.y;
    transform.ty += yx * offset.x;
    transform.ty += parent.translation_y * vertical + position.y * vertical;
    *output = transform;
    return true;
}

bool ab_composite_draw_points(const AbCompositeBoundsPart *part,
                              AbSpriteRenderState parent, AbVec2 position,
                              AbVec2 output[4], AbError *error) {
    AbTransform2 local;
    if (!output || !part || !part->enabled || parent.direct_vertices ||
        parent.scale_x == 0 || parent.scale_y == 0 || !isfinite(parent.scale_x) ||
        !isfinite(parent.scale_y) || !isfinite(parent.translation_x) ||
        !isfinite(parent.translation_y) || !isfinite(position.x) ||
        !isfinite(position.y) || !ab_composite_part_transform(part, &local))
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
    if ((parent.angle_radians != 0 || parent.scale_x < 0 || parent.scale_y < 0) &&
        !context_transform(parent, local, position, part->offset, &transform))
        return ab_fail(error, "Unsupported composite context rotation");
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
