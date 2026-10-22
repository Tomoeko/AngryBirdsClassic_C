#ifndef AB_SCREEN_CLIP_H
#define AB_SCREEN_CLIP_H

#include "ab/draw.h"
#include "ab/physics_geometry.h"

typedef struct {
    float x, y, width, height;
} AbScreenClipRect;

/* gamelogic.setScreenClipRect10410 followed by LuaResources.setClipRect.
 * Add the actual optional screen offset (NULL selects the source's {0,0}),
 * then round each edge independently with roundf, including negative halves.
 * Preserve binary32 addition order. Require finite used values, signed32
 * rounded edges and an ordered rectangle. Failure preserves output. The
 * retained request is not clamped; backend attachment intersection is separate.
 * No allocation, rendering, graphics state reset or fabricated offset. */
bool ab_screen_clip(AbScreenClipRect request, const AbVec2 *offset, AbDrawClip *output,
                    AbError *error);

#endif
