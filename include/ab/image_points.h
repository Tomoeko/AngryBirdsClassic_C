#ifndef AB_IMAGE_POINTS_H
#define AB_IMAGE_POINTS_H

#include "ab/draw.h"
#include "ab/physics_geometry.h"

/* GL_Image's per-vertex-tint point route, restricted to z=0 sprite vertices.
 * Input points already carry their transform. This route uses viewport extents
 * only, without applying current render-state translation, scale or direct mode.
 * Normalize each point to clip space; cull by its complete four-point bounds.
 * No allocation. Finite inputs/results and positive signed-32-bit extents are
 * required; failure preserves both outputs. UVs/order remain unchanged. Shader,
 * color/alpha policy, batch lifetime and actual GPU submission are separate. */
bool ab_image_points_quad(const AbVec2 points[4], uint32_t viewport_width,
                          uint32_t viewport_height, const float uv[8],
                          AbDrawQuad *output, bool *visible, AbError *error);

typedef struct {
    uint32_t texture;
    bool has_alpha;
    float render_alpha;
} AbImagePointMaterial;

/* Default point-image shaders selected by resolved texture alpha presence.
 * Native 2d-sprite uses opaque sampling; 2d-sprite-alpha uses premultiplied
 * blending and multiplies every sampled channel by render_alpha. These default
 * resources do not use per-vertex tint. Shader overrides/effects are outside
 * this contract. Geometry is already transformed, as above. Culled success
 * sets visible=false, preserving command and ignoring material fields. Visible
 * success requires a nonzero texture and finite used alpha; opaque alpha is
 * unused. Failure preserves both outputs. No allocation or resource lookup.
 * Resource interpretation/CPU policy is verified; GPU pixels remain separate. */
bool ab_image_points_command(const AbVec2 points[4], uint32_t viewport_width,
                             uint32_t viewport_height, const float uv[8],
                             AbImagePointMaterial material, AbDrawCommand *output,
                             bool *visible, AbError *error);

#endif
