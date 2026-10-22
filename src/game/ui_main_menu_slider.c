#include "ab/ui_main_menu_slider.h"

#include <math.h>
#include <string.h>

static AbUiPointerValue number(float value) {
    AbUiPointerValue result = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&result.number_bits, &value, sizeof(value));
    return result;
}

static float real(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static AbUiPointerValue text(const char *value) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue key(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static bool named(AbUiPointerValue value, const char *name) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == strlen(name) &&
           (!value.text.size ||
            (value.text.data && !memcmp(value.text.data, name, value.text.size)));
}

static bool same(AbUiPointerValue first, AbUiPointerValue second) {
    if (first.kind != second.kind)
        return false;
    switch (first.kind) {
        case AB_UI_POINTER_NONE:
            return true;
        case AB_UI_POINTER_BOOLEAN:
            return first.boolean == second.boolean;
        case AB_UI_POINTER_NUMBER:
            return real(first.number_bits) == real(second.number_bits);
        case AB_UI_POINTER_TEXT:
            return first.text.size == second.text.size &&
                   (!first.text.size ||
                    !memcmp(first.text.data, second.text.data, first.text.size));
        case AB_UI_POINTER_OBJECT:
            return first.object == second.object;
        case AB_UI_POINTER_FUNCTION:
            return first.function == second.function;
    }
    return false;
}

static bool payload(AbUiPointerValue value, AbDataValue *output, AbError *error) {
    switch (value.kind) {
        case AB_UI_POINTER_NONE:
            *output = (AbDataValue){0};
            return true;
        case AB_UI_POINTER_BOOLEAN:
            *output = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = value.boolean};
            return true;
        case AB_UI_POINTER_NUMBER:
            *output = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = value.number_bits};
            return true;
        case AB_UI_POINTER_TEXT:
            *output = (AbDataValue){.kind = AB_DATA_STRING, .string = value.text};
            return true;
        case AB_UI_POINTER_OBJECT:
            *output = (AbDataValue){.kind = AB_DATA_OBJECT,
                                    .object = (const void *)value.object};
            return true;
        default:
            return ab_fail(error, "Unsupported MainMenuSlider state event value");
    }
}

static bool valid(AbUiMainMenuSlider *slider, AbError *error) {
    return slider && slider->box.widget.initialized && slider->box.widget.context &&
                   !slider->box.widget.context->failed
               ? true
               : ab_fail(error, "Invalid or failed MainMenuSlider");
}

static bool finish(AbUiMainMenuSlider *slider, bool okay) {
    if (!okay)
        slider->box.widget.context->failed = true;
    return okay;
}

static AbUiWidget *child_at(AbUiMainMenuSlider *slider, size_t index, AbError *error) {
    AbUiWidget *widget = &slider->box.widget;
    if (widget->tree.state->child_count > widget->children.capacity ||
        index >= widget->tree.state->child_count || !widget->children.frames) {
        ab_fail(error, "Invalid MainMenuSlider child storage");
        return NULL;
    }
    AbUiFrameNode *node = widget->children.frames[index];
    AbUiWidget *child = node ? node->context : NULL;
    if (!child || !child->initialized || &child->tree != node ||
        child->context != widget->context) {
        ab_fail(error, "Foreign MainMenuSlider child");
        return NULL;
    }
    return child;
}

static bool notify_state(AbUiMainMenuSlider *slider, AbUiPointerValue state,
                         AbError *error) {
    AbDataValue old = {0}, next = {0};
    if (!slider->events || !payload(slider->state, &old, error) ||
        !payload(state, &next, error))
        return ab_fail(error, "Missing MainMenuSlider event owner/payload");
    AbDataEntry fields[4];
    size_t count = 0;
    fields[count++] = (AbDataEntry){key("id"), slider->state_event};
    if (old.kind != AB_DATA_NIL)
        fields[count++] = (AbDataEntry){key("oldState"), old};
    if (next.kind != AB_DATA_NIL)
        fields[count++] = (AbDataEntry){key("newState"), next};
    fields[count++] = (AbDataEntry){
        key("item"), {.kind = AB_DATA_OBJECT, .object = &slider->box.widget}};
    AbDataTable event = {fields, count, 4, NULL};
    return ab_event_manager_notify(slider->events, &event, error);
}

bool ab_ui_main_menu_slider_set_state(AbUiMainMenuSlider *slider,
                                      AbUiPointerValue state, AbError *error) {
    if (!valid(slider, error) || !ab_ui_pointer_value_valid(&state))
        return ab_fail(error, "Invalid MainMenuSlider state");
    if (!same(slider->state, state) && !notify_state(slider, state, error))
        return finish(slider, false);
    slider->state = state;
    AbUiPointerValue visible = {.kind = AB_UI_POINTER_BOOLEAN,
                                .boolean = !named(state, "CLOSED")};
    slider->box.widget.state.image.frame.fields.visible = visible;
    for (size_t index = 0; index < slider->box.widget.tree.state->child_count;
         ++index) {
        AbUiWidget *child = child_at(slider, index, error);
        if (!child)
            return finish(slider, false);
        child->state.image.frame.fields.visible = visible;
    }
    return true;
}

bool ab_ui_main_menu_slider_toggle(AbUiMainMenuSlider *slider, AbError *error) {
    if (!valid(slider, error))
        return false;
    if (named(slider->state, "OPEN") || named(slider->state, "OPENING"))
        return ab_ui_main_menu_slider_set_state(slider, text("CLOSING"), error);
    if (named(slider->state, "CLOSED") || named(slider->state, "CLOSING"))
        return ab_ui_main_menu_slider_set_state(slider, text("OPENING"), error);
    return true;
}

static bool current_height(AbUiMainMenuSlider *slider, float *output, AbError *error) {
    if (!slider->height_assigned || !isfinite(slider->unscaled_height)) {
        ab_fail(error, "MainMenuSlider unscaledHeight is absent or nonfinite");
        return false;
    }
    *output = slider->unscaled_height;
    return true;
}

static bool layout(AbUiMainMenuSlider *slider, AbError *error) {
    AbUiFrameFields *fields = &slider->box.widget.state.image.frame.fields;
    if (!ab_ui_scaling_set_to(slider->scaling, fields, error))
        return false;
    float height = (float)slider->box.widget.tree.state->child_count;
    height = height * slider->element_height;
    height = height + slider->bottom_margin;
    float scale;
    if (!ab_ui_scaling_get(slider->scaling, &scale, error))
        return false;
    height = height / scale;
    if (!isfinite(height))
        return ab_fail(error, "Nonfinite MainMenuSlider layout height");
    fields->h = number(height);
    return true;
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiMainMenuSlider *slider = context;
    if (widget != &slider->box.widget)
        return ab_fail(error, "Foreign MainMenuSlider lifecycle");
    if (method == AB_UI_FRAME_LAYOUT)
        return layout(slider, error);
    if (!ab_ui_widget_lifecycle_base(widget, method, error))
        return false;
    if (method != AB_UI_FRAME_ENTRY)
        return true;
    slider->unscaled_height = 0;
    slider->height_assigned = true;
    return ab_ui_main_menu_slider_set_state(slider, text("CLOSED"), error);
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiMainMenuSlider *slider = context;
    return widget == &slider->box.widget &&
           ab_ui_bg_box_draw_base(&slider->box, input, error);
}

static bool pointer(void *context, AbUiWidget *widget, AbSpan event, AbVec2 point,
                    AbUiPointerReply *output, AbError *error) {
    AbUiMainMenuSlider *slider = context;
    if (!output || widget != &slider->box.widget)
        return ab_fail(error, "Invalid MainMenuSlider pointer reply/widget");
    if (named(slider->state, "OPEN"))
        return ab_ui_widget_pointer_base(widget, event, point, output, error);
    *output = (AbUiPointerReply){0};
    return true;
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiMainMenuSlider *slider = context;
    if (widget != &slider->box.widget)
        return ab_fail(error, "Foreign MainMenuSlider update");
    AbUiFrameFields *fields = &widget->state.image.frame.fields;
    float limit = real(fields->h.number_bits), scale, height;
    if (fields->h.kind != AB_UI_POINTER_NUMBER || !isfinite(limit))
        return ab_fail(error, "Missing MainMenuSlider layout height");
    if (!ab_ui_scaling_get(slider->scaling, &scale, error))
        return false;
    fields->scale_y = number(scale);
    if (!ab_ui_scaling_get(slider->scaling, &scale, error))
        return false;
    fields->scale_x = number(scale);
    if (named(slider->state, "OPENING") || named(slider->state, "CLOSING")) {
        if (!current_height(slider, &height, error))
            return false;
        float step = seconds * real(UINT32_C(0x4419c000));
        if (named(slider->state, "OPENING")) {
            slider->unscaled_height = height + step;
            if (limit <= slider->unscaled_height) {
                slider->unscaled_height = limit;
                if (!ab_ui_main_menu_slider_set_state(slider, text("OPEN"), error))
                    return false;
            }
        } else {
            slider->unscaled_height = height - step;
            if (slider->unscaled_height < 0) {
                slider->unscaled_height = 0;
                if (!ab_ui_main_menu_slider_set_state(slider, text("CLOSED"), error))
                    return false;
            }
        }
    }
    if (!ab_ui_scaling_get(slider->scaling, &scale, error))
        return false;
    float width = slider->unscaled_width * scale;
    if (!current_height(slider, &height, error) ||
        !ab_ui_scaling_get(slider->scaling, &scale, error))
        return false;
    ab_ui_bg_box_set_size(&slider->box, width, height * scale);
    if (named(slider->state, "OPEN") || named(slider->state, "CLOSING") ||
        named(slider->state, "CLOSED") || named(slider->state, "OPENING")) {
        size_t count = widget->tree.state->child_count;
        for (size_t index = 0; index < count; ++index) {
            AbUiWidget *child = child_at(slider, index, error);
            if (!child)
                return false;
            float y = -slider->box.height + (float)index * slider->element_height;
            y = y + slider->element_height * 0.5f;
            child->state.image.frame.fields.y = number(y >= 0 ? 0 : y);
        }
    }
    return ab_ui_widget_update_base(widget, seconds, time, error);
}

bool ab_ui_main_menu_slider_init(AbUiMainMenuSlider *slider, AbUiWidgetContext *context,
                                 AbUiWidgetChildren children,
                                 const AbUiFrameFields *arguments, AbUiScaling *scaling,
                                 AbEventManager *events, AbDataValue state_event,
                                 AbError *error) {
    if (!slider || slider->box.widget.initialized)
        return ab_fail(error, "MainMenuSlider requires fresh caller storage");
    if (!ab_ui_bg_box_init(&slider->box, context, children, arguments, error))
        return false;
    slider->scaling = scaling;
    slider->events = events;
    slider->state_event = state_event;
    slider->box.widget.methods = (AbUiWidgetMethods){.context = slider,
                                                     .lifecycle = lifecycle,
                                                     .draw = draw,
                                                     .pointer = pointer,
                                                     .update = update};
    ab_ui_bg_box_use_scale(
        &slider->box,
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true});
    if (!ab_ui_main_menu_slider_set_state(slider, text("CLOSED"), error))
        return false;
    slider->slide_amount = 0;
    static const char *const pieces[AB_UI_BOX_COMPONENT_COUNT] = {
        "MENU_SLIDER_BG_TOP_LEFT",
        "MENU_SLIDER_BG_TOP_RIGHT",
        NULL,
        NULL,
        "MENU_SLIDER_BG_CENTER",
        "MENU_SLIDER_BG_TOP_MIDDLE",
        "MENU_SLIDER_BG_LEFT",
        "MENU_SLIDER_BG_RIGHT",
        NULL};
    for (size_t index = 0; index < AB_UI_BOX_COMPONENT_COUNT; ++index)
        slider->box.components[index] =
            pieces[index] ? text(pieces[index]) : (AbUiPointerValue){0};
    slider->box.horizontal_anchor = text("HCENTER");
    slider->box.vertical_anchor = text("BOTTOM");
    slider->element_width = slider->element_height = slider->bottom_margin = 0;
    AbUiImageResourceBoundaries *resources = &context->images;
    AbVec2 corner, empty, about;
    if (!resources->bounds)
        return finish(slider, ab_fail(error, "Missing MainMenuSlider resource bounds"));
    if (!resources->bounds(resources->context, text("MENU_SLIDER_BG_TOP_LEFT").text,
                           &corner, error))
        return finish(slider, false);
    slider->border_width = corner.x * 0.5f;
    if (!resources->bounds(resources->context, text("BUTTON_EMPTY").text, &empty,
                           error) ||
        !resources->bounds(resources->context, text("BUTTON_ABOUT_SMALL").text, &about,
                           error))
        return finish(slider, false);
    float first = about.x * real(UINT32_C(0x3f933333));
    float second = empty.x * 0.5f;
    slider->unscaled_width = second < first ? second : first;
    slider->bottom_margin = empty.y * 0.5f;
    slider->element_height = about.y;
    return true;
}
