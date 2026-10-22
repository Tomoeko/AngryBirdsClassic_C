#ifndef AB_UI_SLIDER_H
#define AB_UI_SLIDER_H

#include "ab/ui_bg_box.h"
#include "ab/ui_scaling.h"

typedef enum {
    AB_UI_SLIDER_CLOSED,
    AB_UI_SLIDER_OPENING,
    AB_UI_SLIDER_OPEN,
    AB_UI_SLIDER_CLOSING,
    AB_UI_SLIDER_DISAPPEAR,
    AB_UI_SLIDER_DISAPPEARED,
} AbUiSliderState;

typedef enum {
    AB_UI_SLIDER_LINEAR,
    AB_UI_SLIDER_CUBIC_OUT,
    AB_UI_SLIDER_CUBIC_IN_OUT,
    AB_UI_SLIDER_CUBIC_IN,
} AbUiSliderCurve;

typedef struct {
    AbUiWidget *widget;
    AbUiPointerValue offset_x, offset_y, override_size;
} AbUiSliderChild;

typedef struct {
    AbUiScaling *scaling;
    const AbVec2 *screen;
    void *context;
    bool (*notifications_visible)(void *context, bool *output, AbError *error);
} AbUiSliderHost;

typedef struct {
    AbUiBgBox box;
    AbUiWidget toggle, indicator;
    AbUiSliderChild **items;
    size_t count, capacity;
    AbUiSliderHost host;
    AbUiPointerValue enabled, open_image, closed_image, open_indicator,
        closed_indicator;
    AbVec2 direction, padding;
    float gap, child_size, extend_state, extend_adjusted, disappear_scale;
    AbUiSliderState state;
    AbUiSliderCurve open_curve;
    bool rotate_indicator;
} AbUiSlider;

/* Concrete ui_components/Slider.lua for finite numeric layout, ordinary owned
 * widgets and its four builtin opening curves. Own the BGBox,
 * toggle and indicator in fresh zeroed, stable caller storage. Borrow child records,
 * their raw offset/override fields, arrays and live scaling/screen/notification owner.
 * children capacity must cover all content plus the two internal controls;
 * items covers content only. Constructor copies hit padding to the toggle and
 * reads the supplied actual global enabled value with source false-or-nil
 * default true. No source effects are supplied by missing host callbacks.
 *
 * Content inserts before internal controls in source order. State transitions,
 * strict closing boundary, open-curve reuse during closing, direction-specific
 * positions/visibility and clip reset follow the target. setEnabled closes any
 * nonclosed state even when enabling. Draw children require Boolean true while
 * toggle/indicator draw directly. Custom functions, malformed numeric
 * fields, source's broken removeChild call, collection and retained arbitrary
 * table identities remain unsupported. No allocation, resource loading or VM;
 * failure retains source prefixes and stops the shared widget context. */
bool ab_ui_slider_init(AbUiSlider *slider, AbUiWidgetContext *context,
                       AbUiWidgetChildren children, AbUiSliderChild **items,
                       size_t capacity, const AbUiFrameFields *arguments,
                       const AbUiPointerValue padding[4], AbUiPointerValue enabled,
                       AbUiSliderHost host, AbError *error);
bool ab_ui_slider_add(AbUiSlider *slider, AbUiSliderChild *child, AbError *error);
bool ab_ui_slider_images(AbUiSlider *slider, AbUiPointerValue open,
                         AbUiPointerValue closed, bool indicator, AbError *error);
bool ab_ui_slider_enabled(AbUiSlider *slider, AbUiPointerValue enabled, AbError *error);
bool ab_ui_slider_direction(AbUiSlider *slider, AbSpan direction, AbError *error);
bool ab_ui_slider_curve(AbUiSlider *slider, AbUiSliderCurve curve, AbError *error);
bool ab_ui_slider_open(AbUiSlider *slider, AbError *error);
bool ab_ui_slider_close(AbUiSlider *slider, AbError *error);
bool ab_ui_slider_toggle(AbUiSlider *slider, AbError *error);
bool ab_ui_slider_disappear(AbUiSlider *slider, AbError *error);
bool ab_ui_slider_is_open(const AbUiSlider *slider);
bool ab_ui_slider_total(AbUiSlider *slider, float *output, AbError *error);
bool ab_ui_slider_duration(AbUiSlider *slider, float *output, AbError *error);
/* Source getButtonBounds: left/top/right/bottom, without parent transforms. */
bool ab_ui_slider_button_bounds(AbUiSlider *slider, float output[4], AbError *error);

#endif
