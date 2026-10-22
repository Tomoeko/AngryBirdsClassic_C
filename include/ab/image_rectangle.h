#ifndef AB_IMAGE_RECTANGLE_H
#define AB_IMAGE_RECTANGLE_H

#include "ab/draw.h"

typedef struct {
    float x;
    float y;
    float width;
    float height;
} AbImageRectangle;

typedef struct {
    float angle_radians;
    float translation_x;
    float translation_y;
    float scale_x;
    float scale_y;
    float rotation_origin_x;
    float rotation_origin_y;
    bool direct_vertices;
} AbSpriteRenderState;

/* GL_Image's rectangle route: viewport-normalized clip coordinates, or raw
 * coordinates when direct_vertices is set. Off-screen rejection follows the
 * target's separate rotated and unrotated branches. UVs remain caller inputs.
 * No allocation. Invalid inputs preserve both outputs. */
bool ab_image_rectangle_quad(AbImageRectangle rectangle, AbSpriteRenderState state,
                             uint32_t viewport_width, uint32_t viewport_height,
                             const float uv[8], AbDrawQuad *output, bool *visible,
                             AbError *error);

typedef struct {
    uint32_t texture;
    bool has_alpha;
    float render_alpha;
} AbImageRectangleMaterial;

/* Ordinary default rectangle shaders, with no tint or shader override. Textures
 * with alpha, or render_alpha < 1, select premultiplied blending and multiply
 * all channels by render_alpha. The opaque route ignores alpha. A culled
 * success sets visible=false, preserving the
 * command and ignoring material values. Visible success requires a nonzero
 * texture and finite used alpha. Failure preserves both outputs. */
bool ab_image_rectangle_command(AbImageRectangle rectangle, AbSpriteRenderState state,
                                uint32_t viewport_width, uint32_t viewport_height,
                                const float uv[8], AbImageRectangleMaterial material,
                                AbDrawCommand *output, bool *visible, AbError *error);

#endif
