#ifndef AB_UI_BG_BOX_H
#define AB_UI_BG_BOX_H

#include "ab/ui_widget.h"

typedef enum {
    AB_UI_BOX_TOP_LEFT,
    AB_UI_BOX_TOP_RIGHT,
    AB_UI_BOX_BOTTOM_LEFT,
    AB_UI_BOX_BOTTOM_RIGHT,
    AB_UI_BOX_CENTER,
    AB_UI_BOX_TOP,
    AB_UI_BOX_LEFT,
    AB_UI_BOX_RIGHT,
    AB_UI_BOX_BOTTOM,
    AB_UI_BOX_COMPONENT_COUNT,
} AbUiBoxComponent;

typedef struct {
    AbUiWidget widget;
    AbUiPointerValue components[AB_UI_BOX_COMPONENT_COUNT];
    AbUiPointerValue scale_it, floor_coordinates;
    AbUiPointerValue horizontal_anchor, vertical_anchor;
    float width, height;
} AbUiBgBox;

/* Concrete BGBox with a real Frame base and borrowed resource bindings. Its
 * constructor replaces position/size/components after Frame.init. Stable fresh
 * storage and the widget ownership rules apply. Source drawSelf resets the five
 * ordinary geometry fields before drawing; alpha/direct mode remain live.
 * Absent components become empty identifiers on draw. Empty identifiers produce
 * no sprite; nonempty missing resources fail. Full original panel-pixel parity
 * remains separate from this source-derived composition. */
bool ab_ui_bg_box_init(AbUiBgBox *box, AbUiWidgetContext *context,
                       AbUiWidgetChildren children, const AbUiFrameFields *arguments,
                       AbError *error);
bool ab_ui_bg_box_edge(AbUiBgBox *box, AbUiBoxComponent edge, float *output,
                       AbError *error);
bool ab_ui_bg_box_total(AbUiBgBox *box, bool horizontal, float *output, AbError *error);
bool ab_ui_bg_box_set_total(AbUiBgBox *box, bool horizontal, float total,
                            AbError *error);
void ab_ui_bg_box_set_size(AbUiBgBox *box, float width, float height);
void ab_ui_bg_box_use_scale(AbUiBgBox *box, AbUiPointerValue value);
bool ab_ui_bg_box_draw_self(AbUiBgBox *box, const AbUiDrawInput *input, AbError *error);
/* Direct drawBox preserves source component mutation, bounds query order and
 * last discarded bound (original global _) in the shared widget context. Color
 * is optional; absent channels default to one. A colored center requires an
 * actual uploaded solid-white texture, borrowed as solid_texture. */
bool ab_ui_bg_box_draw_box(AbUiBgBox *box, AbVec2 position, AbVec2 size,
                           const AbUiPointerValue *color, uint32_t solid_texture,
                           AbError *error);
bool ab_ui_bg_box_draw_base(AbUiBgBox *box, const AbUiDrawInput *input, AbError *error);

#endif
