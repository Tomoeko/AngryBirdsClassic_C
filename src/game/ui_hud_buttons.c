#include "ab/ui_hud_buttons.h"

#include <math.h>

static AbUiPointerValue boolean(bool value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = value};
}

static bool pause_draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                       AbError *error) {
    AbUiHudPauseButton *button = context;
    if (ab_ui_pointer_value_truthy(button->using_gesture_controls)) {
        widget->state.enabled = boolean(false);
        return true;
    }
    if (!input)
        return ab_fail(error, "Missing PauseButton parent draw arguments");
    AbUiDrawInput parent = *input;
    parent.present &= 3;
    if (!ab_ui_widget_draw_base(widget, &parent, error))
        return false;
    widget->state.enabled = boolean(true);
    return true;
}

bool ab_ui_hud_pause_button_init(AbUiHudPauseButton *button, AbUiWidgetContext *context,
                                 AbUiWidgetChildren children,
                                 const AbUiImageButtonArguments *arguments, bool osx,
                                 const AbUiPointerValue *using_gesture_controls,
                                 AbError *error) {
    if (!button || (osx && !using_gesture_controls))
        return ab_fail(error, "Missing actual PauseButton gesture binding");
    if (!ab_ui_widget_init(&button->widget, context, AB_UI_WIDGET_IMAGE_BUTTON,
                           children, NULL, NULL, arguments, error))
        return false;
    button->using_gesture_controls = using_gesture_controls;
    if (osx)
        button->widget.methods =
            (AbUiWidgetMethods){.context = button, .draw = pause_draw};
    return true;
}

static bool skip_update(void *context, AbUiWidget *widget, float seconds, float time,
                        AbError *error) {
    AbUiHudSkipButton *button = context;
    if (!ab_ui_widget_update_base(widget, seconds, time, error))
        return false;
    switch (button->state) {
        case AB_UI_SKIP_APPEAR:
            button->scale_multiplier = button->scale_multiplier + seconds * 3;
            if (!isfinite(button->scale_multiplier))
                return ab_fail(error, "SkipButton appearance scale overflow");
            if (button->scale_multiplier >= 1) {
                button->scale_multiplier = 1;
                button->state = AB_UI_SKIP_ONSCREEN;
            }
            break;
        case AB_UI_SKIP_DISAPPEAR:
            widget->state.return_value = (AbUiPointerValue){0};
            button->scale_multiplier = button->scale_multiplier - seconds * 3;
            if (!isfinite(button->scale_multiplier))
                return ab_fail(error, "SkipButton disappearance scale overflow");
            if (button->scale_multiplier <= 0) {
                button->scale_multiplier = 0;
                button->state = AB_UI_SKIP_GONE;
                widget->frame->fields.visible = boolean(false);
            }
            break;
        case AB_UI_SKIP_CLICKED:
            widget->state.return_value = (AbUiPointerValue){0};
            break;
        case AB_UI_SKIP_HIDDEN:
        case AB_UI_SKIP_ONSCREEN:
        case AB_UI_SKIP_GONE:
            break;
        default:
            return ab_fail(error, "Unsupported SkipButton state");
    }
    return true;
}

static bool skip_draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                      AbError *error) {
    AbUiHudSkipButton *button = context;
    /* Source multiplies parameters 3/4 directly. Missing scales have no source
     * default at this subclass boundary; normal Frame traversal supplies both. */
    if (!input || (input->present & 12) != 12)
        return ab_fail(error, "SkipButton draw requires actual parent scales");
    AbUiDrawInput scaled = *input;
    scaled.transform.scale.x = input->transform.scale.x * button->scale_multiplier;
    scaled.transform.scale.y = input->transform.scale.y * button->scale_multiplier;
    return ab_ui_widget_draw_base(widget, &scaled, error);
}

bool ab_ui_hud_skip_button_init(AbUiHudSkipButton *button, AbUiWidgetContext *context,
                                AbUiWidgetChildren children, void *query_context,
                                bool (*is_level_ending)(void *context,
                                                        AbUiPointerValue *output,
                                                        AbError *error),
                                AbError *error) {
    if (!button)
        return ab_fail(error, "Missing SkipButton owner");
    if (!ab_ui_widget_init(&button->widget, context, AB_UI_WIDGET_IMAGE_BUTTON,
                           children, NULL, NULL, NULL, error))
        return false;
    button->scale_multiplier = 0;
    button->state = AB_UI_SKIP_HIDDEN;
    AbUiPointerValue ending;
    if (!is_level_ending) {
        context->failed = true;
        return ab_fail(error, "Missing actual isLevelEnding query");
    }
    if (!is_level_ending(query_context, &ending, error)) {
        context->failed = true;
        return false;
    }
    if (!ab_ui_pointer_value_valid(&ending)) {
        context->failed = true;
        return ab_fail(error, "Invalid actual isLevelEnding result");
    }
    if (ab_ui_pointer_value_truthy(&ending)) {
        button->state = AB_UI_SKIP_ONSCREEN;
        button->scale_multiplier = 1;
        button->widget.frame->fields.visible = boolean(true);
    }
    button->widget.methods = (AbUiWidgetMethods){
        .context = button, .draw = skip_draw, .update = skip_update};
    return true;
}
