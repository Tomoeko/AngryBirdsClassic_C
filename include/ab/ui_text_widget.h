#ifndef AB_UI_TEXT_WIDGET_H
#define AB_UI_TEXT_WIDGET_H

#include "ab/ui_font_resources.h"
#include "ab/ui_text_bounds.h"

typedef struct AbUiTextClipping AbUiTextClipping;

typedef struct AbUiTextWidget {
    AbUiWidget widget;
    AbUiTextState state;
    AbUiFontResources *fonts;
    AbUiPointerValue alpha, rotation_pivot_x, rotation_pivot_y;
    AbUiPointerValue clipped, color;
    AbUiTextClipping *clipping;
} AbUiTextWidget;

/* Actual Text constructor, lifecycle, native leaf draw, and subsequent Frame
 * child traversal. Use one canonical Frame in state, with caller-owned children
 * and real decoded font/catalog resources. This initial concrete route supports
 * uncolored text with native raw-value conversion. Clipped drawing requires the
 * actual lines owner in ui_text_clipping.h; color effects fail explicitly.
 * Numeric conversion borrows only stack scratch
 * through submission and does not change the canonical text field.
 * Getters and setters preserve the existing recovered Text method semantics. */
bool ab_ui_text_widget_init(AbUiTextWidget *text, AbUiWidgetContext *context,
                            AbUiWidgetChildren children, AbUiFontResources *fonts,
                            const AbUiFrameFields *frame,
                            const AbUiTextArguments *arguments, AbError *error);
bool ab_ui_text_widget_set_font(AbUiTextWidget *text, AbUiPointerValue font,
                                AbError *error);
bool ab_ui_text_widget_get_width(AbUiTextWidget *text, float *output, AbError *error);
bool ab_ui_text_widget_get_height(AbUiTextWidget *text, float *output, AbError *error);

#endif
