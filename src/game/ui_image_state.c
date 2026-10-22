#include "ab/ui_image_state.h"

#include <math.h>
#include <string.h>

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static AbUiPointerValue text(const char *bytes, size_t length) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                              .text = {(const uint8_t *)bytes, length}};
}

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

bool ab_ui_image_state_refresh(AbUiImageState *state,
                               const AbUiImageResourceBoundaries *boundaries,
                               AbError *error) {
    if (!state || !boundaries || !ab_ui_pointer_value_valid(&state->image)) {
        return ab_fail(error, "Invalid typed UI image refresh input");
    }
    if (!ab_ui_pointer_value_truthy(&state->image)) {
        return true;
    }
    if (state->image.kind != AB_UI_POINTER_TEXT || !boundaries->bounds ||
        !boundaries->pivot) {
        return ab_fail(error, "Missing typed UI image resource boundary");
    }
    AbVec2 size, pivot;
    if (!boundaries->bounds(boundaries->context, state->image.text, &size, error)) {
        return false;
    }
    if (!finite_point(size)) {
        return ab_fail(error, "Nonfinite typed UI image bounds");
    }
    state->frame.fields.w = number(size.x);
    state->frame.fields.h = number(size.y);
    if (!boundaries->pivot(boundaries->context, state->image.text, &pivot, error)) {
        return false;
    }
    if (!finite_point(pivot)) {
        return ab_fail(error, "Nonfinite typed UI image pivot");
    }
    state->resource_pivot_x = state->rotation_pivot_x = number(pivot.x);
    state->resource_pivot_y = state->rotation_pivot_y = number(pivot.y);
    return true;
}

bool ab_ui_image_state_set_image(AbUiImageState *state, AbUiPointerValue image,
                                 const AbUiImageResourceBoundaries *boundaries,
                                 AbError *error) {
    if (!state || !boundaries || !ab_ui_pointer_value_valid(&image) ||
        (image.kind != AB_UI_POINTER_NONE && image.kind != AB_UI_POINTER_TEXT)) {
        return ab_fail(error, "Invalid typed UI image identifier");
    }
    if (image.kind == AB_UI_POINTER_TEXT) {
        if (!boundaries->localize) {
            return ab_fail(error, "Missing typed UI image localization boundary");
        }
        AbSpan localized;
        if (!boundaries->localize(boundaries->context, image.text, &localized, error)) {
            return false;
        }
        if (localized.size && !localized.data) {
            return ab_fail(error, "Invalid typed UI localized image");
        }
        state->image = localized.size ? (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                                                           .text = localized}
                                      : image;
    } else {
        state->image = image;
    }
    return ab_ui_image_state_refresh(state, boundaries, error);
}

bool ab_ui_image_button_state_enable(AbUiImageButtonState *state,
                                     AbUiPointerValue enabled,
                                     const AbUiImageResourceBoundaries *boundaries,
                                     AbError *error) {
    if (!state || !boundaries || !ab_ui_pointer_value_valid(&enabled) ||
        !ab_ui_pointer_value_valid(&state->disabled_image)) {
        return ab_fail(error, "Invalid typed UI button enabled input");
    }
    state->enabled = enabled;
    if (ab_ui_pointer_value_truthy(&enabled)) {
        return ab_ui_image_state_set_image(&state->image, state->enabled_image,
                                           boundaries, error);
    }
    return !ab_ui_pointer_value_truthy(&state->disabled_image) ||
           ab_ui_image_state_set_image(&state->image, state->disabled_image, boundaries,
                                       error);
}

bool ab_ui_image_button_state_set_image(AbUiImageButtonState *state,
                                        AbUiPointerValue enabled_image,
                                        AbUiPointerValue disabled_image,
                                        const AbUiImageResourceBoundaries *boundaries,
                                        AbError *error) {
    if (!state || !ab_ui_pointer_value_valid(&disabled_image)) {
        return ab_fail(error, "Invalid typed UI button image input");
    }
    if (!ab_ui_image_state_set_image(&state->image, enabled_image, boundaries, error)) {
        return false;
    }
    state->enabled_image = enabled_image;
    state->disabled_image = disabled_image;
    return ab_ui_image_button_state_enable(state, state->enabled, boundaries, error);
}

static bool init_image(AbUiImageState *state, AbUiImageButtonState *button,
                       const AbUiImageArguments *arguments,
                       const AbUiImageInitBoundaries *boundaries,
                       AbUiImageInitSetter setter, AbError *error) {
    if (!state || !boundaries ||
        (arguments && !ab_ui_pointer_value_valid(&arguments->image))) {
        return ab_fail(error, "Invalid typed UI image constructor input");
    }
    if (!ab_ui_frame_init(&state->frame, arguments ? &arguments->frame : NULL,
                          &boundaries->frame, error)) {
        return false;
    }
    state->frame.fields.angle = state->rotation_pivot_x = state->rotation_pivot_y =
        number(0);
    state->alpha = (AbUiPointerValue){0};
    state->floor_coordinates =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    if (!arguments || !ab_ui_pointer_value_truthy(&arguments->image)) {
        return true;
    }
    if (setter.set_image)
        return setter.set_image(setter.context, arguments->image, error);
    return button ? ab_ui_image_button_state_set_image(button, arguments->image,
                                                       (AbUiPointerValue){0},
                                                       &boundaries->resources, error)
                  : ab_ui_image_state_set_image(state, arguments->image,
                                                &boundaries->resources, error);
}

bool ab_ui_image_state_init(AbUiImageState *state, const AbUiImageArguments *arguments,
                            const AbUiImageInitBoundaries *boundaries, AbError *error) {
    return init_image(state, NULL, arguments, boundaries, (AbUiImageInitSetter){0},
                      error);
}

bool ab_ui_image_state_init_derived(AbUiImageState *state,
                                    const AbUiImageArguments *arguments,
                                    const AbUiImageInitBoundaries *boundaries,
                                    AbUiImageInitSetter setter, AbError *error) {
    return init_image(state, NULL, arguments, boundaries, setter, error);
}

bool ab_ui_image_button_state_init_derived(AbUiImageButtonState *state,
                                           const AbUiImageButtonArguments *arguments,
                                           const AbUiImageInitBoundaries *boundaries,
                                           AbUiImageInitSetter setter, AbError *error) {
    const AbUiImageButtonArguments empty = {0};
    const AbUiImageButtonArguments *input = arguments ? arguments : &empty;
    if (!state ||
        !init_image(&state->image, state, arguments ? &arguments->image : NULL,
                    boundaries, setter, error)) {
        return false;
    }
    state->enabled = state->hover_enabled =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    state->hover_scale = state->hover_count = number(0);
    state->hover_state = text("NONE", 4);
    if (!ab_ui_pointer_value_valid(&input->scale_on_touch) ||
        !ab_ui_pointer_value_valid(&input->padding) ||
        !ab_ui_pointer_value_valid(&input->click_sound) ||
        !ab_ui_pointer_value_valid(&input->activate_on_release) ||
        !ab_ui_pointer_value_valid(&input->return_value) ||
        !ab_ui_pointer_value_valid(&input->disabled_return_value) ||
        !ab_ui_pointer_value_valid(&input->enabled_image) ||
        !ab_ui_pointer_value_valid(&input->disabled_image) ||
        !ab_ui_pointer_value_valid(&input->meta)) {
        return ab_fail(error, "Invalid typed UI button constructor fields");
    }
    const AbUiPointerValue yes = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    state->scale_on_touch =
        input->scale_on_touch.kind == AB_UI_POINTER_NONE ? yes : input->scale_on_touch;
    if (!ab_ui_pointer_value_truthy(&input->padding) &&
        (boundaries->empty_padding.kind != AB_UI_POINTER_OBJECT ||
         !ab_ui_pointer_value_valid(&boundaries->empty_padding))) {
        return ab_fail(error, "Missing prepared empty UI button padding object");
    }
    state->padding = ab_ui_pointer_value_truthy(&input->padding)
                         ? input->padding
                         : boundaries->empty_padding;
    state->click_sound = ab_ui_pointer_value_truthy(&input->click_sound)
                             ? input->click_sound
                             : text("menu_confirm", 12);
    state->activate_on_release = input->activate_on_release.kind == AB_UI_POINTER_NONE
                                     ? yes
                                     : input->activate_on_release;
    state->return_value = input->return_value;
    state->disabled_return_value = input->disabled_return_value;
    state->enabled_image = input->enabled_image;
    state->disabled_image = input->disabled_image;
    state->meta = input->meta;
    return true;
}

bool ab_ui_image_button_state_init(AbUiImageButtonState *state,
                                   const AbUiImageButtonArguments *arguments,
                                   const AbUiImageInitBoundaries *boundaries,
                                   AbError *error) {
    return ab_ui_image_button_state_init_derived(state, arguments, boundaries,
                                                 (AbUiImageInitSetter){0}, error);
}

bool ab_ui_image_button_state_enter(AbUiImageButtonState *state, AbUiFrameNode *node,
                                    AbUiFrameLifecycle *lifecycle, AbError *error) {
    if (!state || !node || node->state != &state->image.frame) {
        return ab_fail(error, "Invalid typed UI button entry frame");
    }
    state->click_started = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    state->hover_scale = number(0);
    state->hover_state = text("NONE", 4);
    return ab_ui_frame_enter(node, lifecycle, error);
}
