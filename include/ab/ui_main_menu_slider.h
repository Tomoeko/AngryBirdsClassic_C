#ifndef AB_UI_MAIN_MENU_SLIDER_H
#define AB_UI_MAIN_MENU_SLIDER_H

#include "ab/ui_bg_box.h"
#include "ab/ui_scaling.h"

typedef struct {
    AbUiBgBox box;
    AbUiScaling *scaling;
    AbEventManager *events;
    AbDataValue state_event;
    AbUiPointerValue state;
    float slide_amount, element_width, element_height, bottom_margin;
    float border_width, unscaled_width, unscaled_height;
    bool height_assigned;
} AbUiMainMenuSlider;

/* Concrete MainMenuSlider over real BGBox/Frame and Scaling owners. Resource
 * dimensions come from the active APK registry. Notify actual EventManager
 * listeners before state assignment; event.item is AB_DATA_OBJECT pointing to
 * this owner's canonical box.widget, the same identity used by UI routing.
 * Event tables are synchronous borrowed call views. Receivers may issue nested
 * notifications or change the slider; retain/copy any payload they need beyond
 * the callback rather than queueing this temporary table itself.
 *
 * Initialize fresh caller storage, then enter before ordinary updates. Source
 * unscaledHeight is absent until onEntry; it is not filled by this constructor.
 * Child storage, providers, event ID and owners remain stable and borrowed.
 * Reached failures preserve source prefixes and stop the shared widget context.
 * Used geometry and arithmetic must be finite; no VM or frame allocation. */
bool ab_ui_main_menu_slider_init(AbUiMainMenuSlider *slider, AbUiWidgetContext *context,
                                 AbUiWidgetChildren children,
                                 const AbUiFrameFields *arguments, AbUiScaling *scaling,
                                 AbEventManager *events, AbDataValue state_event,
                                 AbError *error);
bool ab_ui_main_menu_slider_set_state(AbUiMainMenuSlider *slider,
                                      AbUiPointerValue state, AbError *error);
bool ab_ui_main_menu_slider_toggle(AbUiMainMenuSlider *slider, AbError *error);

#endif
