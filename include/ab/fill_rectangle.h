#ifndef AB_FILL_RECTANGLE_H
#define AB_FILL_RECTANGLE_H

#include "ab/draw.h"

typedef enum {
    AB_FILL_BLEND_AUTO,
    AB_FILL_BLEND_ALPHA,
    AB_FILL_BLEND_OPAQUE,
} AbFillBlend;

typedef struct {
    uint32_t argb;
    int32_t left, top, width, height;
    AbFillBlend blend;
} AbFillRectangle;

/* GL_Context.fillRect2D after the native full identity-state reset. Preserve
 * integer-to-float/add order and four-corner culling.
 * Automatic blending selects straight alpha when packed alpha is not255; explicit alpha
 * and opaque modes override it. The caller supplies an owned solid-white texture
 * for the shared textured-command boundary. Native fill itself is untextured.
 * Output uses clip coordinates, normalized ARGB channels and opacity1. Native
 * z=0.001 is omitted at this depth-disabled 2D boundary. Arbitrary render state,
 * GPU color packing, rasterization and pixels remain separate. Culled success
 * preserves command and ignores the texture; failure preserves both outputs.
 * Positive int32 viewport required; no allocation or resource lookup. */
bool ab_fill_rectangle_reset_command(AbFillRectangle rectangle, uint32_t viewport_width,
                                     uint32_t viewport_height, uint32_t white_texture,
                                     AbDrawCommand *output, bool *visible,
                                     AbError *error);

#endif
