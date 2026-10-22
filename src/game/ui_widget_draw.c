#include "ab/ui_widget_draw.h"

#include <math.h>
#include <string.h>

static bool finite_number(const AbUiPointerValue *value, float *output) {
    if (value->kind != AB_UI_POINTER_NUMBER) {
        return false;
    }
    memcpy(output, &value->number_bits, sizeof(*output));
    return isfinite(*output);
}

bool ab_ui_frame_state_draw_view(const AbUiFrameState *state, AbUiDrawFrame *output) {
    if (!state || !output) {
        return false;
    }
    const AbUiFrameFields *fields = &state->fields;
    AbUiDrawFrame view;
    if (!finite_number(&fields->x, &view.position.x) ||
        !finite_number(&fields->y, &view.position.y) ||
        !finite_number(&fields->scale_x, &view.scale.x) ||
        !finite_number(&fields->scale_y, &view.scale.y) ||
        !finite_number(&fields->angle, &view.angle)) {
        return false;
    }
    *output = view;
    return true;
}

bool ab_ui_image_state_draw_view(const AbUiImageState *state, AbUiImageDraw *output) {
    if (!state || !output) {
        return false;
    }
    AbUiImageDraw view = {0};
    if (!ab_ui_frame_state_draw_view(&state->frame, &view.frame)) {
        return false;
    }
    view.visible = state->frame.fields.visible.kind == AB_UI_POINTER_BOOLEAN &&
                   state->frame.fields.visible.boolean;
    view.image_present = state->image.kind != AB_UI_POINTER_NONE;
    view.alpha_present = state->alpha.kind != AB_UI_POINTER_NONE;
    if (view.alpha_present && !finite_number(&state->alpha, &view.alpha)) {
        return false;
    }
    if (view.visible && view.image_present) {
        if (state->image.kind != AB_UI_POINTER_TEXT ||
            !ab_ui_pointer_value_valid(&state->image) ||
            !finite_number(&state->frame.fields.w, &view.size.x) ||
            !finite_number(&state->frame.fields.h, &view.size.y) ||
            !ab_ui_pointer_value_valid(&state->floor_coordinates)) {
            return false;
        }
        view.image = state->image.text;
        view.floor_coordinates = ab_ui_pointer_value_truthy(&state->floor_coordinates);
        const AbUiPointerValue *pivots[] = {&state->pivot_x, &state->pivot_y,
                                            &state->rotation_pivot_x,
                                            &state->rotation_pivot_y};
        float *numbers[] = {&view.pivot.x, &view.pivot.y, &view.rotation_pivot.x,
                            &view.rotation_pivot.y};
        for (size_t index = 0; index < 4; ++index) {
            if (!ab_ui_pointer_value_valid(pivots[index])) {
                return false;
            }
            if (ab_ui_pointer_value_truthy(pivots[index])) {
                if (!finite_number(pivots[index], numbers[index])) {
                    return false;
                }
                view.pivot_present |= 1u << index;
            }
        }
    }
    *output = view;
    return true;
}

static bool button_view(const AbUiImageButtonState *state, AbUiImageDraw *image,
                        AbUiImageButtonAnimation *animation, AbError *error) {
    if (!state || !ab_ui_image_state_draw_view(&state->image, image) ||
        !ab_ui_pointer_value_valid(&state->hover_enabled)) {
        return ab_fail(error, "Unsupported canonical ImageButton draw fields");
    }
    *animation = (AbUiImageButtonAnimation){
        .hover_enabled = ab_ui_pointer_value_truthy(&state->hover_enabled)};
    if (animation->hover_enabled &&
        !finite_number(&state->hover_scale, &animation->hover_scale)) {
        return ab_fail(error, "Unsupported canonical ImageButton hover scale");
    }
    return true;
}

bool ab_ui_image_button_state_draw_self(const AbUiImageButtonState *state,
                                        const AbUiDrawInput *input, AbVec2 screen_size,
                                        AbUiImageDrawSink sink, void *context,
                                        AbError *error) {
    AbUiImageDraw image;
    AbUiImageButtonAnimation animation;
    if (!button_view(state, &image, &animation, error))
        return false;
    AbUiImageButtonDraw button = {.image = &image, .animation = &animation};
    return ab_ui_image_button_draw_self(&button, input, screen_size, sink, context,
                                        error);
}

bool ab_ui_image_button_state_draw(const AbUiImageButtonState *state,
                                   const AbUiDrawChild *children, size_t child_count,
                                   const AbUiDrawInput *input, AbVec2 screen_size,
                                   AbUiImageDrawSink sink, void *context,
                                   AbError *error) {
    AbUiImageDraw image;
    AbUiImageButtonAnimation animation;
    if (!button_view(state, &image, &animation, error))
        return false;
    AbUiImageButtonDraw button = {&image, &animation, children, child_count};
    return ab_ui_image_button_draw(&button, input, screen_size, sink, context, error);
}
