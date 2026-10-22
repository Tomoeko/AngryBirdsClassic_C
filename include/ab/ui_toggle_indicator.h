#ifndef AB_UI_TOGGLE_INDICATOR_H
#define AB_UI_TOGGLE_INDICATOR_H

#include "ab/ui_widget.h"

typedef struct {
    AbUiWidget widget;
    AbUiPointerValue state;
} AbUiToggleIndicator;

/* Concrete main-menu ToggleIndicator Image subclass. Constructor arguments are
 * ignored by the source; assign name/image/geometry afterward. Entry resets
 * state after the real Image/Frame base, retaining angle. Update does not visit
 * children. The source closing branch assigns CLOSED on its first update even
 * while angle is positive. Stable fresh caller storage and widget rules apply. */
bool ab_ui_toggle_indicator_init(AbUiToggleIndicator *indicator,
                                 AbUiWidgetContext *context,
                                 AbUiWidgetChildren children, AbError *error);
bool ab_ui_toggle_indicator_toggle(AbUiToggleIndicator *indicator, AbError *error);

#endif
