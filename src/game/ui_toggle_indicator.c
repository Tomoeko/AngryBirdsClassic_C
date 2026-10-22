#include "ab/ui_toggle_indicator.h"

#include <math.h>
#include <string.h>

static AbUiPointerValue text(const char *name) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)name, strlen(name)}};
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue result = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&result.number_bits, &value, sizeof(value));
    return result;
}

static bool named(AbUiPointerValue value, const char *name) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == strlen(name) &&
           (!value.text.size ||
            (value.text.data && !memcmp(value.text.data, name, value.text.size)));
}

static bool valid(AbUiToggleIndicator *indicator, AbError *error) {
    return indicator && indicator->widget.initialized && indicator->widget.context &&
                   !indicator->widget.context->failed
               ? true
               : ab_fail(error, "Invalid or failed ToggleIndicator");
}

bool ab_ui_toggle_indicator_toggle(AbUiToggleIndicator *indicator, AbError *error) {
    if (!valid(indicator, error))
        return false;
    if (named(indicator->state, "OPEN") || named(indicator->state, "OPENING"))
        indicator->state = text("CLOSING");
    else if (named(indicator->state, "CLOSED") || named(indicator->state, "CLOSING"))
        indicator->state = text("OPENING");
    return true;
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiToggleIndicator *indicator = context;
    if (widget != &indicator->widget ||
        !ab_ui_widget_lifecycle_base(widget, method, error))
        return false;
    if (method == AB_UI_FRAME_ENTRY)
        indicator->state = text("CLOSED");
    return true;
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    (void)time;
    AbUiToggleIndicator *indicator = context;
    if (widget != &indicator->widget)
        return ab_fail(error, "Foreign ToggleIndicator update");
    bool opening = named(indicator->state, "OPENING");
    if (!opening && !named(indicator->state, "CLOSING"))
        return true;
    AbUiPointerValue *field = &widget->state.image.frame.fields.angle;
    float angle;
    memcpy(&angle, &field->number_bits, sizeof(angle));
    if (field->kind != AB_UI_POINTER_NUMBER || !isfinite(angle))
        return ab_fail(error, "Unsupported ToggleIndicator angle");
    float step = seconds * 16;
    angle = opening ? angle + step : angle - step;
    *field = number(angle);
    if (opening) {
        uint32_t limit_bits = UINT32_C(0x40490fd0);
        float limit;
        memcpy(&limit, &limit_bits, sizeof(limit));
        if (limit <= angle) {
            field->number_bits = limit_bits;
            indicator->state = text("OPEN");
        }
    } else {
        if (angle < 0)
            *field = number(0);
        indicator->state = text("CLOSED");
    }
    return true;
}

bool ab_ui_toggle_indicator_init(AbUiToggleIndicator *indicator,
                                 AbUiWidgetContext *context,
                                 AbUiWidgetChildren children, AbError *error) {
    if (!indicator || indicator->widget.initialized)
        return ab_fail(error, "ToggleIndicator requires fresh caller storage");
    if (!ab_ui_widget_init(&indicator->widget, context, AB_UI_WIDGET_IMAGE, children,
                           NULL, NULL, NULL, error))
        return false;
    indicator->state = text("CLOSED");
    indicator->widget.methods = (AbUiWidgetMethods){
        .context = indicator, .lifecycle = lifecycle, .update = update};
    return true;
}
