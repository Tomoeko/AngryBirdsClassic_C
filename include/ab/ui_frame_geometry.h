#ifndef AB_UI_FRAME_GEOMETRY_H
#define AB_UI_FRAME_GEOMETRY_H

#include "ab/base.h"
#include "ab/physics_geometry.h"

typedef struct AbUiFrameGeometry {
    const struct AbUiFrameGeometry *parent;
    AbVec2 position;
    AbVec2 scale;
    AbVec2 scroll;
    uint32_t scroll_present;
} AbUiFrameGeometry;

/* Original Frame numeric queries; borrowed parent chain, no allocation. Depth
 * includes self and bounds traversal/cycles. Screen position starts at self's
 * x/y, adds each ancestor then subtracts that ancestor's scroll only when both
 * numeric fields are present (bits 1/2). Self scroll, scale and angle do not
 * enter this query. Cumulative scale multiplies self first, then its ancestors;
 * a NULL frame returns unit scale. Used values/results must remain finite.
 * On failure preserve output. Resource layout, drawing and input routing are
 * separate; these functions do not combine the two queries into an affine map. */
bool ab_ui_frame_screen_position(const AbUiFrameGeometry *frame, size_t maximum_depth,
                                 AbVec2 *output);
bool ab_ui_frame_cumulative_scale(const AbUiFrameGeometry *frame, size_t maximum_depth,
                                  AbVec2 *output);

typedef struct {
    AbVec2 position;
    AbVec2 size;
    AbVec2 scale;
    /* Present numeric scales, including zero; absent/false selects one. */
    uint32_t scale_present;
} AbUiInvisibleGeometry;

/* Original InvisibleButton centered inclusive rectangle. Preserve width-half
 * subtraction separately from the left half, and retain signed scales without
 * taking absolute values or swapping reversed edges. Used geometry/point and
 * all four bounds must be finite. Failure preserves output. No parent lookup,
 * rotation, visibility, active state, pointer flags or callbacks execute. */
bool ab_ui_invisible_hit(const AbUiInvisibleGeometry *geometry, AbVec2 point,
                         bool *output);

#endif
