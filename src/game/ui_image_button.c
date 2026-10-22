#include "ab/ui_image_button.h"

#include "ui_image_button_internal.h"

#include <math.h>
#include <string.h>

static bool named(AbSpan text, const char *name) {
    size_t size = strlen(name);
    return text.size == size && (!size || memcmp(text.data, name, size) == 0);
}

static AbUiPointerValue text_value(const char *text) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)text, strlen(text)}};
}

static bool false_value(const AbUiPointerValue *value) {
    return value->kind == AB_UI_POINTER_BOOLEAN && !value->boolean;
}

static bool inputs_valid(const AbUiImageButton *button,
                         const AbUiImageButtonBoundaries *boundaries, AbSpan event,
                         AbVec2 point, const AbUiPointerReply *output) {
    return button && boundaries && output && (!event.size || event.data) &&
           isfinite(point.x) && isfinite(point.y) &&
           isfinite(boundaries->touch_count) &&
           ab_ui_pointer_value_valid(&button->enabled) &&
           ab_ui_pointer_value_valid(&button->return_value) &&
           ab_ui_pointer_value_valid(&button->disabled_return_value) &&
           ab_ui_pointer_value_valid(&button->meta) &&
           ab_ui_pointer_value_valid(&button->click_sound);
}

static bool hit_test(const AbUiImageButtonBoundaries *boundaries, AbVec2 point,
                     bool *hit, AbError *error) {
    if (!boundaries->hit_test) {
        return ab_fail(error, "Missing ImageButton hit-test boundary");
    }
    return boundaries->hit_test(boundaries->context, point, hit, error);
}

static void set_click_started(AbUiImageButton *button, bool value,
                              AbUiButtonFlagSink sink, void *context) {
    button->click_started = value;
    if (sink) {
        sink(context, AB_UI_BUTTON_CLICK_STARTED, value);
    }
}

static bool handle(AbUiImageButton *button, const AbUiImageButtonBoundaries *boundaries,
                   AbSpan event, AbVec2 point, AbUiButtonFlagSink sink, void *context,
                   AbUiPointerReply *output, AbError *error) {
    if (!inputs_valid(button, boundaries, event, point, output)) {
        return ab_fail(error, "Invalid ImageButton pointer input");
    }
    bool hit;
    if (named(event, "HOVER") && button->visible && button->image_present) {
        if (!hit_test(boundaries, point, &hit, error)) {
            return false;
        }
        if (hit) {
            *output = (AbUiPointerReply){
                .values = {text_value("HOVER"),
                           {0},
                           {.kind = AB_UI_POINTER_OBJECT, .object = button->object}}};
            return true;
        }
    }
    if (button->activate_on_release && named(event, "LPRESS") && button->visible &&
        button->image_present) {
        if (!hit_test(boundaries, point, &hit, error)) {
            return false;
        }
        if (hit) {
            set_click_started(button, true, sink, context);
        }
    }
    if (button->click_started && boundaries->device_ipad &&
        boundaries->touch_count >= 4) {
        set_click_started(button, false, sink, context);
    }
    bool activation =
        button->activate_on_release ? named(event, "LRELEASE") : named(event, "LPRESS");
    if (activation) {
        bool enabled = !false_value(&button->enabled) ||
                       button->disabled_return_value.kind != AB_UI_POINTER_NONE;
        if (enabled && button->image_present) {
            if (!hit_test(boundaries, point, &hit, error)) {
                return false;
            }
            if (hit && (button->click_started || !button->activate_on_release)) {
                if (ab_ui_pointer_value_truthy(&button->click_sound)) {
                    if (!boundaries->play_audio) {
                        return ab_fail(error, "Missing ImageButton audio boundary");
                    }
                    if (!boundaries->play_audio(boundaries->context,
                                                button->click_sound, 1, false, error)) {
                        return false;
                    }
                }
                AbUiPointerValue value = ab_ui_pointer_value_truthy(&button->enabled)
                                             ? button->return_value
                                             : button->disabled_return_value;
                *output = (AbUiPointerReply){.values = {value,
                                                        button->meta,
                                                        {.kind = AB_UI_POINTER_OBJECT,
                                                         .object = button->object}}};
                return true;
            }
            set_click_started(button, false, sink, context);
        }
    }
    if (button->click_started) {
        if (!hit_test(boundaries, point, &hit, error)) {
            return false;
        }
        if (hit && button->visible && button->image_present) {
            *output = (AbUiPointerReply){.values = {text_value("CLICK"), {0}, {0}}};
            return true;
        }
    }
    *output = (AbUiPointerReply){0};
    return true;
}

bool ab_ui_image_button_dispatch(AbUiImageButton *button,
                                 const AbUiImageButtonBoundaries *boundaries,
                                 AbSpan event, AbVec2 point, bool traverse_children,
                                 AbUiButtonFlagSink sink, void *context,
                                 AbUiPointerReply *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing ImageButton pointer output");
    }
    AbUiPointerReply reply;
    if (!handle(button, boundaries, event, point, sink, context, &reply, error)) {
        return false;
    }
    if (!traverse_children) {
        *output = reply;
        return true;
    }
    const AbUiPointerValue *primary = reply.values;
    if (primary->kind != AB_UI_POINTER_NONE) {
        if (primary->kind != AB_UI_POINTER_TEXT || !named(primary->text, "CLICK")) {
            button->should_clear_pointer_flags = true;
            if (sink) {
                sink(context, AB_UI_BUTTON_CLEAR_POINTER_FLAGS, true);
            }
        }
        *output = reply;
        return true;
    }
    return ab_ui_frame_pointer(boundaries->frame_position, boundaries->children,
                               boundaries->child_count, event, point, output, error);
}

bool ab_ui_image_button_handle(AbUiImageButton *button,
                               const AbUiImageButtonBoundaries *boundaries,
                               AbSpan event, AbVec2 point, AbUiPointerReply *output,
                               AbError *error) {
    return ab_ui_image_button_dispatch(button, boundaries, event, point, false, NULL,
                                       NULL, output, error);
}

bool ab_ui_image_button_pointer(AbUiImageButton *button,
                                const AbUiImageButtonBoundaries *boundaries,
                                AbSpan event, AbVec2 point, AbUiPointerReply *output,
                                AbError *error) {
    return ab_ui_image_button_dispatch(button, boundaries, event, point, true, NULL,
                                       NULL, output, error);
}

void ab_ui_image_button_clear_flags(AbUiImageButton *button) {
    if (button) {
        button->click_started = false;
        button->should_clear_pointer_flags = false;
    }
}

void ab_ui_image_button_clear_events(AbUiImageButton *button) {
    if (button) {
        button->click_started = false;
    }
}
