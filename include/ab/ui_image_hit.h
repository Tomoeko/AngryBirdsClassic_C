#ifndef AB_UI_IMAGE_HIT_H
#define AB_UI_IMAGE_HIT_H

#include "ab/ui_frame_geometry.h"

typedef struct {
    AbVec2 center;
    AbVec2 size;
    float angle_radians;
} AbUiHitRectangle;

/* Native MathUtils rectangle query. Edges are excluded. Preserve binary32
 * projection order and native sinf/cosf separately from Lua's double trig.
 * Inputs and projections must stay finite; the rotation module's angle bound
 * applies. Failure preserves output. Negative sizes are not reordered. */
bool ab_ui_rectangle_hit(const AbUiHitRectangle *rectangle, AbVec2 point, bool *output);

typedef struct {
    const AbUiFrameGeometry *frame;
    AbVec2 sprite_size;
    AbVec2 sprite_pivot;
    float padding_left;
    float padding_right;
    float padding_top;
    float padding_bottom;
    float angle_radians;
    bool image_present;
} AbUiImageHit;

/* ImageButton.hitTest geometry. Borrow the frame chain and explicit resource
 * bounds/pivot; callers resolve absent/false padding to zero. Multiply scales
 * self-to-root then take their absolute values. Use self position without
 * ancestor position/scroll. Rotate opposite corners with Lua trig, then derive
 * their midpoint. A zero cumulative scale or absent image returns present=false
 * without touching rectangle. Used values/results must be finite and |angle|
 * <= 1024. Failure preserves both outputs; no allocation or resource lookup. */
bool ab_ui_image_button_rectangle(const AbUiImageHit *button, size_t maximum_depth,
                                  AbUiHitRectangle *rectangle, bool *present);
bool ab_ui_image_button_hit(const AbUiImageHit *button, size_t maximum_depth,
                            AbVec2 point, bool *output);

#endif
