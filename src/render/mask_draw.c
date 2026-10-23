#include "ab/mask_draw.h"

#include <math.h>

bool ab_mask_draw_plan(AbMaskDrawInput input, AbMaskDrawPlan *output, AbError *error) {
    const AbSpriteRegion *mask = input.mask;
    if (!mask || !output || !input.width || !input.height || input.width > INT32_MAX ||
        input.height > INT32_MAX || !input.texture_width || !input.texture_height ||
        !input.mask_width || !input.mask_height)
        return ab_fail(error, "Invalid mask draw metadata/dimensions");
    float pivot_x = (float)(int32_t)((float)mask->pivot_x + 0.5f);
    float pivot_y = (float)(int32_t)((float)mask->pivot_y + 0.5f);
    float x = input.position.x - pivot_x;
    float y = input.position.y - pivot_y;
    float origin_x = input.rotation_origin.x;
    float origin_y = input.rotation_origin.y;
    float right = (float)mask->width - origin_x;
    float bottom = (float)mask->height - origin_y;
    float translation_x = x + origin_x;
    translation_x += input.transform.tx;
    float translation_y = y + origin_y;
    translation_y += input.transform.ty;
    float horizontal = (input.scale.x + input.scale.x) / (float)input.width;
    float vertical = (input.scale.y * -2) / (float)input.height;
    float left_x = -origin_x * input.transform.xx;
    float top_x = -origin_y * input.transform.xy;
    float left_y = -origin_x * input.transform.yx;
    float top_y = -origin_y * input.transform.yy;
    float right_x = right * input.transform.xx;
    float right_y = right * input.transform.yx;
    float bottom_x = bottom * input.transform.xy;
    float bottom_y = bottom * input.transform.yy;
    AbVec2 local[4] = {{left_x + top_x, left_y + top_y},
                       {top_x + right_x, top_y + right_y},
                       {left_x + bottom_x, left_y + bottom_y},
                       {bottom_x + right_x, right_y + bottom_y}};
    float inverse_mask_x = 1 / (float)input.mask_width;
    float inverse_mask_y = 1 / (float)input.mask_height;
    float u0 = (float)mask->x * inverse_mask_x;
    float v0 = (float)mask->y * inverse_mask_y;
    float u1 = (float)((int32_t)mask->x + mask->width) * inverse_mask_x;
    float v1 = (float)((int32_t)mask->y + mask->height) * inverse_mask_y;
    const float uv[8] = {u0, v0, u1, v0, u0, v1, u1, v1};
    float inverse_texture_x = 1 / (float)input.texture_width;
    float inverse_texture_y = 1 / (float)input.texture_height;
    x += pivot_x;
    y += pivot_y;
    AbMaskDrawPlan plan = {0};
    float min_x = INFINITY, min_y = INFINITY;
    float max_x = -INFINITY, max_y = -INFINITY;
    for (size_t vertex = 0; vertex < 4; ++vertex) {
        float px = translation_x + local[vertex].x;
        float py = translation_y + local[vertex].y;
        px = px * horizontal - 1;
        py = 1 + py * vertical;
        float tu = inverse_texture_x * (local[vertex].x + x);
        float tv = inverse_texture_y * (local[vertex].y + y);
        if (!isfinite(px) || !isfinite(py) || !isfinite(tu) || !isfinite(tv))
            return ab_fail(error, "Nonfinite mask draw transform");
        plan.quad.vertices[vertex] =
            (AbDrawVertex){px, py, uv[vertex * 2], uv[vertex * 2 + 1]};
        plan.texture_uv[vertex * 2] = tu;
        plan.texture_uv[vertex * 2 + 1] = tv;
        if (px < min_x)
            min_x = px;
        if (px > max_x)
            max_x = px;
        if (py < min_y)
            min_y = py;
        if (py > max_y)
            max_y = py;
    }
    plan.visible = max_x >= -1 && max_y >= -1 && min_x < 1 && min_y < 1;
    *output = plan;
    return true;
}
