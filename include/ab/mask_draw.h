#ifndef AB_MASK_DRAW_H
#define AB_MASK_DRAW_H

#include "ab/draw.h"
#include "ab/physics_geometry.h"

typedef struct {
    const AbSpriteRegion *mask;
    AbVec2 position, scale, rotation_origin;
    AbTransform2 transform;
    uint32_t width, height;
    uint32_t texture_width, texture_height, mask_width, mask_height;
} AbMaskDrawInput;

typedef struct {
    AbDrawQuad quad;
    float texture_uv[8];
    bool visible;
} AbMaskDrawPlan;

typedef struct {
    uint32_t texture, width, height;
} AbMaskTexture;

/* MaskedImage.add's submitted clip quad and two independent UV sets. The quad
 * carries mask-atlas coordinates; texture_uv carries world-anchored terrain
 * coordinates. The caller supplies the current rotation matrix, translation,
 * scale and pivot at the native context boundary. Texture logical and uploaded
 * dimensions must match in this path. No sampling, blending, allocation or GPU
 * calls. Preserve output on malformed/nonfinite input; offscreen plans retain
 * their geometry and report visible=false. */
bool ab_mask_draw_plan(AbMaskDrawInput input, AbMaskDrawPlan *output, AbError *error);

#endif
