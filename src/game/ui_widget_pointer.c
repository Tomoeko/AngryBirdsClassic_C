#include "ab/ui_widget_pointer.h"

#include "ui_image_button_internal.h"

static void assign_flag(void *context, AbUiButtonFlag flag, bool value) {
    AbUiImageButtonState *state = context;
    AbUiPointerValue *field = flag == AB_UI_BUTTON_CLICK_STARTED
                                  ? &state->click_started
                                  : &state->clear_pointer_flags;
    *field = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = value};
}

static bool dispatch(AbUiImageButtonState *state, uintptr_t object,
                     const AbUiImageButtonBoundaries *boundaries, AbSpan event,
                     AbVec2 point, bool traverse_children, AbUiPointerReply *output,
                     AbError *error) {
    if (!state || !ab_ui_pointer_value_valid(&state->image.frame.fields.visible) ||
        !ab_ui_pointer_value_valid(&state->image.image) ||
        !ab_ui_pointer_value_valid(&state->activate_on_release) ||
        !ab_ui_pointer_value_valid(&state->click_started) ||
        !ab_ui_pointer_value_valid(&state->clear_pointer_flags)) {
        return ab_fail(error, "Invalid canonical ImageButton pointer fields");
    }
    AbUiImageButton button = {
        .visible = ab_ui_pointer_value_truthy(&state->image.frame.fields.visible),
        .image_present = ab_ui_pointer_value_truthy(&state->image.image),
        .activate_on_release = ab_ui_pointer_value_truthy(&state->activate_on_release),
        .click_started = ab_ui_pointer_value_truthy(&state->click_started),
        .should_clear_pointer_flags =
            ab_ui_pointer_value_truthy(&state->clear_pointer_flags),
        .enabled = state->enabled,
        .return_value = state->return_value,
        .disabled_return_value = state->disabled_return_value,
        .meta = state->meta,
        .click_sound = state->click_sound,
        .object = object,
    };
    return ab_ui_image_button_dispatch(&button, boundaries, event, point,
                                       traverse_children, assign_flag, state, output,
                                       error);
}

bool ab_ui_image_button_state_handle(AbUiImageButtonState *state, uintptr_t object,
                                     const AbUiImageButtonBoundaries *boundaries,
                                     AbSpan event, AbVec2 point,
                                     AbUiPointerReply *output, AbError *error) {
    return dispatch(state, object, boundaries, event, point, false, output, error);
}

bool ab_ui_image_button_state_pointer(AbUiImageButtonState *state, uintptr_t object,
                                      const AbUiImageButtonBoundaries *boundaries,
                                      AbSpan event, AbVec2 point,
                                      AbUiPointerReply *output, AbError *error) {
    return dispatch(state, object, boundaries, event, point, true, output, error);
}

void ab_ui_image_button_state_clear_flags(AbUiImageButtonState *state) {
    if (state) {
        assign_flag(state, AB_UI_BUTTON_CLICK_STARTED, false);
        assign_flag(state, AB_UI_BUTTON_CLEAR_POINTER_FLAGS, false);
    }
}

void ab_ui_image_button_state_clear_events(AbUiImageButtonState *state) {
    if (state) {
        assign_flag(state, AB_UI_BUTTON_CLICK_STARTED, false);
    }
}
