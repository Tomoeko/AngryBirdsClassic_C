#include "ab/image_rectangle.h"
#include "ab/rotation.h"

#include <math.h>

static void unrotated(AbImageRectangle rectangle, AbSpriteRenderState state,
                      float horizontal, float vertical, AbDrawQuad *quad) {
    float left = rectangle.x + state.translation_x;
    float top = rectangle.y + state.translation_y;
    float right = left + rectangle.width;
    float bottom = top + rectangle.height;
    float x0 = -1.0f + horizontal * left;
    float x1 = -1.0f + right * horizontal;
    float y0 = 1.0f + vertical * top;
    float y1 = 1.0f + bottom * vertical;
    quad->vertices[0].x = quad->vertices[2].x = x0;
    quad->vertices[1].x = quad->vertices[3].x = x1;
    quad->vertices[0].y = quad->vertices[1].y = y0;
    quad->vertices[2].y = quad->vertices[3].y = y1;
}

static void rotated(AbImageRectangle rectangle, AbSpriteRenderState state,
                    AbRotation rotation, float horizontal, float vertical,
                    AbDrawQuad *quad) {
    float x = rectangle.x + state.rotation_origin_x;
    float y = rectangle.y + state.rotation_origin_y;
    x += state.translation_x;
    y += state.translation_y;
    float left_x = x + -state.rotation_origin_x * rotation.cosine;
    float left_y = y + -state.rotation_origin_x * rotation.sine;
    float top_x = -state.rotation_origin_y * -rotation.sine;
    float top_y = -state.rotation_origin_y * rotation.cosine;
    float right = rectangle.width - state.rotation_origin_x;
    float right_x = x + right * rotation.cosine;
    float right_y = y + right * rotation.sine;
    float bottom = rectangle.height - state.rotation_origin_y;
    float bottom_x = bottom * -rotation.sine;
    float bottom_y = bottom * rotation.cosine;
    float positions[4][2] = {
        {left_x + top_x, left_y + top_y},
        {top_x + right_x, top_y + right_y},
        {left_x + bottom_x, left_y + bottom_y},
        {right_x + bottom_x, right_y + bottom_y},
    };
    for (size_t index = 0; index < 4; ++index) {
        quad->vertices[index].x = -1.0f + positions[index][0] * horizontal;
        quad->vertices[index].y = 1.0f + positions[index][1] * vertical;
    }
}

static bool rotated_visible(const AbDrawQuad *quad) {
    float minimum_x = quad->vertices[0].x, maximum_x = minimum_x;
    float minimum_y = quad->vertices[0].y, maximum_y = minimum_y;
    for (size_t index = 1; index < 4; ++index) {
        float x = quad->vertices[index].x, y = quad->vertices[index].y;
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
    return maximum_x >= -1 && maximum_y >= -1 && minimum_x < 1 && minimum_y < 1;
}

static bool unrotated_visible(AbImageRectangle rectangle, AbSpriteRenderState state,
                              const AbDrawQuad *quad) {
    float left = quad->vertices[0].x, right = quad->vertices[1].x;
    float top = quad->vertices[0].y, bottom = quad->vertices[2].y;
    bool horizontal = rectangle.width * state.scale_x < 0 ? left >= -1 && right < 1
                                                          : right >= -1 && left < 1;
    bool vertical = rectangle.height * state.scale_y < 0 ? top <= 1 && bottom > -1
                                                         : bottom <= 1 && top > -1;
    return horizontal && vertical;
}

bool ab_image_rectangle_quad(AbImageRectangle rectangle, AbSpriteRenderState state,
                             uint32_t viewport_width, uint32_t viewport_height,
                             const float uv[8], AbDrawQuad *output, bool *visible,
                             AbError *error) {
    if (!output || !visible || !uv || !viewport_width || !viewport_height ||
        viewport_width > INT32_MAX || viewport_height > INT32_MAX ||
        !isfinite(rectangle.x) || !isfinite(rectangle.y) ||
        !isfinite(rectangle.width) || !isfinite(rectangle.height) ||
        !isfinite(state.translation_x) || !isfinite(state.translation_y) ||
        !isfinite(state.scale_x) || !isfinite(state.scale_y) ||
        !isfinite(state.rotation_origin_x) || !isfinite(state.rotation_origin_y)) {
        return ab_fail(error, "Invalid image rectangle input");
    }
    AbRotation rotation;
    if (!ab_rotation_from_angle(state.angle_radians, &rotation)) {
        return ab_fail(error, "Unsupported image rectangle rotation");
    }
    AbDrawQuad quad = {0};
    bool drawn = true;
    if (state.direct_vertices) {
        quad.vertices[0].x = quad.vertices[2].x = rectangle.x;
        quad.vertices[1].x = quad.vertices[3].x = rectangle.x + rectangle.width;
        quad.vertices[0].y = quad.vertices[1].y = rectangle.y;
        quad.vertices[2].y = quad.vertices[3].y = rectangle.y + rectangle.height;
    } else {
        float horizontal = (state.scale_x + state.scale_x) / (float)viewport_width;
        float vertical = (state.scale_y * -2.0f) / (float)viewport_height;
        if (state.angle_radians == 0) {
            unrotated(rectangle, state, horizontal, vertical, &quad);
            drawn = unrotated_visible(rectangle, state, &quad);
        } else {
            rotated(rectangle, state, rotation, horizontal, vertical, &quad);
            drawn = rotated_visible(&quad);
        }
    }
    for (size_t index = 0; index < 4; ++index) {
        AbDrawVertex *vertex = quad.vertices + index;
        vertex->u = uv[index * 2];
        vertex->v = uv[index * 2 + 1];
        if (!isfinite(vertex->x) || !isfinite(vertex->y) || !isfinite(vertex->u) ||
            !isfinite(vertex->v)) {
            return ab_fail(error, "Image rectangle exceeds finite range");
        }
    }
    *output = quad;
    *visible = drawn;
    return true;
}

bool ab_image_rectangle_command(AbImageRectangle rectangle, AbSpriteRenderState state,
                                uint32_t viewport_width, uint32_t viewport_height,
                                const float uv[8], AbImageRectangleMaterial material,
                                AbDrawCommand *output, bool *visible, AbError *error) {
    AbDrawQuad quad = {0};
    bool drawn = false;
    if (!output || !visible ||
        !ab_image_rectangle_quad(rectangle, state, viewport_width, viewport_height, uv,
                                 &quad, &drawn, error)) {
        return ab_fail(error, "Invalid rectangle image command geometry/outputs");
    }
    if (!drawn) {
        *visible = false;
        return true;
    }
    bool alpha = material.has_alpha || material.render_alpha < 1;
    if (!material.texture || (alpha && !isfinite(material.render_alpha))) {
        return ab_fail(error, "Invalid default rectangle image material");
    }
    *output = (AbDrawCommand){.blend = alpha ? AB_BLEND_PREMULTIPLIED : AB_BLEND_OPAQUE,
                              .texture = material.texture,
                              .quad = quad,
                              .color = {1, 1, 1, 1},
                              .opacity = alpha ? material.render_alpha : 1,
                              .coordinates = AB_DRAW_CLIP};
    *visible = true;
    return true;
}
