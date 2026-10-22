#ifndef AB_UI_TEXT_DRAW_H
#define AB_UI_TEXT_DRAW_H

#include "ab/font_layout.h"
#include "ab/rotation.h"
#include "ab/ui_frame_draw.h"

typedef struct {
    AbVec2 position, scale, rotation_pivot;
    AbSpan font, group, text;
    AbVerticalAnchor vertical;
    AbHorizontalAnchor horizontal;
    float alpha;
    uint32_t rotation_pivot_present;
    bool font_present, visible, floor_coordinates, alpha_present;
    bool clipped, color_present;
} AbUiTextDraw;

typedef struct {
    AbSpan font, group, text;
    AbFontPlacement layout;
    AbRotation rotation;
    AbVec2 scale;
    float angle, draw_alpha, after_alpha;
    bool draw, alpha_override;
} AbUiTextDrawPlan;

/* Native Android drawUIText's unclipped/uncolored value boundary. Visible is
 * resolved by original Lua truthiness. Hidden text skips all other fields.
 * Visible text requires 2-6 finite numeric parent arguments in x,y,sx,sy,angle,
 * factor order. Three arguments still use unit parent scales: both scales are
 * read only with at least four arguments. Angle/factor default to zero/one.
 * Scale order is parent*(self*factor); position is parent+self*factor, divided
 * by final scale and then optionally floored. Self angle is not an input.
 * Rotation pivots are used only when both are numeric. Numeric alpha below one
 * overrides drawing alpha and restores one; other alpha preserves context.
 * The plan selects a font request (default FONT_BASIC), borrowed group/text and
 * unit-scale native font layout. Resource lookup, glyph layout, render-state
 * application and child traversal are separate. Finite nonzero final scales,
 * finite positions/alpha/pivots and the rotation module's angle domain only.
 * No allocation or callbacks. Failure preserves output. */
bool ab_ui_text_draw_plan(const AbUiTextDraw *text, const float *arguments,
                          size_t count, float current_alpha, AbUiTextDrawPlan *output);

/* Original clipped-text vertical visibility helper. Width, text, x, horizontal
 * anchor and horizontal scale are unused at this boundary. Preserve signed
 * font metrics/scales and operation order; do not reorder bounds for negative
 * scales. Only vertical values VCENTER(1) and BOTTOM(2) shift the interval.
 * Top equal to screen height rejects; bottom equal to zero accepts. IEEE
 * unordered comparisons follow the native helper, including nonfinite inputs.
 * No allocation, resource lookup or floating-point status-flag guarantee. */
bool ab_ui_text_on_screen(int32_t max_ascending, int32_t max_descending, float y,
                          float scale_y, int32_t vertical, int32_t screen_height);

typedef struct {
    AbVec2 parent_position, position, scale;
    AbSpan group;
    float factor, angle;
    bool visible;
} AbUiTextClipFrame;

/* Prepare one original clipped root's numeric frame. Hidden roots skip all
 * remaining fields. A visible root uses its position, scales and group;
 * its font, text, anchors, color, alpha and rotation pivots are unused.
 * Same finite numeric argument/final-scale domain as the leaf draw plan. */
bool ab_ui_text_clip_begin(const AbUiTextDraw *root, const float *arguments,
                           size_t count, AbUiTextClipFrame *output);

/* Geometry phase for an unclipped/uncolored prepared line. Native traversal
 * ignores the line's visible flag and uses the root group. First test vertical
 * visibility at parent+(root_position+line_position)*factor with root scale,
 * before multiplying line scales. A surviving line receives those supplied
 * coordinates/anchors and unit local factor; do not add line position again.
 * Root and line inputs are borrowed/stable. The caller retains ordered line
 * traversal, current font selection/metrics and context alpha between calls.
 * Native localization lookups before culling and again during drawing remain
 * separate resource boundaries. This function does not perform them. Nested
 * clipped lines and color effects remain unsupported. No allocation/callbacks;
 * failure preserves output. */
bool ab_ui_text_clip_line_plan(const AbUiTextClipFrame *frame, const AbUiTextDraw *line,
                               const AbBitmapFont *current_font, int32_t screen_height,
                               float current_alpha, AbUiTextDrawPlan *output);

#endif
