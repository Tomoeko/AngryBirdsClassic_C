#include "ab/image_points.h"

#include <math.h>

bool ab_image_points_quad(const AbVec2 points[4], uint32_t viewport_width,
                          uint32_t viewport_height, const float uv[8],
                          AbDrawQuad *output, bool *visible, AbError *error) {
    if (!points || !uv || !output || !visible || !viewport_width || !viewport_height ||
        viewport_width > INT32_MAX || viewport_height > INT32_MAX) {
        return ab_fail(error, "Invalid image point viewport/outputs");
    }
    float horizontal = 2 / (float)viewport_width;
    float vertical = -2 / (float)viewport_height;
    AbDrawQuad quad = {0};
    for (size_t index = 0; index < 4; ++index) {
        float x = -1 + horizontal * points[index].x;
        float y = 1 + vertical * points[index].y;
        if (!isfinite(points[index].x) || !isfinite(points[index].y) || !isfinite(x) ||
            !isfinite(y) || !isfinite(uv[index * 2]) || !isfinite(uv[index * 2 + 1])) {
            return ab_fail(error, "Nonfinite image point geometry");
        }
        quad.vertices[index] = (AbDrawVertex){x, y, uv[index * 2], uv[index * 2 + 1]};
    }
    float minimum_x = quad.vertices[0].x, maximum_x = minimum_x;
    float minimum_y = quad.vertices[0].y, maximum_y = minimum_y;
    for (size_t index = 1; index < 4; ++index) {
        float x = quad.vertices[index].x, y = quad.vertices[index].y;
        if (x < minimum_x) {
            minimum_x = x;
        }
        if (x > maximum_x) {
            maximum_x = x;
        }
        if (y < minimum_y) {
            minimum_y = y;
        }
        if (y > maximum_y) {
            maximum_y = y;
        }
    }
    *output = quad;
    *visible = maximum_x >= -1 && maximum_y >= -1 && minimum_x < 1 && minimum_y < 1;
    return true;
}

bool ab_image_points_command(const AbVec2 points[4], uint32_t viewport_width,
                             uint32_t viewport_height, const float uv[8],
                             AbImagePointMaterial material, AbDrawCommand *output,
                             bool *visible, AbError *error) {
    AbDrawQuad quad = {0};
    bool drawn = false;
    if (!output || !visible ||
        !ab_image_points_quad(points, viewport_width, viewport_height, uv, &quad,
                              &drawn, error)) {
        return ab_fail(error, "Invalid point image command geometry/outputs");
    }
    if (!drawn) {
        *visible = false;
        return true;
    }
    if (!material.texture || (material.has_alpha && !isfinite(material.render_alpha))) {
        return ab_fail(error, "Invalid default point image material");
    }
    *output = (AbDrawCommand){.blend = material.has_alpha ? AB_BLEND_PREMULTIPLIED
                                                          : AB_BLEND_OPAQUE,
                              .texture = material.texture,
                              .quad = quad,
                              .color = {1, 1, 1, 1},
                              .opacity = material.has_alpha ? material.render_alpha : 1,
                              .coordinates = AB_DRAW_CLIP};
    *visible = true;
    return true;
}
