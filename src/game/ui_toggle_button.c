#include "ab/ui_toggle_button.h"

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

static AbDataValue ordinal(size_t index) {
    AbDataValue value = {.kind = AB_DATA_NUMBER};
    float numeric = (float)index;
    memcpy(&value.bits, &numeric, sizeof(numeric));
    return value;
}

static bool valid(AbUiToggleButton *button, AbError *error) {
    return button && button->widget.initialized && button->widget.context &&
                   !button->widget.context->failed
               ? true
               : ab_fail(error, "Invalid or failed ToggleButton");
}

static bool finish(AbUiToggleButton *button, bool okay) {
    if (!okay)
        button->widget.context->failed = true;
    return okay;
}

static bool valid_states(const AbDataTable *states, AbError *error) {
    if (!states || states->count > states->capacity ||
        (states->count && !states->entries) || states->count >= UINT32_C(16777216))
        return ab_fail(error, "Invalid ToggleButton state array");
    for (size_t index = 1; index <= states->count; ++index) {
        const AbDataValue *image = ab_data_get(states, ordinal(index));
        if (!image || image->kind != AB_DATA_STRING ||
            (image->string.size && !image->string.data))
            return ab_fail(error, "ToggleButton requires dense string state images");
    }
    return true;
}

static AbUiPointerValue selected_image(const AbUiToggleButton *button) {
    if (button->state.kind != AB_UI_POINTER_NUMBER)
        return (AbUiPointerValue){0};
    const AbDataValue *image =
        ab_data_get(button->states, (AbDataValue){.kind = AB_DATA_NUMBER,
                                                  .bits = button->state.number_bits});
    return image ? (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = image->string}
                 : (AbUiPointerValue){0};
}

static bool bind_images(AbUiToggleButton *button, AbDataTable *states,
                        AbUiPointerValue disabled, AbError *error) {
    button->states = states;
    return valid_states(states, error) &&
           ab_ui_image_button_state_set_image(&button->widget.state,
                                              selected_image(button), disabled,
                                              &button->widget.context->images, error);
}

static bool constructor_image(void *context, AbUiPointerValue image, AbError *error) {
    AbUiToggleButton *button = context;
    if (image.kind != AB_UI_POINTER_OBJECT || !image.object)
        return ab_fail(error, "ToggleButton constructor image requires a state table");
    return bind_images(button, (AbDataTable *)image.object, (AbUiPointerValue){0},
                       error);
}

bool ab_ui_toggle_button_set_images(AbUiToggleButton *button, AbDataTable *states,
                                    AbUiPointerValue disabled_image, AbError *error) {
    if (!valid(button, error))
        return false;
    return finish(button, bind_images(button, states, disabled_image, error));
}

bool ab_ui_toggle_button_set_state(AbUiToggleButton *button, float state,
                                   AbError *error) {
    if (!valid(button, error))
        return false;
    button->state = number(state);
    return finish(button, valid_states(button->states, error) &&
                              ab_ui_image_button_state_set_image(
                                  &button->widget.state, selected_image(button),
                                  button->widget.state.disabled_image,
                                  &button->widget.context->images, error));
}

AbUiPointerValue ab_ui_toggle_button_get_state(const AbUiToggleButton *button) {
    return button ? button->state : (AbUiPointerValue){0};
}

bool ab_ui_toggle_button_get_state_image(AbUiToggleButton *button, size_t state,
                                         AbUiPointerValue *output, AbError *error) {
    if (!valid(button, error) || !output || !valid_states(button->states, error))
        return false;
    const AbDataValue *image = state <= button->states->count
                                   ? ab_data_get(button->states, ordinal(state))
                                   : NULL;
    *output =
        image ? (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = image->string}
              : (AbUiPointerValue){0};
    return true;
}

bool ab_ui_toggle_button_set_state_image(AbUiToggleButton *button, size_t state,
                                         AbSpan image, AbError *error) {
    if (!valid(button, error) || !valid_states(button->states, error) ||
        (image.size && !image.data))
        return false;
    if (state > button->states->count)
        return true;
    if (!state)
        return finish(button,
                      ab_fail(error, "ToggleButton cannot insert sparse state slots"));
    AbDataValue key = ordinal(state);
    for (size_t index = 0; index < button->states->count; ++index) {
        AbDataEntry *entry = button->states->entries + index;
        if (entry->key.kind != AB_DATA_NUMBER || entry->key.bits != key.bits)
            continue;
        entry->value = (AbDataValue){.kind = AB_DATA_STRING, .string = image};
        if (button->state.kind == AB_UI_POINTER_NUMBER &&
            real(button->state.number_bits) == (float)state)
            return finish(button, ab_ui_image_button_state_set_image(
                                      &button->widget.state,
                                      (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                                                         .text = image},
                                      button->widget.state.disabled_image,
                                      &button->widget.context->images, error));
        return true;
    }
    return finish(button, ab_fail(error, "Missing ToggleButton state slot"));
}

static bool equal(AbUiPointerValue first, AbUiPointerValue second) {
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

static bool pointer(void *context, AbUiWidget *widget, AbSpan event, AbVec2 point,
                    AbUiPointerReply *output, AbError *error) {
    AbUiToggleButton *button = context;
    AbUiPointerReply reply;
    if (!output || widget != &button->widget ||
        !ab_ui_widget_pointer_base(widget, event, point, &reply, error))
        return false;
    if (equal(reply.values[0], widget->state.return_value) &&
        reply.values[2].kind == AB_UI_POINTER_OBJECT &&
        reply.values[2].object == (uintptr_t)widget) {
        if (button->state.kind != AB_UI_POINTER_NUMBER)
            return ab_fail(error, "ToggleButton cycling requires numeric state");
        button->state = number(real(button->state.number_bits) + 1);
        if (!valid_states(button->states, error))
            return false;
        if ((float)button->states->count < real(button->state.number_bits))
            button->state = number(1);
        if (!ab_ui_image_button_state_set_image(&widget->state, selected_image(button),
                                                widget->state.disabled_image,
                                                &widget->context->images, error))
            return false;
    }
    *output = reply;
    return true;
}

bool ab_ui_toggle_button_init(AbUiToggleButton *button, AbUiWidgetContext *context,
                              AbUiWidgetChildren children,
                              const AbUiImageButtonArguments *arguments,
                              AbError *error) {
    if (!button || button->widget.initialized)
        return ab_fail(error, "ToggleButton requires fresh caller storage");
    if (!ab_ui_widget_init_derived(
            &button->widget, context, AB_UI_WIDGET_IMAGE_BUTTON, children, NULL, NULL,
            arguments, (AbUiImageInitSetter){button, constructor_image}, error))
        return false;
    button->state = number(1);
    button->states = &button->empty_states;
    button->widget.methods = (AbUiWidgetMethods){.context = button, .pointer = pointer};
    return true;
}
